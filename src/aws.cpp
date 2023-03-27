#include "stdafx.h"
#include "aws.h"

#if !defined(USE_AWSSDK)
AWS::done_t AWS::DefaultDone = []() {};
AWS::fail_t AWS::DefaultFail = [](const std::string&, const std::string&) {};
bool AWS::S3Get::IsRunning() const {
    return false;
}
bool AWS::S3Get::Abort() {
    return false;
}
bool AWS::S3Get::Wait() {
    return false;
}
size_t AWS::S3Get::Read(buf_t& buf) {
    return 0;
}
bool AWS::S3Put::IsRunning() const {
    return false;
}
bool AWS::S3Put::Abort() {
    return false;
}
bool AWS::S3Put::Wait() {
    return false;
}
bool AWS::Init(const Json& conf) {
    return false;
}
void AWS::Term() {
}
bool AWS::S3ListObjects(const std::string& bucketName, const std::string& marker, std::vector<std::string>& list) {
    return false;
}
bool AWS::S3DeleteObject(const std::string& bucketName, const std::string& keyName) {
    return false;
}
AWS::S3Get AWS::S3GetObject(const std::string& bucketName, const std::string& keyName, size_t bufSiz, const done_t& done, const fail_t& fail) {
    return S3Get();
}
AWS::S3Put AWS::S3PutObject(const std::string& bucketName, const std::string& keyName, const std::string& srcFile, const done_t& done, const fail_t& fail) {
    return S3Put();
}
bool AWS::Test() {
    return false;
}
#else

#include <aws/core/Aws.h>
//#include <aws/core/utils/threading/Executor.h>
#include <aws/s3/S3Client.h>
//#include <aws/core/utils/memory/AWSMemory.h>
//#include <aws/core/utils/memory/stl/AWSStreamFwd.h>
//#include <aws/core/utils/stream/PreallocatedStreamBuf.h>
//#include <aws/core/utils/StringUtils.h>

#include <aws/s3/model/CreateBucketRequest.h>
#include <aws/s3/model/DeleteBucketRequest.h>
#include <aws/s3/model/ListObjectsRequest.h>
#include <aws/s3/model/DeleteObjectRequest.h>
#include <aws/s3/model/PutObjectRequest.h>
#include <aws/s3/model/GetObjectRequest.h>
//#include <aws/s3/model/CreateMultipartUploadRequest.h>
//#include <aws/s3/model/AbortMultipartUploadRequest.h>
//#include <aws/s3/model/CompleteMultipartUploadRequest.h>
//#include <aws/s3/model/UploadPartRequest.h>
//#include <aws/core/utils/UUID.h>
//#include <aws/core/utils/stream/PreallocatedStreamBuf.h>
//#include <aws/core/utils/stream/ConcurrentStreamBuf.h>

#if defined(WIN32) || defined(WIN64)
#pragma comment(lib, "userenv.lib")
#pragma comment(lib, "version.lib")
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "ncrypt.lib")
#pragma comment(lib, "secur32.lib")
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "shlwapi.lib")
#endif

#include "logger.h"

class AWS::S3Get::Impl {
    class Bufstream : public std::streambuf {
        buf_t buf_;
        boost::mutex mutex_;
        boost::condition_variable cond_;
        volatile bool eos_; // end-of-stream
    public:
        Bufstream(size_t bufSiz) : buf_(), mutex_(), cond_(), eos_(false) {
            buf_.resize(std::max<size_t>(bufSiz, 1), 0);
            this->setp(&buf_.at(0), &buf_.at(0) + buf_.size());
            this->setg(&buf_.at(0), &buf_.at(0), &buf_.at(0) + buf_.size());
        }
        size_t Read(buf_t& buf) {
            if (!buf.size()) return 0;
            size_t pos = 0;
            while (pos < buf.size()) {
                size_t size = std::min<size_t>(Readable(), buf.size() - pos);
                if (size) {	// copy from read buffer
                    traits_type::copy(&buf.at(pos), this->gptr(), size);
                    pos += size;
                    this->gbump(static_cast<int>(size));
                } else if (!eos_) {
                    boost::unique_lock<boost::mutex> lk(mutex_);
                    cond_.notify_one();
                    cond_.wait(lk);
                } else {
                    break;
                }
            }
            return pos;
        }
        void SetEos() {
            if (eos_) return;
            boost::unique_lock<boost::mutex> lk(mutex_);
            eos_ = true;
            cond_.notify_one();
        }
    protected:
        size_t Readable() const {
            const char* gp = this->gptr();
            const char* pp = this->pptr();
            return (gp && pp && pp > gp) ? (pp - gp) : 0;
        }
        int_type overflow(int_type ch) override {
            boost::unique_lock<boost::mutex> lk(mutex_);
            for (;;) {
                if (eos_) return traits_type::eof();
                if (!Readable()) break;
                cond_.notify_one();
                cond_.wait(lk);
            }
            this->setp(&buf_.at(0), &buf_.at(0) + buf_.size());
            this->setg(&buf_.at(0), &buf_.at(0), &buf_.at(0) + buf_.size());
            if (!traits_type::eq_int_type(ch, traits_type::eof())) this->sputc(ch);
            cond_.notify_one();
            return traits_type::not_eof(ch);
        }
    };
    enum State { Failed = -1, Ready, Running, Done };
    Aws::S3::S3Client client_;
    Aws::String bucketName_;
    Aws::String keyName_;
    done_t done_;
    fail_t fail_;
    State state_;
    Bufstream bufstream_;
    boost::mutex mutex_;
    boost::condition_variable cond_;
public:
    Impl(const Aws::S3::S3ClientConfiguration& clientConfig, const std::string& bucketName, const std::string& keyName, size_t bufSiz, const done_t& done, const fail_t& fail)
        : client_(clientConfig), bucketName_(bucketName), keyName_(keyName), done_(done), fail_(fail), state_(Ready), bufstream_(bufSiz), mutex_(), cond_() {
        Begin();
    }
    virtual ~Impl() {
        Wait();
    }
    virtual bool IsRunning() const {
        return state_ == Running;
    }
    virtual bool Abort() {
        boost::unique_lock<boost::mutex> lk(mutex_);
        if (!IsRunning()) return false;
        client_.DisableRequestProcessing();
        bufstream_.SetEos();
        return true;
    }
    virtual bool Wait() {
        boost::unique_lock<boost::mutex> lk(mutex_);
        if (IsRunning()) cond_.wait(lk);
        return state_ == Done;
    }
    virtual size_t Read(buf_t& buf) {
        return bufstream_.Read(buf);
    }
protected:
    void Begin() {
        Logger::Trace(boost::format("AWS::S3GetObject(%s, %s) ->") % bucketName_ % keyName_);
        Aws::S3::Model::GetObjectRequest request;
        request.SetBucket(bucketName_);
        request.SetKey(keyName_);
        Aws::IOStream* stream = Aws::New<Aws::IOStream>("SampleAllocationTag", &bufstream_);
        request.SetResponseStreamFactory([stream]() { return stream; });
        //std::shared_ptr<Aws::Client::AsyncCallerContext> context = Aws::MakeShared<Aws::Client::AsyncCallerContext>("GetObjectAllocationTag");
        //context->SetUUID(keyName_);
        client_.GetObjectAsync(request, [this](const Aws::S3::S3Client* client,
            const Aws::S3::Model::GetObjectRequest& request, const Aws::S3::Model::GetObjectOutcome& outcome,
            const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context) {
            boost::unique_lock<boost::mutex> lk(mutex_);
            bufstream_.SetEos();
            if (!outcome.IsSuccess()) {
                const Aws::S3::S3Error& err = outcome.GetError();
                Logger::Error(boost::format("AWS::S3GetObject(%s, %s): Error: %s: %s") % bucketName_ % keyName_ % err.GetExceptionName() % err.GetMessage());
                state_ = Failed;
                fail_(err.GetExceptionName(), err.GetMessage());
            } else {
                Logger::Trace(boost::format("AWS::S3GetObject(%s, %s): Done") % bucketName_ % keyName_);
                state_ = Done;
                done_();
            }
            cond_.notify_one();
        //}, context);
        });
        state_ = Running;
    }
};

bool AWS::S3Get::IsRunning() const {
    return pimpl_ ? pimpl_->IsRunning() : false;
}

bool AWS::S3Get::Abort() {
    return pimpl_ ? pimpl_->Abort() : false;
}

bool AWS::S3Get::Wait() {
    return pimpl_ ? pimpl_->Wait() : false;
}

size_t AWS::S3Get::Read(buf_t& buf) {
    return pimpl_ ? pimpl_->Read(buf) : 0;
}

class AWS::S3Put::Impl {
    enum State { Failed = -1, Ready, Running, Done };
    Aws::S3::S3Client client_;
    Aws::String bucketName_;
    Aws::String keyName_;
    Aws::String srcFile_;
    done_t done_;
    fail_t fail_;
    State state_;
    boost::mutex mutex_;
    boost::condition_variable cond_;
public:
    Impl(const Aws::S3::S3ClientConfiguration& clientConfig, const std::string& bucketName, const std::string& keyName, const std::string& srcFile, const done_t& done, const fail_t& fail)
        : client_(clientConfig), bucketName_(bucketName), keyName_(keyName), srcFile_(srcFile), done_(done), fail_(fail), state_(Ready), mutex_(), cond_() {
        Begin();
    }
    virtual ~Impl() {
        Wait();
    }
    virtual bool IsRunning() const {
        return state_ == Running;
    }
    virtual bool Abort() {
        boost::unique_lock<boost::mutex> lk(mutex_);
        if (!IsRunning()) return false;
        client_.DisableRequestProcessing();
        return true;
    }
    virtual bool Wait() {
        boost::unique_lock<boost::mutex> lk(mutex_);
        if (IsRunning()) cond_.wait(lk);
        return state_ == Done;
    }
protected:
    void Begin() {
        Logger::Trace(boost::format("AWS::S3PutObject(%s, %s, %s) ->") % bucketName_ % keyName_ % srcFile_);
        Aws::S3::Model::PutObjectRequest request;
        request.SetBucket(bucketName_);
        request.SetKey(keyName_);
        const std::shared_ptr<Aws::IOStream> infile = Aws::MakeShared<Aws::FStream>("SampleAllocationTag", srcFile_.c_str(), std::ios_base::in | std::ios_base::binary);
        if (!*infile) {
            Logger::Error(boost::format("AWS::S3PutObject(%s, %s, %s): Error: unable to open file") % bucketName_ % keyName_ % srcFile_);
            state_ = Failed;
            return;
        }
        request.SetBody(infile);
        //std::shared_ptr<Aws::Client::AsyncCallerContext> context = Aws::MakeShared<Aws::Client::AsyncCallerContext>("PutObjectAllocationTag");
        //context->SetUUID(keyName_);
        client_.PutObjectAsync(request, [this](const Aws::S3::S3Client* client,
            const Aws::S3::Model::PutObjectRequest& request, const Aws::S3::Model::PutObjectOutcome& outcome,
            const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context) {
            boost::unique_lock<boost::mutex> lk(mutex_);
            if (!outcome.IsSuccess()) {
                const Aws::S3::S3Error& err = outcome.GetError();
                Logger::Error(boost::format("AWS::S3PutObject(%s, %s, %s): Error: %s: %s") % bucketName_ % keyName_ % srcFile_ % err.GetExceptionName() % err.GetMessage());
                state_ = Failed;
                fail_(err.GetExceptionName(), err.GetMessage());
            } else {
                Logger::Trace(boost::format("AWS::S3PutObject(%s, %s, %s): Done") % bucketName_ % keyName_ % srcFile_);
                state_ = Done;
                done_();
            }
            cond_.notify_one();
        //}, context);
        });
        state_ = Running;
    }
};

bool AWS::S3Put::IsRunning() const {
    return pimpl_ ? pimpl_->IsRunning() : false;
}

bool AWS::S3Put::Abort() {
    return pimpl_ ? pimpl_->Abort() : false;
}

bool AWS::S3Put::Wait() {
    return pimpl_ ?  pimpl_->Wait() : false;
}

class AwsLogAdapter : public Aws::Utils::Logging::LogSystemInterface {
    Aws::Utils::Logging::LogLevel logLevel_;
    std::string prefix_;
public:
    AwsLogAdapter(Aws::Utils::Logging::LogLevel logLevel, const std::string& prefix) : logLevel_(logLevel), prefix_(prefix) {}
    virtual Aws::Utils::Logging::LogLevel GetLogLevel(void) const override {
        return logLevel_;
    }
    virtual void Log(Aws::Utils::Logging::LogLevel logLevel, const char* tag, const char* formatStr, ...) override {
        va_list ap;
        va_start(ap, formatStr);
#if defined(WIN32) || defined(WIN64)
        size_t len = _vscprintf(formatStr, ap);
        std::vector<char> buf(len + 1, '\0');
        buf.resize(_vsnprintf(buf.data(), len, formatStr, ap) + 1);
#else
        va_list ap2;
        va_copy(ap2, ap);
        size_t len = vsnprintf(nullptr, 0, formatStr, ap2);
        std::vector<char> buf(len + 1, '\0');
        buf.resize(vsnprintf(buf.data(), len + 1, formatStr, ap) + 1);
#endif
        va_end(ap);
        Aws::OStringStream ss;
        ss << &buf.at(0);
        LogStream(logLevel, tag, ss);
    }
    virtual void LogStream(Aws::Utils::Logging::LogLevel logLevel, const char* tag, const Aws::OStringStream &messageStream) override {
        switch (logLevel) {
            case Aws::Utils::Logging::LogLevel::Trace: Logger::Trace(boost::format("%s|%s : %s") % prefix_ % tag % messageStream.str()); break;
            case Aws::Utils::Logging::LogLevel::Debug: Logger::Debug(boost::format("%s|%s : %s") % prefix_ % tag % messageStream.str()); break;
            case Aws::Utils::Logging::LogLevel::Info: Logger::Info(boost::format("%s|%s : %s") % prefix_ % tag % messageStream.str()); break;
            case Aws::Utils::Logging::LogLevel::Warn: Logger::Warning(boost::format("%s|%s : %s") % prefix_ % tag % messageStream.str()); break;
            case Aws::Utils::Logging::LogLevel::Error: Logger::Error(boost::format("%s|%s : %s") % prefix_ % tag % messageStream.str()); break;
            case Aws::Utils::Logging::LogLevel::Fatal: Logger::Fatal(boost::format("%s|%s : %s") % prefix_ % tag % messageStream.str()); break;
        }
    }
    virtual void Flush() override {
    }
};

class AWS::Impl {
    Aws::SDKOptions options_;
    boost::scoped_ptr<Aws::Client::ClientConfiguration> clientConfig_;
public:
    Impl() : options_(), clientConfig_() {
    }
    virtual ~Impl() {
        Term();
    }
    virtual bool Initialize(const Json& conf) {
        std::string level = conf["aws"]["loglevel"].to<std::string>();
        if (level.empty()) conf["logger"]["level"].to<std::string>();
        if (boost::istarts_with(level, "t")) {
            options_.loggingOptions.logLevel = Aws::Utils::Logging::LogLevel::Trace;
        } else if (boost::istarts_with(level, "d")) {
            options_.loggingOptions.logLevel = Aws::Utils::Logging::LogLevel::Debug;
        } else if (boost::istarts_with(level, "w")) {
            options_.loggingOptions.logLevel = Aws::Utils::Logging::LogLevel::Warn;
        } else if (boost::istarts_with(level, "e")) {
            options_.loggingOptions.logLevel = Aws::Utils::Logging::LogLevel::Error;
        } else if (boost::istarts_with(level, "f")) {
            options_.loggingOptions.logLevel = Aws::Utils::Logging::LogLevel::Fatal;
        } else {
            options_.loggingOptions.logLevel = Aws::Utils::Logging::LogLevel::Info;
        }
        options_.loggingOptions.logger_create_fn = [&]() {
            return std::shared_ptr<Aws::Utils::Logging::LogSystemInterface>(
                new AwsLogAdapter(options_.loggingOptions.logLevel, conf["aws"]["logprefix"].to<std::string>("AWSSDK"))
            );
        };
        Aws::InitAPI(options_);
        clientConfig_.reset(new Aws::Client::ClientConfiguration);
        clientConfig_->region = conf["aws"]["region"].to<std::string>();
        return true;
    }
    virtual void Term() {
        Aws::ShutdownAPI(options_);
    }
    virtual bool S3ListBuckets(std::vector<std::string>& list) const {
        Logger::Trace(boost::format("AWS::S3ListBucket() ->"));
        Aws::S3::S3Client client(*clientConfig_);
        Aws::S3::Model::ListBucketsOutcome outcome = client.ListBuckets();
        list.clear();
        if (!outcome.IsSuccess()) {
            const Aws::S3::S3Error& err = outcome.GetError();
            Logger::Error(boost::format("AWS::S3ListBuckets: Error: %s: %s") % err.GetExceptionName() % err.GetMessage());
            return false;
        } else {
            Aws::Vector<Aws::S3::Model::Bucket> buckets = outcome.GetResult().GetBuckets();
            Logger::Trace(boost::format("AWS::S3ListBuckets: Found: %d") % outcome.GetResult().GetBuckets().size());
            for (Aws::S3::Model::Bucket& b : buckets) list.push_back(b.GetName());
            return true;
        }
    }
    virtual bool S3CreateBucket(const Aws::String& bucketName) {
        Logger::Trace(boost::format("AWS::S3CreateBucket(%s) ->") % bucketName);
        Aws::S3::S3Client client(*clientConfig_);
        Aws::S3::Model::CreateBucketRequest request;
        request.SetBucket(bucketName);
        Aws::S3::Model::CreateBucketConfiguration createBucketConfig;
        if (clientConfig_->region != "us-east-1") {
            createBucketConfig.SetLocationConstraint(Aws::S3::Model::BucketLocationConstraintMapper::GetBucketLocationConstraintForName(clientConfig_->region));
        }
        request.SetCreateBucketConfiguration(createBucketConfig);
        Aws::S3::Model::CreateBucketOutcome outcome = client.CreateBucket(request);
        if (!outcome.IsSuccess()) {
            const Aws::S3::S3Error& err = outcome.GetError();
            Logger::Error(boost::format("AWS::S3CreateBucket(%s): Error: %s: %s") % bucketName % err.GetExceptionName() % err.GetMessage());
            return false;
        } else {
            Logger::Trace(boost::format("AWS::S3CreateBucket(%s): Done") % bucketName);
            return true;
        }
    }
    virtual bool S3DeleteBucket(const Aws::String& bucketName) {
        Logger::Trace(boost::format("AWS::S3DeleteBucket(%s) ->") % bucketName);
        Aws::S3::S3Client client(*clientConfig_);
        Aws::S3::Model::DeleteBucketRequest request;
        request.SetBucket(bucketName);
        Aws::S3::Model::DeleteBucketOutcome outcome = client.DeleteBucket(request);
        if (!outcome.IsSuccess()) {
            const Aws::S3::S3Error& err = outcome.GetError();
            Logger::Error(boost::format("AWS::S3DeleteBucket(%s): Error: %s: %s") % bucketName % err.GetExceptionName() % err.GetMessage());
            return false;
        } else {
            Logger::Trace(boost::format("AWS::S3DeleteBucket(%s): Done") % bucketName);
            return true;
        }
    }
    virtual bool S3ListObjects(const std::string& bucketName, const std::string& marker, std::vector<std::string>& list) const {
        Logger::Trace(boost::format("AWS::S3ListObjects(%s, %s) ->") % bucketName % marker);
        Aws::S3::S3Client s3_client(*clientConfig_);
        Aws::S3::Model::ListObjectsRequest request;
        request.SetBucket(bucketName);
        if (!marker.empty()) request.SetMarker(marker);
        list.clear();
        Aws::S3::Model::ListObjectsOutcome outcome = s3_client.ListObjects(request);
        if (!outcome.IsSuccess()) {
            const Aws::S3::S3Error& err = outcome.GetError();
            Logger::Error(boost::format("AWS::S3ListObjects(%s, %s): Error: %s: %s") % bucketName % marker %  err.GetExceptionName() % err.GetMessage());
            return false;
        } else {
            Aws::Vector<Aws::S3::Model::Object> objects = outcome.GetResult().GetContents();
            Logger::Trace(boost::format("AWS::S3ListObjects(%s, %s): Found: %u") % bucketName % marker % objects.size());
            for (Aws::S3::Model::Object& object : objects) list.push_back(object.GetKey());
            return true;
        }
    }
    virtual bool S3DeleteObject(const Aws::String& bucketName, const Aws::String& keyName) {
        Logger::Trace(boost::format("AWS::S3DeleteObject(%s, %s) ->") % bucketName % keyName);
        Aws::S3::S3Client s3_client(*clientConfig_);
        Aws::S3::Model::DeleteObjectRequest request;
        request.SetBucket(bucketName);
        request.SetKey(keyName);
        Aws::S3::Model::DeleteObjectOutcome outcome = s3_client.DeleteObject(request);
        if (!outcome.IsSuccess()) {
            const Aws::S3::S3Error& err = outcome.GetError();
            Logger::Error(boost::format("AWS::S3DeleteObject(%s, %s): Error: %s: %s") % bucketName % keyName %  err.GetExceptionName() % err.GetMessage());
            return false;
        } else {
            Logger::Trace(boost::format("AWS::S3DeleteObject(%s, %s): Done") % bucketName % keyName);
            return true;
        }
    }
    virtual S3Get::pimpl_t S3GetObject(const std::string& bucketName, const std::string& keyName, size_t bufSiz, const done_t& done, const fail_t& fail) {
        return S3Get::pimpl_t(new S3Get::Impl(*clientConfig_, bucketName, keyName, bufSiz, done, fail));
    }
    virtual S3Put::pimpl_t S3PutObject(const std::string& bucketName, const std::string& keyName, const std::string& srcFile, const done_t& done, const fail_t& fail) {
        return S3Put::pimpl_t(new S3Put::Impl(*clientConfig_, bucketName, keyName, srcFile, done, fail));
    }
protected:

};

AWS::pimpl_t AWS::pimpl_;
AWS::done_t AWS::DefaultDone = []() {};
AWS::fail_t AWS::DefaultFail = [](const std::string&, const std::string&) {};

bool AWS::Init(const Json& conf) {
    if (pimpl_) return true;
    pimpl_.reset(new Impl());
    if (pimpl_->Initialize(conf)) return true;
    pimpl_.reset();
    return false;
}

void AWS::Term() {
    pimpl_.reset();
}

bool AWS::S3ListObjects(const std::string& bucketName, const std::string& marker, std::vector<std::string>& list) {
    if (!pimpl_) return false;
    return pimpl_->S3ListObjects(bucketName, marker, list);
}

bool AWS::S3DeleteObject(const std::string& bucketName, const std::string& keyName) {
    if (!pimpl_) return false;
    return pimpl_->S3DeleteObject(bucketName, keyName);
}

AWS::S3Get AWS::S3GetObject(const std::string& bucketName, const std::string& keyName, size_t bufSiz, const done_t& done, const fail_t& fail) {
    if (!pimpl_) return S3Get();
    return S3Get(pimpl_->S3GetObject(bucketName, keyName, bufSiz, done, fail));
}

AWS::S3Put AWS::S3PutObject(const std::string& bucketName, const std::string& keyName, const std::string& srcFile, const done_t& done, const fail_t& fail) {
    if (!pimpl_) return S3Put();
    return S3Put(pimpl_->S3PutObject(bucketName, keyName, srcFile, done, fail));
}

bool AWS::Test() {
    Aws::String bucketName = "wakabayashik-test-c7510592-5e75-475c-b841-34cc6f8190e8";
    std::vector<std::string> list;
    pimpl_->S3CreateBucket(bucketName + "x");
    //pimpl_->S3ListBuckets(list);
    //for (std::string& b : list) {
    //    std::cout << " - bucket: " << b << std::endl;
    //}
    pimpl_->S3ListObjects(bucketName, "test", list);
    for (std::string& o : list) {
        std::cout << " - object: " << o << std::endl;
    }
    if (0) {
        std::string keyName = "test/ResponseStream.h";
        std::string srcFile = "D:\\Projects\\reference\\vcpkg\\installed\\x64-windows-static\\include\\aws\\core\\utils\\stream\\ResponseStream.h";
        S3Put put = S3PutObject(bucketName, keyName, srcFile, []() {
            std::cout << "Put DONE" << std::endl;
        }, [](const std::string& name, const std::string& message) {
            std::cout << "Put FAIL:" << name << ": " << message << std::endl;
        });
        //put.Abort();
    }
    if (1) {
        std::string keyName = "test/ResponseStream.h";
        S3Get get = S3GetObject(bucketName, keyName, 188 * 50, []() {
            std::cout << "Get DONE" << std::endl;
        }, [](const std::string& name, const std::string& message) {
            std::cout << "Get FAIL:" << name << ": " << message << std::endl;
        });
        std::stringstream ss;
        S3Get::buf_t buf;
        for (int i = 0; ; ++i) {
            buf.resize(188);
            buf.resize(get.Read(buf));
            if (!buf.size()) break;
            std::cout << "read: " << buf.size() << std::endl;
            ss << std::string(&buf.at(0), buf.size());
        }
        std::string str = ss.str();
        std::cout << "total read: " << str.length() << std::endl;
        std::cout << str << std::endl;
    }
    //pimpl_->S3DeleteObject(bucketName, "test/ResponseStream.h");
    pimpl_->S3DeleteBucket(bucketName + "x");
    return true;
}

#endif // !defined(USE_AWSSDK)
