#include <json/json.h>
#include <sstream>
#include <cctype>

namespace json {

class Parser {
    const std::string& input;
    size_t pos;
    
public:
    Parser(const std::string& in) : input(in), pos(0) {}
    
    std::shared_ptr<Value> parse() {
        skipWhitespace();
        auto result = parseValue();
        skipWhitespace();
        if (pos != input.length()) {
            throw std::runtime_error("Unexpected characters after JSON value");
        }
        return result;
    }
    
private:
    void skipWhitespace() {
        while (pos < input.length() && std::isspace(input[pos])) {
            pos++;
        }
    }
    
    std::shared_ptr<Value> parseValue() {
        skipWhitespace();
        if (pos >= input.length()) {
            throw std::runtime_error("Unexpected end of input");
        }
        
        char ch = input[pos];
        if (ch == '"') return parseString();
        if (ch == '{') return parseObject();
        if (ch == '[') return parseArray();
        if (ch == 't' || ch == 'f') return parseBool();
        if (ch == 'n') return parseNull();
        if (ch == '-' || std::isdigit(ch)) return parseNumber();
        
        throw std::runtime_error("Unexpected character");
    }
    
    std::shared_ptr<Value> parseString() {
        pos++; // Skip opening quote
        std::string result;
        
        while (pos < input.length() && input[pos] != '"') {
            if (input[pos] == '\\') {
                pos++;
                if (pos >= input.length()) {
                    throw std::runtime_error("Unexpected end of string");
                }
                char escape = input[pos];
                switch (escape) {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case '/': result += '/'; break;
                    case 'b': result += '\b'; break;
                    case 'f': result += '\f'; break;
                    case 'n': result += '\n'; break;
                    case 'r': result += '\r'; break;
                    case 't': result += '\t'; break;
                    default: throw std::runtime_error("Invalid escape sequence");
                }
            } else {
                result += input[pos];
            }
            pos++;
        }
        
        if (pos >= input.length()) {
            throw std::runtime_error("Unterminated string");
        }
        
        pos++; // Skip closing quote
        return std::make_shared<Value>(result);
    }
    
    std::shared_ptr<Value> parseNumber() {
        size_t start = pos;
        
        if (input[pos] == '-') pos++;
        
        if (!std::isdigit(input[pos])) {
            throw std::runtime_error("Invalid number");
        }
        
        while (pos < input.length() && std::isdigit(input[pos])) {
            pos++;
        }
        
        if (pos < input.length() && input[pos] == '.') {
            pos++;
            if (!std::isdigit(input[pos])) {
                throw std::runtime_error("Invalid number");
            }
            while (pos < input.length() && std::isdigit(input[pos])) {
                pos++;
            }
        }
        
        std::string numStr = input.substr(start, pos - start);
        double num = std::stod(numStr);
        return std::make_shared<Value>(num);
    }
    
    std::shared_ptr<Value> parseBool() {
        if (input.substr(pos, 4) == "true") {
            pos += 4;
            return std::make_shared<Value>(true);
        } else if (input.substr(pos, 5) == "false") {
            pos += 5;
            return std::make_shared<Value>(false);
        }
        throw std::runtime_error("Invalid boolean value");
    }
    
    std::shared_ptr<Value> parseNull() {
        if (input.substr(pos, 4) == "null") {
            pos += 4;
            return std::make_shared<Value>();
        }
        throw std::runtime_error("Invalid null value");
    }
    
    std::shared_ptr<Value> parseArray() {
        pos++; // Skip '['
        Array arr;
        
        skipWhitespace();
        if (pos < input.length() && input[pos] == ']') {
            pos++;
            return std::make_shared<Value>(arr);
        }
        
        while (true) {
            arr.push_back(parseValue());
            skipWhitespace();
            
            if (pos >= input.length()) {
                throw std::runtime_error("Unterminated array");
            }
            
            if (input[pos] == ']') {
                pos++;
                break;
            }
            
            if (input[pos] != ',') {
                throw std::runtime_error("Expected ',' or ']' in array");
            }
            pos++;
        }
        
        return std::make_shared<Value>(arr);
    }
    
    std::shared_ptr<Value> parseObject() {
        pos++; // Skip '{'
        Object obj;
        
        skipWhitespace();
        if (pos < input.length() && input[pos] == '}') {
            pos++;
            return std::make_shared<Value>(obj);
        }
        
        while (true) {
            skipWhitespace();
            if (pos >= input.length() || input[pos] != '"') {
                throw std::runtime_error("Expected string key in object");
            }
            
            auto keyValue = parseString();
            std::string key = keyValue->asString();
            
            skipWhitespace();
            if (pos >= input.length() || input[pos] != ':') {
                throw std::runtime_error("Expected ':' after object key");
            }
            pos++;
            
            obj[key] = parseValue();
            skipWhitespace();
            
            if (pos >= input.length()) {
                throw std::runtime_error("Unterminated object");
            }
            
            if (input[pos] == '}') {
                pos++;
                break;
            }
            
            if (input[pos] != ',') {
                throw std::runtime_error("Expected ',' or '}' in object");
            }
            pos++;
        }
        
        return std::make_shared<Value>(obj);
    }
};

std::shared_ptr<Value> parse(const std::string& json) {
    Parser parser(json);
    return parser.parse();
}

} // namespace json