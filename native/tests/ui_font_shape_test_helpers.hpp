#pragma once
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <cstdint>
#include <stdexcept>
inline int failures=0, checks=0;
inline void check(bool v,const char *what) { ++checks; if(!v){++failures; std::cerr<<what<<'\n';} }
inline std::vector<std::string> split(const std::string &s,char sep) {
    std::vector<std::string> out; std::size_t start=0;
    for(;;){auto end=s.find(sep,start);out.push_back(s.substr(start,end-start));if(end==std::string::npos)break;start=end+1;}return out;
}
inline std::vector<std::uint8_t> unhex(const std::string &s) {
    std::vector<std::uint8_t> v;for(std::size_t i=0;i<s.size();i+=2)v.push_back(static_cast<std::uint8_t>(std::stoul(s.substr(i,2),nullptr,16)));return v;
}
inline std::string hex(const std::vector<std::uint8_t> &v) {
    const char *digits="0123456789abcdef";std::string s;for(auto b:v){s+=digits[b>>4];s+=digits[b&15];}return s;
}
template<class T> std::string records(const T &rows) {
    std::string s;for(const auto &row:rows){if(!s.empty())s+=';';bool first=true;for(auto v:row){if(!first)s+=',';first=false;s+=std::to_string(v);}}return s;
}
inline void put32(std::vector<std::uint8_t> &b,std::size_t o,std::uint32_t v){for(unsigned i=0;i<4;++i)b[o+i]=static_cast<std::uint8_t>(v>>(8*i));}
template<class F> void refuses(F f,const char *label){bool refused=false;try{f();}catch(const champions::FormatError &){refused=true;}check(refused,label);}
inline std::ifstream vectors(){std::ifstream f(UI_FONT_SHAPE_VECTORS);if(!f)throw std::runtime_error("Missing pinned synthetic vectors");return f;}
