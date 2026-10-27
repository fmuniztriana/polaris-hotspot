#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <cstring>
#include <cstdlib>

struct BoardInfo {
    std::wstring name=L"N/A", units=L"N/A", revision=L"N/A", bios=L"N/A", sid=L"N/A", quality=L"N/A";
    unsigned acquisitions=0;
    unsigned qualityRaw=0;
    std::string error;
    std::wstring memoryVendor=L"N/A",memoryModel=L"N/A",memorySize=L"N/A";
    unsigned long long memoryBytes=0;
    unsigned memoryModule=0;
    unsigned memoryAcquisitions=0;
    std::string memoryError;
};
inline std::wstring memoryVendorName(unsigned id){
    switch(id&15){case 1:return L"Samsung";case 2:return L"Infineon";case 3:return L"Elpida";case 4:return L"Etron";case 5:return L"Nanya";case 6:return L"SK hynix";case 7:return L"Mosel";case 8:return L"Winbond";case 9:return L"ESMT";case 15:return L"Micron";default:return L"N/A";}
}
inline std::wstring biosFromDriver(const std::wstring& path, unsigned bus,BoardInfo& board) {
    if(path.empty()) return L"N/A";
    HMODULE lib=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!lib)return L"N/A";
    auto create=reinterpret_cast<int(__cdecl*)(void*(__cdecl*)(int),int,void**)>(GetProcAddress(lib,"ADL2_Main_Control_Create"));
    auto destroy=reinterpret_cast<int(__cdecl*)(void*)>(GetProcAddress(lib,"ADL2_Main_Control_Destroy"));
    auto number=reinterpret_cast<int(__cdecl*)(void*,int*)>(GetProcAddress(lib,"ADL2_Adapter_NumberOfAdapters_Get"));
    auto info=reinterpret_cast<int(__cdecl*)(void*,void*,int)>(GetProcAddress(lib,"ADL2_Adapter_AdapterInfo_Get"));
    auto bios=reinterpret_cast<int(__cdecl*)(void*,int,void*)>(GetProcAddress(lib,"ADL2_Adapter_VideoBiosInfo_Get"));
    using Memory=int(__cdecl*)(void*,int,void*);
    auto memory4=reinterpret_cast<Memory>(GetProcAddress(lib,"ADL2_Adapter_MemoryInfoX4_Get"));
    auto memory3=reinterpret_cast<Memory>(GetProcAddress(lib,"ADL2_Adapter_MemoryInfo3_Get"));
    auto memory1=reinterpret_cast<Memory>(GetProcAddress(lib,"ADL2_Adapter_MemoryInfo_Get"));
    void* context=nullptr;std::wstring result=L"N/A";
    if(create&&destroy&&number&&info&&bios&&create([](int n)->void*{return std::malloc(n);},1,&context)==0){
        int count=0;
        if(number(context,&count)==0&&count>0&&count<=32){
            std::vector<unsigned char> data(static_cast<size_t>(count)*1572);
            for(int i=0;i<count;i++){int size=1572;std::memcpy(data.data()+i*1572,&size,4);}
            if(info(context,data.data(),static_cast<int>(data.size()))==0)for(int i=0;i<count;i++){
                int index=-1,pciBus=-1;std::memcpy(&index,data.data()+i*1572+4,4);std::memcpy(&pciBus,data.data()+i*1572+264,4);
                if(pciBus!=static_cast<int>(bus))continue;
                alignas(8) unsigned char memory[320]{};
                bool extended=(memory4&&memory4(context,index,memory)==0)||(memory3&&memory3(context,index,memory)==0);
                if(extended||(memory1&&memory1(context,index,memory)==0)){
                    unsigned long long bytes=0;std::memcpy(&bytes,memory,8);
                    if(bytes>0&&bytes<128ULL*1024*1024*1024){board.memoryBytes=bytes;wchar_t size[32];swprintf_s(size,L"%.0f GiB",bytes/(1024.0*1024*1024));board.memorySize=size;}
                    if(extended){unsigned long long vendor=0;std::memcpy(&vendor,memory+296,8);if(vendor&&vendor!=~0ULL)board.memoryVendor=memoryVendorName(static_cast<unsigned>(vendor));}
                }
                char values[768]{};
                if(bios(context,index,values)==0){auto p=values+256;size_t length=strnlen_s(p,256);if(length&&length<256){result.assign(p,p+length);break;}}
            }
        }
        destroy(context);
    }
    FreeLibrary(lib);return result;
}
