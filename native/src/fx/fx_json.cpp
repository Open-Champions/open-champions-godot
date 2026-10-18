#include "champions/fx.hpp"
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <utility>

namespace champions::rta::fx {
namespace {
struct Value {
    enum Type { Null, Boolean, Number, String, Array, Object } type = Null;
    bool boolean = false;
    double number = 0;
    std::string string;
    std::vector<Value> array;
    std::map<std::string,Value> object;
    const Value &at(const char *key) const {
        const auto i=object.find(key);
        if (i==object.end()) throw std::runtime_error(std::string("missing ")+key);
        return i->second;
    }
    int integer() const {
        if (type!=Number || !std::isfinite(number) || number<std::numeric_limits<int>::min() ||
            number>std::numeric_limits<int>::max() || std::trunc(number)!=number)
            throw std::runtime_error("expected integer");
        return int(number);
    }
    float real() const {
        if (type!=Number || !std::isfinite(number) ||
            std::abs(number)>std::numeric_limits<float>::max())
            throw std::runtime_error("expected finite number");
        return float(number);
    }
};
class Parser {
public:
    explicit Parser(const std::string &text):text_(text) {}
    Value parse() {
        Value v=read(0); white();
        if (i_!=text_.size()) throw std::runtime_error("trailing JSON content");
        return v;
    }
private:
    const std::string &text_;
    std::size_t i_=0;
    void white() { while (i_<text_.size() && std::isspace(static_cast<unsigned char>(text_[i_]))) ++i_; }
    char take() {
        if (i_==text_.size()) throw std::runtime_error("truncated JSON");
        return text_[i_++];
    }
    void expect(char c) { white(); if (take()!=c) throw std::runtime_error("invalid JSON delimiter"); }
    Value read(int depth) {
        if (depth>12) throw std::runtime_error("JSON nesting exceeds limit");
        white(); if (i_==text_.size()) throw std::runtime_error("truncated JSON");
        const char c=text_[i_];
        if (c=='{') {
            ++i_; Value v; v.type=Value::Object; white();
            if (i_<text_.size() && text_[i_]=='}') { ++i_; return v; }
            for (;;) {
                white(); Value k=read(depth+1);
                if (k.type!=Value::String) throw std::runtime_error("JSON key is not a string");
                expect(':'); v.object[std::move(k.string)]=read(depth+1);
                white(); const char d=take();
                if (d=='}') break;
                if (d!=',') throw std::runtime_error("invalid JSON object");
            }
            return v;
        }
        if (c=='[') {
            ++i_; Value v; v.type=Value::Array; white();
            if (i_<text_.size() && text_[i_]==']') { ++i_; return v; }
            for (;;) {
                v.array.push_back(read(depth+1));
                white(); const char d=take();
                if (d==']') break;
                if (d!=',') throw std::runtime_error("invalid JSON array");
            }
            return v;
        }
        if (c=='"') {
            ++i_; Value v; v.type=Value::String;
            while (i_<text_.size()) {
                const char d=take();
                if (d=='"') return v;
                if (d=='\\') {
                    const char e=take();
                    if (e=='u') {
                        if (i_+4>text_.size()) throw std::runtime_error("truncated unicode escape");
                        i_+=4; v.string.push_back('?');
                    } else if (e=='"' || e=='\\' || e=='/') v.string.push_back(e);
                    else if (e=='n') v.string.push_back('\n');
                    else if (e=='t') v.string.push_back('\t');
                    else throw std::runtime_error("unsupported JSON escape");
                } else v.string.push_back(d);
            }
            throw std::runtime_error("unterminated JSON string");
        }
        if (text_.compare(i_,4,"true")==0) { i_+=4; Value v; v.type=Value::Boolean; v.boolean=true; return v; }
        if (text_.compare(i_,5,"false")==0) { i_+=5; Value v; v.type=Value::Boolean; return v; }
        if (text_.compare(i_,4,"null")==0) { i_+=4; return {}; }
        const char *start=text_.c_str()+i_;
        char *end=nullptr;
        const double n=std::strtod(start,&end);
        if (end==start || !std::isfinite(n)) throw std::runtime_error("invalid JSON number");
        i_+=static_cast<std::size_t>(end-start);
        Value v; v.type=Value::Number; v.number=n; return v;
    }
};
Rgba rgba(const Value &v) {
    if (v.type!=Value::Array || v.array.size()!=4) throw std::runtime_error("RGBA needs four channels");
    Rgba c{}; for (int i=0;i<4;++i) c[i]=v.array[i].integer(); return c;
}
}

bool load_definition_json(const std::string &json,std::map<std::uint32_t,Definition> &out,
                          std::string &error) {
    try {
        if (json.size()>4*1024*1024) throw std::runtime_error("definition JSON exceeds 4 MiB");
        const Value root=Parser(json).parse();
        if (root.type!=Value::Array || root.array.size()>4096)
            throw std::runtime_error("expected bounded definition array");
        std::map<std::uint32_t,Definition> parsed;
        for (const Value &v:root.array) {
            if (v.type!=Value::Object || v.at("complete").type!=Value::Boolean ||
                !v.at("complete").boolean) throw std::runtime_error("partial definition");
            const int addr=v.at("def").integer();
            if (addr<=0) throw std::runtime_error("invalid definition address");
            if (v.at("callback").integer()!=0) throw std::runtime_error("nonzero callback");
            const Definition d=Definition::build(v.at("size_start").real(),v.at("size_end").real(),
                v.at("gravity").real(),rgba(v.at("start_rgba")),rgba(v.at("end_rgba")),
                v.at("life_class").integer(),v.at("kind").integer(),v.at("tile").integer(),
                static_cast<std::uint32_t>(addr));
            parsed[d.address]=d; // a later particleDef_set supersedes its earlier ctor.
        }
        out=std::move(parsed); error.clear(); return true;
    } catch (const std::exception &e) { error=e.what(); return false; }
}
bool System::load_json(const std::string &json,std::string &error) {
    std::map<std::uint32_t,Definition> parsed;
    if (!load_definition_json(json,parsed,error)) return false;
    reset();
    definitions_=std::move(parsed);
    return true;
}
} // namespace champions::rta::fx
