#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <cstring>
#include <cstdlib>

struct BoardInfo {
    std::wstring name=L"N/A", units=L"N/A", revision=L"N/A", bios=L"N/A", sid=L"N/A";
    unsigned acquisitions=0;
    std::string error;
};
inline std::wstring biosFromDriver(const std::wstring& path, unsigned bus) {
    if(path.empty()) return L"N/A";
    HMODULE lib=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!lib)return L"N/A";
    auto create=reinterpret_cast<int(__cdecl*)(void*(__cdecl*)(int),int,void**)>(GetProcAddress(lib,"ADL2_Main_Control_Create"));
    auto destroy=reinterpret_cast<int(__cdecl*)(void*)>(GetProcAddress(lib,"ADL2_Main_Control_Destroy"));
    auto number=reinterpret_cast<int(__cdecl*)(void*,int*)>(GetProcAddress(lib,"ADL2_Adapter_NumberOfAdapters_Get"));
    auto info=reinterpret_cast<int(__cdecl*)(void*,void*,int)>(GetProcAddress(lib,"ADL2_Adapter_AdapterInfo_Get"));
    auto bios=reinterpret_cast<int(__cdecl*)(void*,int,void*)>(GetProcAddress(lib,"ADL2_Adapter_VideoBiosInfo_Get"));
    void* context=nullptr;std::wstring result=L"N/A";
    if(create&&destroy&&number&&info&&bios&&create([](int n)->void*{return std::malloc(n);},1,&context)==0){
        int count=0;
        if(number(context,&count)==0&&count>0&&count<=32){
            std::vector<unsigned char> data(static_cast<size_t>(count)*1572);
            for(int i=0;i<count;i++){int size=1572;std::memcpy(data.data()+i*1572,&size,4);}
            if(info(context,data.data(),static_cast<int>(data.size()))==0)for(int i=0;i<count;i++){
                int index=-1,pciBus=-1;std::memcpy(&index,data.data()+i*1572+4,4);std::memcpy(&pciBus,data.data()+i*1572+264,4);
                if(pciBus!=static_cast<int>(bus))continue;
                char values[768]{};
                if(bios(context,index,values)==0){auto p=values+256;size_t length=strnlen_s(p,256);if(length&&length<256){result.assign(p,p+length);break;}}
            }
        }
        destroy(context);
    }
    FreeLibrary(lib);return result;
}
