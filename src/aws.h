#pragma once

#include "json.h"

class AWS {
    class Impl;
    typedef boost::scoped_ptr<Impl> pimpl_t;
    static pimpl_t pimpl_;
public:
    class S3Get {
        friend class AWS;
        class Impl;
        typedef boost::shared_ptr<Impl> pimpl_t;
        pimpl_t pimpl_;
        S3Get(pimpl_t pimpl = pimpl_t()) : pimpl_(pimpl) {}
    public:
        typedef std::vector<char> buf_t;
        virtual ~S3Get() {}
        virtual bool IsRunning() const;
        virtual bool Abort();
        virtual bool Wait();
        virtual size_t Read(buf_t& buf);
        virtual std::istream& GetStream();
    };
    class S3Put {
        friend class AWS;
        class Impl;
        typedef boost::shared_ptr<Impl> pimpl_t;
        pimpl_t pimpl_;
        S3Put(pimpl_t pimpl = pimpl_t()) : pimpl_(pimpl) {}
    public:
        virtual ~S3Put() {}
        virtual bool IsRunning() const;
        virtual bool Abort();
        virtual bool Wait();
    };
public:
    typedef std::function<void()> done_t;
    typedef std::function<void(const std::string& name, const std::string& message)> fail_t;
    static done_t DefaultDone;
    static fail_t DefaultFail;
public:
    static bool Init(const Json& conf);
    static void Term();
    static bool S3ListObjects(const std::string& bucketName, const std::string& marker, std::vector<std::string>& list);
    static bool S3DeleteObject(const std::string& bucketName, const std::string& keyName);
    static S3Get S3GetObject(const std::string& bucketName, const std::string& keyName, size_t bufSiz = 188 * 50, const done_t& done = DefaultDone, const fail_t& fail = DefaultFail);
    static S3Put S3PutObject(const std::string& bucketName, const std::string& keyName, const std::string& srcFile, const done_t& done = DefaultDone, const fail_t& fail = DefaultFail);
    static bool Test();
};
