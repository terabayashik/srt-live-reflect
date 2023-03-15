#include "stdafx.h"
#include "looprec.h"
#include "logger.h"
#include "sender.h"

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
    const std::string log_prefix_;
    boost::filesystem::path dat_path_;
    boost::filesystem::path idx_path_;
    bool continuous_;
    bool expired_;
public:
    typedef boost::shared_ptr<Segment> ptr_t;
    typedef std::map<boost::posix_time::ptime, ptr_t> map_t;
    Segment(const std::string& log_prefix, const boost::filesystem::path& path, const std::string& idx_ext)
        : log_prefix_(log_prefix), dat_path_(path), idx_path_(boost::filesystem::change_extension(path, idx_ext)), continuous_(false), expired_(false) {
        continuous_ = boost::algorithm::ends_with(path.stem().string(), CONTINUOUS);
    }
    virtual ~Segment() {
        Destroy();
    }
    virtual bool Initialize() {
        return true;
    }
    virtual void Destroy() {
        if (!expired_) {
            return;
        }
        if (!dat_path_.empty()) {
            boost::system::error_code ec;
            if (boost::filesystem::remove(dat_path_, ec)) {
                Logger::Info(boost::format("%s : remove segment [%s]") % log_prefix_ % DatPath().filename().string());
                dat_path_.clear();
            } else {
                Logger::Warning(boost::format("%s : failed to remove segment [%s] : %s") % log_prefix_ % DatPath().filename().string() % ec.to_string());
            }
        }
        if (!idx_path_.empty()) {
            boost::system::error_code ec;
            if (boost::filesystem::remove(idx_path_, ec)) {
                Logger::Debug(boost::format("%s : remove segment index [%s]") % log_prefix_ % IdxPath().filename().string());
                idx_path_.clear();
            } else {
                Logger::Warning(boost::format("%s : failed to remove segment index [%s] : %s") % log_prefix_ % IdxPath().filename().string() % ec.to_string());
            }
        }
    }
    virtual const boost::filesystem::path& DatPath() const {
        return dat_path_;
    }
    virtual const boost::filesystem::path& IdxPath() const {
        return idx_path_;
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
class SegmentWriter
{
    const std::string log_prefix_;
    const Segment::ptr_t segment_;
    std::ofstream dat_file_;
    std::ofstream idx_file_;
    std::chrono::milliseconds idx_interval_;
    std::chrono::steady_clock::time_point idx_time_;
public:
    typedef boost::shared_ptr<SegmentWriter> ptr_t;
    SegmentWriter(const std::string& log_prefix, Segment::ptr_t segment, const std::chrono::milliseconds& idx_interval, const std::chrono::steady_clock::time_point& idx_time)
        : log_prefix_(log_prefix), segment_(segment), dat_file_(), idx_file_(), idx_interval_(idx_interval), idx_time_(idx_time) {
    }
    virtual ~SegmentWriter() {
        Destroy();
    }
    virtual bool Initialize() {
        if (!segment_) {
            return false;
        }
        dat_file_.open(segment_->DatPath().string(), std::ios::out | std::ios::trunc | std::ios::binary);
        if (dat_file_.is_open()) {
            Logger::Info(boost::format("%s : create segment [%s]") % log_prefix_ % segment_->DatPath().filename().string());
        }
        idx_file_.open(segment_->IdxPath().string(), std::ios::out | std::ios::trunc | std::ios::binary);
        if (idx_file_.is_open()) {
            Logger::Debug(boost::format("%s : create segment index [%s]") % log_prefix_ % segment_->IdxPath().filename().string());
        }
        return WriteIndex();
    }
    virtual void Destroy() {
        Close();
    }
    virtual bool Write(const std::chrono::steady_clock::time_point& tick, const Event::buf_t& buf) {
        if (!dat_file_.is_open()) return false;
        dat_file_.write(&buf.at(0), buf.size());
        bool flush = false;
        while (tick >= idx_time_) {
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
        return true;
    }
};

//----------------------------------------------------------------------------
/// @class SegmentReader
//----------------------------------------------------------------------------
class SegmentReader
{
    const std::string log_prefix_;
    const boost::posix_time::ptime utc_;
    const Segment::ptr_t segment_;
    std::ifstream dat_file_;
    std::ifstream idx_file_;
    std::chrono::milliseconds idx_interval_;
    std::chrono::steady_clock::time_point base_time_;
    std::streamoff pos_;
    std::streamoff next_;
    int64_t read_;
    int64_t offset_ns_;
public:
    typedef boost::scoped_ptr<SegmentReader> ptr_t;
    SegmentReader(const std::string& log_prefix, const boost::posix_time::ptime& utc, Segment::ptr_t segment,
        const std::chrono::milliseconds& idx_interval, const std::chrono::steady_clock::time_point base_time)
        : log_prefix_(log_prefix), utc_(utc), segment_(segment), dat_file_(), idx_file_()
        , idx_interval_(idx_interval), base_time_(base_time), pos_(0), next_(0), read_(0), offset_ns_(0) {
    }
    virtual ~SegmentReader() {
        Destroy();
    }
    virtual bool Initialize(int64_t offset_ms) {
        if (!segment_) {
            return false;
        }
        dat_file_.open(segment_->DatPath().string(), std::ios::in | std::ios::binary);
        if (!dat_file_.is_open()) {
            Logger::Warning(boost::format("%s : failed to open segment [%s]") % log_prefix_ % segment_->DatPath().filename().string());
            return false;
        }
        idx_file_.open(segment_->IdxPath().string(), std::ios::in | std::ios::binary);
        if (!idx_file_.is_open()) {
            Logger::Warning(boost::format("%s : failed to open segment index [%s]") % log_prefix_ % segment_->IdxPath().filename().string());
            return false;
        }
        int64_t offset = offset_ms / idx_interval_.count();
        idx_file_.seekg(offset * sizeof(std::streamoff));
        if (idx_file_.read(reinterpret_cast<char*>(&pos_), sizeof(std::streamoff)).gcount() < sizeof(std::streamoff)) {
            Logger::Warning(boost::format("%s : failed to read segment index (pos) [%s]") % log_prefix_ % segment_->IdxPath().filename().string());
            return false;
        }
        if (idx_file_.read(reinterpret_cast<char*>(&next_), sizeof(std::streamoff)).gcount() < sizeof(std::streamoff)) {
            Logger::Warning(boost::format("%s : failed to read segment index (next) [%s]") % log_prefix_ % segment_->IdxPath().filename().string());
            return false;
        }
        Logger::Debug(boost::format("%s : open segment [%s]") % log_prefix_ % segment_->DatPath().filename().string());
        dat_file_.seekg(pos_);
        read_ = 0;
        offset_ns_ = offset * 1000ll * 1000 * idx_interval_.count(); // millisec to nanosec
        return true;
    }
    virtual void Destroy() {
        bool opened = dat_file_.is_open();
        dat_file_.close();
        idx_file_.close();
        if (opened && segment_) {
            Logger::Debug(boost::format("%s : close segment [%s]") % log_prefix_ % segment_->DatPath().filename().string());
        }
    }
    virtual bool Read(Event::buf_t& buf) {
        std::chrono::steady_clock::time_point tick = std::chrono::steady_clock::now();
        int64_t elapsed_ns = (tick - base_time_).count();
        if (elapsed_ns < 0) {
            //std::cout << "read wait: " << (-elapsed_ns / 1000 / 1000) << "[ms]" << std::endl;
            boost::this_thread::sleep_for(boost::chrono::nanoseconds(-elapsed_ns));
            return Read(buf);
        }
        buf.resize(dat_file_.read(&buf.at(0), buf.size()).gcount());
        if (buf.empty()) {
            return false;
        }
        if (next_ > pos_) {
            int64_t reference_ns = offset_ns_  + 1000ll * 1000 * idx_interval_.count() * read_ / (next_ - pos_);
            if (reference_ns > elapsed_ns) {
                if (reference_ns - elapsed_ns > 1000ll * 1000 * 100) {
                    Logger::Warning(boost::format("%s : too long wait : %lld[ms]") % log_prefix_ % ((reference_ns - elapsed_ns) / 1000 / 1000));
                }
                boost::this_thread::sleep_for(boost::chrono::nanoseconds(reference_ns - elapsed_ns));
            } else if (elapsed_ns - reference_ns > 1000ll * 1000 * 300) {
                Logger::Warning(boost::format("%s : late to send : %lld[ms]") % log_prefix_ % ((elapsed_ns - reference_ns) / 1000 / 1000));
            }
        }
        read_ += buf.size();
        while (pos_ + read_ >= next_) {
            std::streamoff next = 0;
            if (idx_file_.read(reinterpret_cast<char*>(&next), sizeof(std::streamoff)).gcount() < sizeof(std::streamoff)) {
                break;
            }
            if (next_ > pos_) {
                read_ -= next_ - pos_;
            }
            offset_ns_ += 1000ll * 1000 * idx_interval_.count();
            pos_ = next_;
            next_ = next;
        }
        return true;
    }
    const std::chrono::steady_clock::time_point& BaseTime() const {
        return base_time_;
    }
    const boost::posix_time::ptime& Utc() const {
        return utc_;
    }
};

//----------------------------------------------------------------------------
/// @class LoopRec::Impl
//----------------------------------------------------------------------------
class LoopRec::Impl
{
    //----------------------------------------------------------------------------
    /// @class LoopRec::Impl::SenderRunner
    //----------------------------------------------------------------------------
    class SenderRunner : public boost::enable_shared_from_this<SenderRunner>, private boost::noncopyable {
        LoopRec::Impl* pimpl_;
        Sender::ptr_t sender_;
        const StreamOption option_;
        boost::thread thread_;
    public:
        typedef boost::shared_ptr<SenderRunner> ptr_t;
        typedef std::vector<ptr_t> vector_t;
        SenderRunner(LoopRec::Impl* pimpl, int sfd, const SendOption& sendOption, const StreamOption& streamOption)
            : pimpl_(pimpl), sender_(Sender::Create(sfd, sendOption)), option_(streamOption), thread_() {
        }
        virtual ~SenderRunner() {
            Destroy();
        }
        virtual bool Initialize() {
            thread_ = boost::thread(&SenderRunner::Thread, this);
            return true;
        }
        virtual void Destroy() {
            sender_.reset();
            if (thread_.joinable()) {
                thread_.join();
            }
        }
    protected:
        virtual void Thread() {
            pimpl_->Send(sender_, option_);
            boost::thread thread([](LoopRec::Impl* pimpl, ptr_t sender_runner) {
                pimpl->RemoveSender(sender_runner);
            }, pimpl_, shared_from_this());
        }
    };
    LoopRec* owner_;
    const Json conf_;
    const std::string app_;
    const std::string name_;
    const std::string log_prefix_;
    Segment::map_t segments_;
    SegmentWriter::ptr_t writer_;
    boost::filesystem::path dir_;
    std::string dat_ext_;
    std::string idx_ext_;
    std::chrono::seconds segment_duration_;
    std::chrono::seconds total_duration_;
    std::chrono::milliseconds idx_interval_;
    std::chrono::steady_clock::time_point segment_time_;
    mutable boost::mutex mutex_;
    SenderRunner::vector_t sender_runners_;
public:
    Impl(LoopRec* owner, const Json& conf, const std::string& app, const std::string& name)
        : owner_(owner), conf_(conf), app_(app), name_(name), log_prefix_((boost::format("<%s> loopRec [ %s ]") % app % name).str())
        , segments_(), writer_(), dir_(), dat_ext_(".dat"), idx_ext_(".idx")
        , segment_duration_(600), total_duration_(3600), idx_interval_(100), segment_time_(), mutex_(), sender_runners_() {
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
                    Segment::ptr_t segment(new Segment(log_prefix_, path, idx_ext_));
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
        sender_runners_.clear();
        writer_.reset();
        segments_.clear();
    }
    virtual bool IsAcceptable(const StreamOption& streamOption) const {
        const boost::posix_time::ptime now = boost::posix_time::microsec_clock::universal_time();
        const boost::posix_time::ptime at = GetStartedAt(streamOption.Get<std::string>("at"), now);
        if (at.is_special()) return false;
        if (at + boost::posix_time::seconds(total_duration_.count()) < now || now < at) return false;
        return true;
    }
    virtual void CreateSender(int sfd, const SendOption& sendOption, const StreamOption& streamOption) {
        SenderRunner::ptr_t sender_runner(new SenderRunner(this, sfd, sendOption, streamOption));
        boost::mutex::scoped_lock lock(mutex_);
        sender_runners_.push_back(sender_runner);
        lock.unlock();
        sender_runner->Initialize();
    }
    virtual bool OnReceive(const ReceiveOption& option, const Event::buf_t& buf, bool discrete) {
        std::chrono::steady_clock::time_point tick = std::chrono::steady_clock::now();
        std::string suffix = "Z"; // UTC
        if (writer_ && tick >= segment_time_) {
            writer_->Close();
            writer_.reset();
            suffix += CONTINUOUS; // '=' means continuous data from previous segment
        }
        if (!writer_) {
            boost::posix_time::ptime utc = boost::posix_time::microsec_clock::universal_time();
            RemoveExpiredSegments(utc);
            boost::filesystem::path path = dir_ / (boost::posix_time::to_iso_string(utc) + suffix + dat_ext_);
            Segment::ptr_t segment(new Segment(log_prefix_, path, idx_ext_));
            SegmentWriter::ptr_t writer(new SegmentWriter(log_prefix_, segment, idx_interval_, tick));
            if (segment->Initialize() && writer->Initialize()) {
                boost::mutex::scoped_lock lock(mutex_);
                segments_[utc] = segment;
                writer_ = writer;
                if (suffix.length() == 2) { // CONTINUOUS
                    segment_time_ += segment_duration_;
                } else {
                    segment_time_ = tick + segment_duration_;
                }
            }
        }
        if (writer_) {
            writer_->Write(tick, buf);
        }
        return true;
    }
    virtual bool OnDisconnected(const ReceiveOption& option) {
        if (writer_) {
            writer_->Close();
            writer_.reset();
        }
        return true;
    }
protected:
    typedef std::pair<boost::posix_time::ptime, Segment::ptr_t> time_segment_t;
    virtual void RemoveExpiredSegments(const boost::posix_time::ptime& utc) {
        boost::posix_time::seconds dur((total_duration_ + segment_duration_).count());
        boost::mutex::scoped_lock lock(mutex_);
        Segment::map_t::iterator it = segments_.begin();
        for (; it != segments_.end(); ++it) {
            if (it->first + dur > utc) break;
            it->second->SetExpired(true);
        }
        segments_.erase(segments_.begin(), it);
    }
    virtual void RemoveSender(SenderRunner::ptr_t sender_runner) {
        boost::mutex::scoped_lock lock(mutex_);
        boost::range::remove_erase(sender_runners_, sender_runner);
    }
    virtual time_segment_t GetSegment(const boost::posix_time::ptime& utc, bool next = false) {
        boost::mutex::scoped_lock lock(mutex_);
        if (segments_.empty()) {
            return std::make_pair(boost::posix_time::ptime(), Segment::ptr_t());
        }
        Segment::map_t::const_iterator it = segments_.upper_bound(utc);
        if (!next && it != segments_.begin()) {
            --it;
        }
        if (it == segments_.end()) {
            return std::make_pair(boost::posix_time::ptime(), Segment::ptr_t());
        }
        return std::make_pair(it->first, it->second);
    }
    virtual void Send(Sender::ptr_t sender, const StreamOption& option) {
        const std::string log_prefix = (boost::format("%s > [ %s ]") % log_prefix_ % sender->GetOption().Get<std::string>("peer")).str();
        const std::chrono::steady_clock::time_point tick = std::chrono::steady_clock::now();
        const boost::posix_time::ptime startedAt = GetStartedAt(option.Get<std::string>("at"));
        if (startedAt.is_special()) {
            return;
        }
        const int32_t bufsiz = std::min<int32_t>(option.Get<int32_t>("bufsiz", 188 * 7), 1456);
        Logger::Info(boost::format("%s : started : %s") % log_prefix % option());
        Event::buf_t buf(bufsiz);
        SegmentReader::ptr_t reader;
        while (sender->IsConnected()) {
            if (!reader) {
                std::chrono::steady_clock::time_point tack = std::chrono::steady_clock::now();
                boost::posix_time::ptime at = startedAt + boost::posix_time::microseconds((tack - tick).count() / 1000); // nanosec to microsec
                time_segment_t segment = GetSegment(at);
                if (segment.first.is_special() || !segment.second) {
                    boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
                    continue;
                }
                if (segment.first > at) {
                    int64_t wait_ns = (segment.first - at).total_nanoseconds();
                    wait_ns = std::min<int64_t>(wait_ns, 1000ll * 1000 * 100); // 100[ms]
                    boost::this_thread::sleep_for(boost::chrono::nanoseconds(wait_ns));
                    continue;
                }
                int64_t offset_ns = (at - segment.first).total_nanoseconds();
                if (std::chrono::nanoseconds(offset_ns) >= segment_duration_) {
                    boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
                    continue;
                }
                reader.reset(new SegmentReader(log_prefix, segment.first, segment.second, idx_interval_, tack - std::chrono::nanoseconds(offset_ns)));
                if (!reader->Initialize(offset_ns / 1000 / 1000)) {
                    reader.reset();
                    boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
                    continue;
                }
            }
            buf.resize(bufsiz);
            if (!reader->Read(buf)) {
                time_segment_t segment = GetSegment(reader->Utc(), true);
                if (segment.second && segment.second->Continuous()) {
                    reader.reset(new SegmentReader(log_prefix, segment.first, segment.second, idx_interval_, reader->BaseTime() + segment_duration_));
                    if (!reader->Initialize(0)) {
                        reader.reset();
                    }
                } else {
                    reader.reset();
                }
                continue;
            }
            if (sender->Send(buf)) {
                continue;
            }
            std::string err = sender->GetErrMsg();
            Logger::Info(boost::format("%s : failed : %s") % log_prefix % err);
            break;
        }
        Logger::Info(boost::format("%s : done : %s") % log_prefix % option());
    }
    static boost::posix_time::ptime GetStartedAt(std::string str, const boost::posix_time::ptime now = boost::posix_time::microsec_clock::universal_time()) {
        if (boost::algorithm::istarts_with(str, "now-")) {
            try {
                double delaySec = boost::lexical_cast<double>(str.substr(4));
                boost::posix_time::microseconds delay(static_cast<long long>(delaySec * 1000 * 1000));
                return now - delay;
            } catch (boost::bad_lexical_cast) {
                return boost::posix_time::ptime();
            }
        }
        std::string tzd = "";
        std::string::size_type pos = str.find_last_of("TtZz+-");
        if (pos != std::string::npos && str[pos] != 'T' && str[pos] != 't') {
            tzd = str.substr(pos);
            str = str.substr(0, pos);
        }
        boost::posix_time::ptime time;
        bool extended = false;
        try {
            time = boost::posix_time::from_iso_string(str);
        } catch (boost::bad_lexical_cast) {
        }
        if (time.is_special()) {
            try {
                time = boost::posix_time::from_iso_extended_string(str);
                extended = true;
            } catch (boost::bad_lexical_cast) {
            }
        }
        if (time.is_special() || tzd[0] == 'z' || tzd[0] == 'Z') {
            return time;
        } else if (tzd[0] == '+' || tzd[0] == '-') {
            try {
                int32_t hours = 0, minutes = 0;
                if (extended) {
                    std::vector<std::string> tz;
                    boost::algorithm::split(tz, tzd, boost::algorithm::is_any_of(":"));
                    if (tz.size() >= 1) hours = boost::lexical_cast<int32_t>(tz[0]);
                    if (tz.size() >= 2) minutes = boost::lexical_cast<int32_t>(tz[1]);
                } else {
                    hours = boost::lexical_cast<int32_t>(tzd.substr(0, 3));
                    minutes = boost::lexical_cast<int32_t>(tzd.substr(3));
                }
                return time - boost::posix_time::time_duration(hours, minutes, 0);
            } catch (boost::bad_lexical_cast) {
            }
        }
        const boost::posix_time::ptime local = boost::date_time::c_local_adjustor<boost::posix_time::ptime>::utc_to_local(now);
        const boost::posix_time::posix_time_system::time_duration_type td = local - now;
        return time - td;
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
bool LoopRec::IsAcceptable(const StreamOption& streamOption) const {
    return pimpl_->IsAcceptable(streamOption);
}
void LoopRec::CreateSender(int sfd, const SendOption& sendOption, const StreamOption& streamOption) {
    return pimpl_->CreateSender(sfd, sendOption, streamOption);
}
bool LoopRec::OnReceive(const ReceiveOption& option, const Event::buf_t& buf, bool discrete) {
    return pimpl_->OnReceive(option, buf, discrete);
}
bool LoopRec::OnDisconnected(const ReceiveOption& option) {
    return pimpl_->OnDisconnected(option);
}
