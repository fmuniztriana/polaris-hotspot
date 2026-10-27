#pragma once
#include <vector>
#include <string>
#include <stdexcept>
#include <cstdint>

namespace polaris {
struct MemoryModule { unsigned vendor; std::string part; };
// ATOM VRAM_Info 2.2 / module v8. Reject other layouts rather than guessing.
inline MemoryModule memory_module(const std::vector<unsigned char>& rom,unsigned module){
    auto byte=[&](size_t p)->unsigned{if(p>=rom.size())throw std::runtime_error("VBIOS offset out of bounds");return rom[p];};
    auto word=[&](size_t p)->unsigned{return byte(p)|(byte(p+1)<<8);};
    if(word(0)!=0xAA55)throw std::runtime_error("VBIOS signature unavailable");
    size_t header=word(0x48);
    if(byte(header+4)!='A'||byte(header+5)!='T'||byte(header+6)!='O'||byte(header+7)!='M')throw std::runtime_error("ATOM header unavailable");
    size_t master=word(header+32);if(!master||word(master)<4+29*2)throw std::runtime_error("VBIOS master table incomplete: header="+std::to_string(header)+" master="+std::to_string(master)+" size="+std::to_string(word(master)));
    size_t table=word(master+4+28*2);if(!table)throw std::runtime_error("Memory table unavailable");
    size_t end=table+word(table);if(end>rom.size()||end<table+20)throw std::runtime_error("Memory table truncated");
    if(byte(table+2)!=2||byte(table+3)!=2||byte(table+18)!=8)throw std::runtime_error("Unsupported memory table layout");
    unsigned count=byte(table+16);if(module>=count)throw std::runtime_error("Active memory module out of range");
    size_t pos=table+20;
    for(unsigned i=0;i<=module;i++){
        if(pos+45>end)throw std::runtime_error("Memory module truncated");
        unsigned size=word(pos+4);if(size<45||pos+size>end)throw std::runtime_error("Invalid memory module size");
        if(i==module){
            std::string part;
            for(unsigned j=0;j<20&&j<size-44;j++){unsigned c=byte(pos+44+j);if(!c)break;if(c<32||c>126)throw std::runtime_error("Invalid memory part string");part.push_back(static_cast<char>(c));}
            while(!part.empty()&&part.back()==' ')part.pop_back();
            return {byte(pos+28)&15,part};
        }
        pos+=size;
    }
    throw std::runtime_error("Memory module unavailable");
}
}
