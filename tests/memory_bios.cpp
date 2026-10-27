#include "polaris_memory_bios.hpp"
#include <cassert>
#include <fstream>
#include <cstdio>
int main(int argc,char** argv){
    if(argc==2){std::ifstream f(argv[1],std::ios::binary);std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(f)),{});auto m=polaris::memory_module(bytes,0);std::printf("vendor=%u model=%s\n",m.vendor,m.part.c_str());return 0;}
    std::vector<unsigned char> rom(1024);
    auto word=[&](size_t p,unsigned v){rom[p]=static_cast<unsigned char>(v);rom[p+1]=static_cast<unsigned char>(v>>8);};
    word(0,0xAA55);word(0x48,0x80);rom[0x84]='A';rom[0x85]='T';rom[0x86]='O';rom[0x87]='M';word(0xA0,0x100);word(0x100,64);word(0x13C,0x200);
    word(0x200,148);rom[0x202]=2;rom[0x203]=2;rom[0x210]=2;rom[0x212]=8;
    word(0x218,64);word(0x258,64);rom[0x230]=1;rom[0x270]=6;
    const char a[]="PART-A",b[]="PART-B";for(unsigned i=0;i<sizeof a;i++)rom[0x240+i]=a[i];for(unsigned i=0;i<sizeof b;i++)rom[0x280+i]=b[i];
    auto selected=polaris::memory_module(rom,1);assert(selected.part=="PART-B"&&selected.vendor==6);
    word(0x218,51);assert(polaris::memory_module(rom,0).part=="PART-A");word(0x218,64);
    bool rejected=false;try{polaris::memory_module(rom,2);}catch(...){rejected=true;}assert(rejected);
    word(0x258,65535);rejected=false;try{polaris::memory_module(rom,1);}catch(...){rejected=true;}assert(rejected);
    return 0;
}
