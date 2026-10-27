#pragma once
#include <windows.h>
#include <bcrypt.h>
#include <string>
#include <vector>
#include <fstream>
#include <cstring>
#include <cstdint>
#include <cstdio>

namespace service_guard {
constexpr wchar_t serviceName[]=L"PolarisHotspotRTCore";

// The validated driver files from Afterburner 4.6.5, 4.6.6 and 4.6.7 Beta 2.
inline bool compatible(const std::wstring& path) {
    std::ifstream input(path,std::ios::binary|std::ios::ate);
    if(!input||input.tellg()<=0||input.tellg()>1024*1024)return false;
    input.seekg(0);std::string bytes((std::istreambuf_iterator<char>(input)),{});
    if(bytes.empty())return false;
    unsigned char digest[32]{};
    if(BCryptHash(BCRYPT_SHA256_ALG_HANDLE,nullptr,0,reinterpret_cast<PUCHAR>(&bytes[0]),static_cast<ULONG>(bytes.size()),digest,32)!=0)return false;
    const unsigned char allowed[][32]={
        {0xf1,0xc8,0xca,0x23,0x27,0x89,0xc2,0xf1,0x1a,0x51,0x1c,0x8c,0xd9,0x5a,0x9f,0x38,0x30,0xdd,0x71,0x9c,0xad,0x5a,0xa2,0x2c,0xb7,0xc3,0x53,0x9a,0xb8,0xcb,0x4d,0xc3},
        {0xa4,0xf4,0x4e,0x26,0x76,0x98,0xd4,0x7f,0x6a,0x90,0x5e,0x96,0xb3,0x56,0x58,0x2b,0x4e,0xe9,0xcf,0x40,0x49,0xe8,0xc7,0x92,0xff,0xda,0x1b,0x73,0x56,0xe6,0x8d,0x35}
    };
    for(const auto& hash:allowed)if(std::memcmp(digest,hash,32)==0)return true;
    return false;
}

inline void recordFailure(const char* operation,DWORD error) {
    wchar_t directory[MAX_PATH]{};
    if(!GetTempPathW(MAX_PATH,directory))return;
    std::wstring path=std::wstring(directory)+L"PolarisHotspot-cleanup.log";
    FILE* f=nullptr;
    if(_wfopen_s(&f,path.c_str(),L"a")==0){std::fprintf(f,"%s: Windows error %lu\n",operation,error);std::fclose(f);}
}

inline bool requestStop(SC_HANDLE service) {
    SERVICE_STATUS status{};
    if(!QueryServiceStatus(service,&status))return false;
    if(status.dwCurrentState==SERVICE_STOPPED||status.dwCurrentState==SERVICE_STOP_PENDING)return true;
    return ControlService(service,SERVICE_CONTROL_STOP,&status)||GetLastError()==ERROR_SERVICE_NOT_ACTIVE;
}

// Recover only this application's stopped temporary service with an approved driver.
inline bool removeStale(SC_HANDLE manager) {
    SC_HANDLE old=OpenServiceW(manager,serviceName,SERVICE_QUERY_STATUS|SERVICE_QUERY_CONFIG|DELETE);
    if(!old)return false;
    SERVICE_STATUS state{};DWORD needed=0;
    QueryServiceConfigW(old,nullptr,0,&needed);
    std::vector<unsigned char> buffer(needed);
    auto config=reinterpret_cast<QUERY_SERVICE_CONFIGW*>(buffer.data());
    bool safe=needed&&QueryServiceStatus(old,&state)&&state.dwCurrentState==SERVICE_STOPPED&&
        QueryServiceConfigW(old,config,needed,&needed)&&config->dwServiceType==SERVICE_KERNEL_DRIVER&&config->lpBinaryPathName;
    if(safe){
        std::wstring path=config->lpBinaryPathName;
        if(path.compare(0,4,L"\\??\\")==0)path.erase(0,4);
        if(path.size()>1&&path.front()==L'"'&&path.back()==L'"')path=path.substr(1,path.size()-2);
        safe=compatible(path);
    }
    bool ok=safe&&DeleteService(old);
    CloseServiceHandle(old);return ok;
}

// This child owns only a service that it created. Inherited process handle pins
// the actual parent, avoiding PID-reuse races after an abnormal termination.
inline int run(HANDLE parent,HANDLE gate,HANDLE ready,HANDLE success,const std::wstring& path) {
    SC_HANDLE manager=OpenSCManagerW(nullptr,nullptr,SC_MANAGER_CONNECT|SC_MANAGER_CREATE_SERVICE);
    SC_HANDLE service=nullptr;DWORD failure=ERROR_SUCCESS;
    if(!manager)failure=GetLastError();
    if(manager){
        for(int attempt=0;attempt<30;attempt++){
            service=CreateServiceW(manager,serviceName,L"Polaris Hotspot register access",SERVICE_START|SERVICE_STOP|SERVICE_QUERY_STATUS|DELETE,SERVICE_KERNEL_DRIVER,SERVICE_DEMAND_START,SERVICE_ERROR_NORMAL,path.c_str(),nullptr,nullptr,nullptr,nullptr,nullptr);
            if(service)break;
            failure=GetLastError();
            if(failure==ERROR_SERVICE_EXISTS){if(!removeStale(manager))break;}
            else if(failure!=ERROR_SERVICE_MARKED_FOR_DELETE)break;
            Sleep(100);
        }
        if(!service && failure==ERROR_SUCCESS)failure=ERROR_SERVICE_EXISTS;
    }
    bool started=service&&StartServiceW(service,0,nullptr);
    if(service&&!started)failure=GetLastError();
    if(started)SetEvent(success);
    SetEvent(ready);
    if(started)WaitForSingleObject(parent,INFINITE);
    if(service){
        if(!requestStop(service))recordFailure("StopService",GetLastError());
        else if(!DeleteService(service)&&GetLastError()!=ERROR_SERVICE_MARKED_FOR_DELETE)recordFailure("DeleteService",GetLastError());
        CloseServiceHandle(service);
    }
    if(manager)CloseServiceHandle(manager);
    CloseHandle(parent);CloseHandle(gate);CloseHandle(ready);CloseHandle(success);
    return started?0:static_cast<int>(failure?failure:ERROR_GEN_FAILURE);
}

// Starts a hidden copy of this executable; only four explicit handles are inherited.
inline bool launch(HANDLE gate,const std::wstring& path) {
    SECURITY_ATTRIBUTES security{sizeof(security),nullptr,TRUE};
    HANDLE parent=nullptr;
    if(!DuplicateHandle(GetCurrentProcess(),GetCurrentProcess(),GetCurrentProcess(),&parent,0,TRUE,DUPLICATE_SAME_ACCESS))return false;
    HANDLE ready=CreateEventW(&security,TRUE,FALSE,nullptr),success=CreateEventW(&security,TRUE,FALSE,nullptr);
    if(!ready||!success){if(ready)CloseHandle(ready);if(success)CloseHandle(success);CloseHandle(parent);return false;}
    wchar_t exe[MAX_PATH]{};DWORD n=GetModuleFileNameW(nullptr,exe,MAX_PATH);
    bool ok=n>0&&n<MAX_PATH;
    std::wstring command;
    if(ok)command=L"\""+std::wstring(exe)+L"\" --service-guardian "+std::to_wstring(reinterpret_cast<std::uintptr_t>(parent))+L" "+std::to_wstring(reinterpret_cast<std::uintptr_t>(gate))+L" "+std::to_wstring(reinterpret_cast<std::uintptr_t>(ready))+L" "+std::to_wstring(reinterpret_cast<std::uintptr_t>(success))+L" \""+path+L"\"";
    SIZE_T bytes=0;InitializeProcThreadAttributeList(nullptr,1,0,&bytes);
    std::vector<unsigned char> storage(bytes);
    auto attrs=reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
    bool attrsReady=ok&&InitializeProcThreadAttributeList(attrs,1,0,&bytes);
    HANDLE inherited[]={parent,gate,ready,success};
    if(attrsReady)ok=UpdateProcThreadAttribute(attrs,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,inherited,sizeof inherited,nullptr,nullptr)!=FALSE;else ok=false;
    STARTUPINFOEXW si{};si.StartupInfo.cb=sizeof si;si.StartupInfo.dwFlags=STARTF_USESHOWWINDOW;si.StartupInfo.wShowWindow=SW_HIDE;si.lpAttributeList=attrs;
    PROCESS_INFORMATION pi{};
    if(ok){std::vector<wchar_t> args(command.begin(),command.end());args.push_back(0);ok=CreateProcessW(exe,args.data(),nullptr,nullptr,TRUE,EXTENDED_STARTUPINFO_PRESENT|CREATE_NO_WINDOW,nullptr,nullptr,&si.StartupInfo,&pi)!=FALSE;}
    if(attrsReady)DeleteProcThreadAttributeList(attrs);
    if(ok){ok=WaitForSingleObject(ready,15000)==WAIT_OBJECT_0&&WaitForSingleObject(success,0)==WAIT_OBJECT_0;CloseHandle(pi.hThread);CloseHandle(pi.hProcess);}
    CloseHandle(parent);CloseHandle(ready);CloseHandle(success);return ok;
}
}
