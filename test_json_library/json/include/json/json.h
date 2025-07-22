#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>

namespace json {

class Value;
using Object = std::map<std::string, std::shared_ptr<Value>>;
using Array = std::vector<std::shared_ptr<Value>>;

enum class Type {
    Null,
    Bool,
    Number,
    String,
    Array,
    Object
};

class Value {
public:
    Value();
    explicit Value(bool b);
    explicit Value(double d);
    explicit Value(int i);
    explicit Value(const std::string& s);
    explicit Value(const char* s);
    explicit Value(const Array& a);
    explicit Value(const Object& o);

    Type getType() const { return type; }
    
    bool isNull() const { return type == Type::Null; }
    bool isBool() const { return type == Type::Bool; }
    bool isNumber() const { return type == Type::Number; }
    bool isString() const { return type == Type::String; }
    bool isArray() const { return type == Type::Array; }
    bool isObject() const { return type == Type::Object; }

    bool asBool() const;
    double asNumber() const;
    const std::string& asString() const;
    Array& asArray();
    const Array& asArray() const;
    Object& asObject();
    const Object& asObject() const;

    std::shared_ptr<Value> get(const std::string& key);
    std::shared_ptr<Value> get(size_t index);

private:
    Type type;
    union {
        bool boolValue;
        double numberValue;
    };
    std::string stringValue;
    Array arrayValue;
    Object objectValue;
};

// Parser
std::shared_ptr<Value> parse(const std::string& json);

// Writer
std::string stringify(const Value& value, bool pretty = false);
std::string stringify(std::shared_ptr<Value> value, bool pretty = false);

} // namespace json