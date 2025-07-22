#include <json/json.h>
#include <sstream>
#include <iomanip>

namespace json {

class Writer {
    std::ostringstream output;
    bool pretty;
    int indentLevel;
    
    void indent() {
        if (pretty) {
            output << std::string(indentLevel * 2, ' ');
        }
    }
    
    void newline() {
        if (pretty) {
            output << '\n';
        }
    }
    
public:
    Writer(bool p) : pretty(p), indentLevel(0) {}
    
    std::string write(const Value& value) {
        writeValue(value);
        return output.str();
    }
    
    std::string write(std::shared_ptr<Value> value) {
        if (value) {
            writeValue(*value);
        } else {
            output << "null";
        }
        return output.str();
    }
    
private:
    void writeValue(const Value& value) {
        switch (value.getType()) {
            case Type::Null:
                output << "null";
                break;
            case Type::Bool:
                output << (value.asBool() ? "true" : "false");
                break;
            case Type::Number:
                output << value.asNumber();
                break;
            case Type::String:
                writeString(value.asString());
                break;
            case Type::Array:
                writeArray(value.asArray());
                break;
            case Type::Object:
                writeObject(value.asObject());
                break;
        }
    }
    
    void writeString(const std::string& str) {
        output << '"';
        for (char ch : str) {
            switch (ch) {
                case '"': output << "\\\""; break;
                case '\\': output << "\\\\"; break;
                case '\b': output << "\\b"; break;
                case '\f': output << "\\f"; break;
                case '\n': output << "\\n"; break;
                case '\r': output << "\\r"; break;
                case '\t': output << "\\t"; break;
                default:
                    if (ch < 0x20) {
                        output << "\\u" << std::hex << std::setw(4) << std::setfill('0') << int(ch);
                    } else {
                        output << ch;
                    }
            }
        }
        output << '"';
    }
    
    void writeArray(const Array& arr) {
        output << '[';
        if (pretty && !arr.empty()) {
            indentLevel++;
            for (size_t i = 0; i < arr.size(); ++i) {
                if (i > 0) output << ',';
                newline();
                indent();
                if (arr[i]) {
                    writeValue(*arr[i]);
                } else {
                    output << "null";
                }
            }
            indentLevel--;
            newline();
            indent();
        } else {
            for (size_t i = 0; i < arr.size(); ++i) {
                if (i > 0) output << ',';
                if (arr[i]) {
                    writeValue(*arr[i]);
                } else {
                    output << "null";
                }
            }
        }
        output << ']';
    }
    
    void writeObject(const Object& obj) {
        output << '{';
        if (pretty && !obj.empty()) {
            indentLevel++;
            bool first = true;
            for (const auto& pair : obj) {
                if (!first) output << ',';
                first = false;
                newline();
                indent();
                writeString(pair.first);
                output << ':';
                if (pretty) output << ' ';
                if (pair.second) {
                    writeValue(*pair.second);
                } else {
                    output << "null";
                }
            }
            indentLevel--;
            newline();
            indent();
        } else {
            bool first = true;
            for (const auto& pair : obj) {
                if (!first) output << ',';
                first = false;
                writeString(pair.first);
                output << ':';
                if (pair.second) {
                    writeValue(*pair.second);
                } else {
                    output << "null";
                }
            }
        }
        output << '}';
    }
};

std::string stringify(const Value& value, bool pretty) {
    Writer writer(pretty);
    return writer.write(value);
}

std::string stringify(std::shared_ptr<Value> value, bool pretty) {
    Writer writer(pretty);
    return writer.write(value);
}

} // namespace json