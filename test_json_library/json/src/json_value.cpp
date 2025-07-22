#include <json/json.h>
#include <stdexcept>

namespace json {

Value::Value() : type(Type::Null), numberValue(0) {}

Value::Value(bool b) : type(Type::Bool), boolValue(b) {}

Value::Value(double d) : type(Type::Number), numberValue(d) {}

Value::Value(int i) : type(Type::Number), numberValue(static_cast<double>(i)) {}

Value::Value(const std::string& s) : type(Type::String), numberValue(0), stringValue(s) {}

Value::Value(const char* s) : type(Type::String), numberValue(0), stringValue(s) {}

Value::Value(const Array& a) : type(Type::Array), numberValue(0), arrayValue(a) {}

Value::Value(const Object& o) : type(Type::Object), numberValue(0), objectValue(o) {}

bool Value::asBool() const {
    if (type != Type::Bool) throw std::runtime_error("Value is not a boolean");
    return boolValue;
}

double Value::asNumber() const {
    if (type != Type::Number) throw std::runtime_error("Value is not a number");
    return numberValue;
}

const std::string& Value::asString() const {
    if (type != Type::String) throw std::runtime_error("Value is not a string");
    return stringValue;
}

Array& Value::asArray() {
    if (type != Type::Array) throw std::runtime_error("Value is not an array");
    return arrayValue;
}

const Array& Value::asArray() const {
    if (type != Type::Array) throw std::runtime_error("Value is not an array");
    return arrayValue;
}

Object& Value::asObject() {
    if (type != Type::Object) throw std::runtime_error("Value is not an object");
    return objectValue;
}

const Object& Value::asObject() const {
    if (type != Type::Object) throw std::runtime_error("Value is not an object");
    return objectValue;
}

std::shared_ptr<Value> Value::get(const std::string& key) {
    if (type != Type::Object) return nullptr;
    auto it = objectValue.find(key);
    if (it == objectValue.end()) return nullptr;
    return it->second;
}

std::shared_ptr<Value> Value::get(size_t index) {
    if (type != Type::Array) return nullptr;
    if (index >= arrayValue.size()) return nullptr;
    return arrayValue[index];
}

} // namespace json