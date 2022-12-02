#pragma once

class Json : public boost::json::value
{
    struct Path { std::string name; size_t idx; };
    typedef std::vector<Path> path_t;
public:
    class Node {
        path_t path_;
        boost::json::value* root_;
        boost::json::value* value_;
    protected:
        friend class Json;
        Node(boost::json::value* value) : path_(), root_(value), value_(value) {}
        Node(const path_t& path, boost::json::value* root, boost::json::value* value) : path_(path), root_(root), value_(value) {}
    public:
        Node& operator=(const boost::json::value& value);
        Node operator[](const std::string& name);
        Node operator[](size_t idx);
        const Node operator[](const std::string& name) const;
        const Node operator[](size_t idx) const;
        size_t size() const { return (!value_ || !value_->is_array()) ? 0 : value_->as_array().size(); }
        bool undefined() const { return !value_; }
        boost::json::value to_value() const { return value_ ? *value_ : boost::json::value(); }
        std::string to_string() const { return value_ ? boost::json::serialize(*value_) : ""; }
        template <typename Type> Type to(const Type& defVal = Type()) const;
    };
public:
    typedef std::vector<std::string> keys_t;
    Json(const boost::json::value& value = boost::json::value()) : boost::json::value(value) {}
    Json(const Node& node) : boost::json::value(node.to_value()) {}
    Json& operator=(const boost::json::value& value) { *static_cast<boost::json::value*>(this) = value; return *this; }
    Node operator[](const std::string& name) { return Node(this)[name]; }
    Node operator[](size_t idx) { return Node(this)[idx]; }
    const Node operator[](const std::string& name) const { return Node(const_cast<Json*>(this))[name]; }
    const Node operator[](size_t idx) const { return Node(const_cast<Json*>(this))[idx]; }
    size_t size() const { return Node(const_cast<Json*>(this)).size(); }
    std::string to_string() const { return Node(const_cast<Json*>(this)).to_string(); }
    template <typename Type> Type to(const Type& defVal = Type()) const { return Node(const_cast<Json*>(this)).to<Type>(defVal); }
    static Json parse(const std::string& str);
    static Json load(const std::string& path);
};
