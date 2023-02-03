#include "stdafx.h"
#include "looprec.h"
#include "logger.h"

//----------------------------------------------------------------------------
///
//----------------------------------------------------------------------------
static const std::string CONTINUOUS = "=";

//----------------------------------------------------------------------------
/// @class Segment
//----------------------------------------------------------------------------
class Segment : private boost::noncopyable
{
protected:
    const std::string app_;
    const std::string name_;
    boost::filesystem::path dat_path_;
    boost::filesystem::path idx_path_;
    bool continuous_;
    bool expired_;
public:
    typedef boost::shared_ptr<Segment> ptr_t;
    typedef std::map<boost::posix_time::ptime, ptr_t> map_t;
    Segment(const std::string& app, const std::string& name, const boost::filesystem::path& path, const std::string& idx_ext)
        : app_(app), name_(name), dat_path_(path), idx_path_(boost::filesystem::change_extension(path, idx_ext)), continuous_(false), expired_(false) {
        continuous_ = boost::algorithm::ends_with(path.stem().string(), CONTINUOUS);
    }
    virtual ~Segment() {
        Destroy();
    }
    virtual bool Initialize() {
        return true;
    }
    virtual void Destroy() {
        if (!expired_) return;
        if (!dat_path_.empty()) {
            boost::system::error_code ec;
            if (boost::filesystem::remove(dat_path_, ec)) {
                Logger::Info(boost::format("<%s> loopRec [ %s ] remove segment data [%s]") % app_ % name_ % dat_path_.filename().string());
                dat_path_.clear();
            } else {
                Logger::Warning(boost::format("<%s> loopRec [ %s ] failed to remove segment data [%s] : %s") % app_ % name_ % dat_path_.filename().string() % ec.to_string());
            }
        }
        if (!idx_path_.empty()) {
            boost::system::error_code ec;
            if (boost::filesystem::remove(idx_path_, ec)) {
                Logger::Debug(boost::format("<%s> loopRec [ %s ] remove segment index [%s]") % app_ % name_ % idx_path_.filename().string());
                idx_path_.clear();
            } else {
                Logger::Warning(boost::format("<%s> loopRec [ %s ] failed to remove segment index [%s] : %s") % app_ % name_ % idx_path_.filename().string() % ec.to_string());
            }
        }
    }
    virtual bool Continuous() const {
        return continuous_;
    }
    virtual void SetExpired(bool expired) {
        expired_ = expired;
    }
};

//----------------------------------------------------------------------------
/// @class SegmentWriter
//----------------------------------------------------------------------------
class SegmentWriter : public Segment
{
    std::ofstream dat_file_;
    std::ofstream idx_file_;
    std::chrono::milliseconds idx_interval_;
    std::chrono::steady_clock::time_point idx_time_;
public:
    typedef boost::shared_ptr<SegmentWriter> ptr_t;
    SegmentWriter(const std::string& app, const std::string& name, const boost::filesystem::path& path,
        const std::string& idx_ext, const std::chrono::milliseconds& idx_interval, const std::chrono::steady_clock::time_point& idx_time)
        : Segment(app, name, path, idx_ext), dat_file_(), idx_file_(), idx_interval_(idx_interval), idx_time_(idx_time) {
    }
    virtual ~SegmentWriter() {
        Destroy();
    }
    virtual bool Initialize() override {
        if (!__super::Initialize()) {
            return false;
        }
        dat_file_.open(dat_path_.string(), std::ios::out | std::ios::trunc | std::ios::binary);
        if (dat_file_.is_open()) {
            Logger::Info(boost::format("<%s> loopRec [ %s ] create segment data [%s]") % app_ % name_ % dat_path_.filename().string());
        }
        idx_file_.open(idx_path_.string(), std::ios::out | std::ios::trunc | std::ios::binary);
        if (idx_file_.is_open()) {
            Logger::Debug(boost::format("<%s> loopRec [ %s ] create segment index [%s]") % app_ % name_ % idx_path_.filename().string());
        }
        return WriteIndex();
    }
    virtual void Destroy() override {
        Close();
        __super::Destroy();
    }
    virtual bool Write(const std::chrono::steady_clock::time_point& now, const Event::buf_t& buf) {
        if (!dat_file_.is_open()) return false;
        dat_file_.write(&buf.at(0), buf.size());
        bool flush = false;
        while (now >= idx_time_) {
            if (!WriteIndex()) return false;
            flush = true;
        }
        if (flush) Flush();
        return true;
    }
    virtual void Close() {
        if (dat_file_.is_open()) dat_file_.close();
        if (idx_file_.is_open()) idx_file_.close();
    }
    virtual void Flush() {
        if (dat_file_.is_open()) dat_file_.flush();
        if (idx_file_.is_open()) idx_file_.flush();
    }
protected:
    virtual bool WriteIndex() {
        if (!dat_file_.is_open() || !idx_file_.is_open()) return false;
        std::streamoff pos = dat_file_.tellp();
        idx_file_.write(reinterpret_cast<const char*>(&pos), sizeof(std::streamoff));
        idx_time_ += idx_interval_;
#if defined(_DEBUG) && 0
        std::cout << "write idx:" << pos << std::endl;
        idx_file_.flush();
        std::ifstream xx;
        xx.open(idx_.string(), std::ios::in | std::ios::binary);
        if (!xx.is_open()) {
            std::cout << "couldn't open " << idx_path_ << std::endl;
            return true;
        } else {
            std::streamoff pos = xx.tellg();
            std::cout << "open " << idx_path_ << " " << pos << std::endl;
        }
        for (int i = 0; !xx.eof(); ++i) {
            std::streamoff pos = 0;
            xx.read(reinterpret_cast<char*>(&pos), sizeof(std::streamoff));
            if (xx.gcount() != sizeof(std::streamoff)) break;
            std::cout << "read idx:" << i << " " << pos << std::endl;
        }
#endif
        return true;
    }
};

//----------------------------------------------------------------------------
/// @class LoopRec::Impl
//----------------------------------------------------------------------------
class LoopRec::Impl
{
    LoopRec* owner_;
    const Json conf_;
    const std::string app_;
    const std::string name_;
    Segment::map_t segments_;
    SegmentWriter::ptr_t segmentWriter_;
    boost::filesystem::path dir_;
    std::string dat_ext_;
    std::string idx_ext_;
    std::chrono::seconds segment_duration_;
    std::chrono::seconds total_duration_;
    std::chrono::milliseconds idx_interval_;
    std::chrono::steady_clock::time_point segment_time_;
public:
    Impl(LoopRec* owner, const Json& conf, const std::string& app, const std::string& name)
        : owner_(owner), conf_(conf), app_(app), name_(name), segments_(), segmentWriter_(), dir_(), dat_ext_(".dat"), idx_ext_(".idx")
        , segment_duration_(600), total_duration_(3600), idx_interval_(100), segment_time_() {
    }
    virtual ~Impl() {
        Destroy();
    }
    virtual bool Initialize() {
        try {
            dir_ = conf_["dir"].to<boost::filesystem::path>();
            dat_ext_ = "." + boost::trim_left_copy_if(conf_["data_extension"].to<std::string>("dat"), boost::is_any_of("."));
            idx_ext_ = "." + boost::trim_left_copy_if(conf_["index_extension"].to<std::string>("idx"), boost::is_any_of("."));
            if (dat_ext_ == idx_ext_) idx_ext_ += "_idx";
            uint32_t segdur = std::max<uint32_t>(conf_["segment_duration"].to<uint32_t>(600), 10);
            segment_duration_ = std::chrono::seconds(segdur);
            total_duration_ = std::chrono::seconds(std::max<uint32_t>(conf_["total_duration"].to<uint32_t>(3600), segdur));
            idx_interval_ = std::chrono::milliseconds(std::max<uint32_t>(conf_["index_interval"].to<uint32_t>(100), 1));
            if (dir_.empty()) dir_ = "./" + name_;
            boost::filesystem::create_directories(dir_);
            for (boost::filesystem::directory_iterator it(dir_), end; it != end; ++it) {
                const boost::filesystem::path path(*it);
                std::string ext = boost::filesystem::extension(path);
                if (ext != dat_ext_) continue;
                try {
                    std::string fname = path.filename().string();
                    boost::posix_time::ptime utc = boost::posix_time::from_iso_string(fname);
                    if (utc.is_special()) continue;
                    Segment::ptr_t segment(new Segment(app_, name_, path, idx_ext_));
                    if (segment->Initialize()) {
                        segments_[utc] = segment;
                    }
                } catch (boost::bad_lexical_cast) {
                    continue;
                }
            }
            RemoveExpiredSegments(boost::posix_time::microsec_clock::universal_time());
        } catch (boost::filesystem::filesystem_error& ex) {
            Logger::Warning(boost::format("<%s> an error occured while initializing loopRec [ %s ] : %s") % app_ % name_ % ex.what());
            return false;
        } catch (std::exception& ex) {
            Logger::Warning(boost::format("<%s> an error occured while initializing loopRec [ %s ] : %s") % app_ % name_ % ex.what());
            return false;
        }
        return true;
    }
    virtual void Destroy() {
        segmentWriter_.reset();
        segments_.clear();
    }
    virtual bool OnReceive(const ReceiveOption& option, const Event::buf_t& buf, bool discrete) {
        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
        std::string suffix = "Z"; // UTC
        if (segmentWriter_ && now >= segment_time_) {
            segmentWriter_->Close();
            segmentWriter_.reset();
            suffix += CONTINUOUS; // '=' means continuous data from previous segment
        }
        if (!segmentWriter_) {
            boost::posix_time::ptime utc = boost::posix_time::microsec_clock::universal_time();
            RemoveExpiredSegments(utc);
            boost::filesystem::path path = dir_ / (boost::posix_time::to_iso_string(utc) + suffix + dat_ext_);
            SegmentWriter::ptr_t segmentWriter(new SegmentWriter(app_, name_, path, idx_ext_, idx_interval_, now));
            if (segmentWriter->Initialize()) {
                segments_[utc] = segmentWriter_ = segmentWriter;
                segment_time_ = now + segment_duration_;
            }
        }
        if (segmentWriter_) {
            segmentWriter_->Write(now, buf);
        }
        return true;
    }
    virtual bool OnDisconnected(const ReceiveOption& option) {
        if (segmentWriter_) {
            segmentWriter_->Close();
            segmentWriter_.reset();
        }
        return true;
    }
protected:
    virtual void RemoveExpiredSegments(const boost::posix_time::ptime& utc) {
        boost::posix_time::seconds dur((total_duration_ + segment_duration_).count());
        while (!segments_.empty()) {
            Segment::map_t::iterator it = segments_.begin();
            if (it->first + dur > utc) break;
            it->second->SetExpired(true);
            segments_.erase(it);
        }
    }
};

LoopRec::map_t LoopRec::Create(const Json::Node& loopRecs, const std::string& app) {
    map_t map;
    for (size_t i = 0, c = loopRecs.size(); i < c; ++i) {
        const Json conf = loopRecs[i];
        std::string name = conf["name"].to<std::string>("");
        if (name.empty()) continue;
        ptr_t loopRec(new LoopRec(conf, app, name));
        if (loopRec->Initialize()) map[name] = loopRec;
    }
    return map;
}
LoopRec::LoopRec(const Json& conf, const std::string& app, const std::string& name)
    : pimpl_(new Impl(this, conf, app, name)) {
}
LoopRec::~LoopRec() {
    pimpl_.reset();
}
bool LoopRec::Initialize() {
    return pimpl_->Initialize();
}
void LoopRec::Destroy() {
    return pimpl_->Destroy();
}
bool LoopRec::OnReceive(const ReceiveOption& option, const Event::buf_t& buf, bool discrete) {
    return pimpl_->OnReceive(option, buf, discrete);
}
bool LoopRec::OnDisconnected(const ReceiveOption& option) {
    return pimpl_->OnDisconnected(option);
}
