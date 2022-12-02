#include "stdafx.h"
#include "json.h"

Json::Node& Json::Node::operator=(const boost::json::value& value) {
    if (value_) {
        *value_ = value;
        return *this;
    }
    value_ = root_;
    for (path_t::iterator it = path_.begin(); value_ && it != path_.end(); ++it) {
        if (it->idx != static_cast<size_t>(-1)) {
            if (value_->is_null()) {
                *value_ = boost::json::array();
            }
            if (!value_->is_array()) {
                value_ = nullptr;
                break;
            }
            boost::json::array& arr = value_->as_array();
            while (arr.size() <= it->idx) {
                arr.push_back(boost::json::value());
            }
            value_ = arr.if_contains(it->idx);
        } else if (!it->name.empty()) {
            if (value_->is_null()) {
                *value_ = boost::json::object();
            }
            if (!value_->is_object()) {
                value_ = nullptr;
                break;
            }
            boost::json::object& obj = value_->as_object();
            value_ = obj.if_contains(it->name);
            if (!value_) {
                obj[it->name] = boost::json::value();
                value_ = obj.if_contains(it->name);
            }
        } else {
            value_ = nullptr;
            break;
        }
    }
    if (value_) {
        *value_ = value;
    }
    return *this;
}

Json::Node Json::Node::operator[](const std::string& name) {
    path_t path = path_;
    path.push_back(Path{ name, static_cast<size_t>(-1) });
    if (undefined() || !value_->is_object()) return Node(path, root_, nullptr);
    return Node(path, root_, value_->as_object().if_contains(name));
}

Json::Node Json::Node::operator[](size_t idx) {
    path_t path = path_;
    path.push_back(Path{ "", idx });
    if (undefined() || !value_->is_array()) return Node(path, root_, nullptr);
    return Node(path, root_, value_->as_array().if_contains(idx));
}

const Json::Node Json::Node::operator[](const std::string& name) const {
    if (undefined() || !value_->is_object()) return Node(path_t(), nullptr, nullptr);
    return Node(path_t(), nullptr, value_->as_object().if_contains(name));
}

const Json::Node Json::Node::operator[](size_t idx) const {
    if (undefined() || !value_->is_array()) return Node(path_t(), nullptr, nullptr);
    return Node(path_t(), nullptr, value_->as_array().if_contains(idx));
}

template <typename Type> Type Json::Node::to(const Type& defVal) const {
    if (undefined() || value_->is_null()) return defVal;
    if (value_->is_int64()) return static_cast<Type>(value_->as_int64());
    if (value_->is_uint64()) return static_cast<Type>(value_->as_uint64());
    if (value_->is_double()) return static_cast<Type>(value_->as_double());
    if (value_->is_bool()) return static_cast<Type>(value_->as_bool());
    try {
        if (value_->is_string()) return boost::lexical_cast<Type>(value_->as_string());
        return defVal;
    } catch (boost::bad_lexical_cast&) {
        return defVal;
    }
}

template <> std::string Json::Node::to(const std::string& defVal) const {
    if (undefined() || value_->is_null()) return defVal;
    if (value_->is_string()) return value_->as_string().data();
    try {
        if (value_->is_int64()) return boost::lexical_cast<std::string>(value_->as_int64());
        if (value_->is_uint64()) return boost::lexical_cast<std::string>(value_->as_uint64());
        if (value_->is_double()) return boost::lexical_cast<std::string>(value_->as_double());
        if (value_->is_bool()) return boost::lexical_cast<std::string>(value_->as_bool());
        return defVal;
    } catch (boost::bad_lexical_cast&) {
        return defVal;
    }
}

template <> Json::keys_t Json::Node::to(const keys_t& defVal) const {
    if (undefined() || !value_->is_object()) return defVal;
    const boost::json::object& obj = value_->as_object();
    keys_t keys;
    for (boost::json::object::const_iterator it = obj.begin(); it != obj.end(); ++it) {
        keys.push_back(it->key());
    }
    return keys;
}

template char Json::Node::to(const char&) const;
template unsigned char Json::Node::to(const unsigned char&) const;
template short Json::Node::to(const short&) const;
template unsigned short Json::Node::to(const unsigned short&) const;
template int Json::Node::to(const int&) const;
template unsigned int Json::Node::to(const unsigned int&) const;
template long Json::Node::to(const long&) const;
template unsigned long Json::Node::to(const unsigned long&) const;
template long long Json::Node::to(const long long&) const;
template unsigned long long Json::Node::to(const unsigned long long&) const;
template double Json::Node::to(const double&) const;

Json Json::parse(const std::string& str) {
    boost::json::parse_options opt;
    opt.allow_comments = true;
    opt.allow_trailing_commas = true;
    opt.allow_invalid_utf8 = true;
    return Json(boost::json::parse(str, boost::json::storage_ptr(), opt));
}

Json Json::load(const std::string& path) {
    boost::json::parse_options opt;
    opt.allow_comments = true;
    opt.allow_trailing_commas = true;
    opt.allow_invalid_utf8 = true;
    boost::json::stream_parser parser(boost::json::storage_ptr(), opt);
    std::ifstream file(path);
    std::string line;
    while (std::getline(file, line)) parser.write(line + "\n");
    parser.finish();
    return Json(parser.release());
}
