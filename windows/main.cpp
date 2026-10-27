#define UNICODE
#define _UNICODE
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <setupapi.h>
#include <devguid.h>
#include <shellapi.h>
#include <bcrypt.h>
#include <array>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include "polaris_hotspot.hpp"
#include "polaris_devices.hpp"
#include "BoardInfo.hpp"
#include "polaris_memory_bios.hpp"
#include "polaris_slt.hpp"

static void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
class Reader {
    static constexpr DWORD grbmIndex=0xC200u*4; // Register index -> BAR5 byte offset.
    HANDLE device = INVALID_HANDLE_VALUE, mutex = nullptr;
    SC_HANDLE manager = nullptr, service = nullptr;
    std::array<unsigned char,48> mapping{};
    bool mapped = false;
    DWORD bus = 0, slot = 0, function = 0;
    std::wstring adlPath;
    static void put(unsigned char* p, unsigned offset, DWORD v) { std::memcpy(p+offset,&v,4); }
    static DWORD get(const unsigned char* p, unsigned offset) { DWORD v; std::memcpy(&v,p+offset,4); return v; }
    void io(DWORD code, unsigned char* data, DWORD size) {
        DWORD bytes=0;
        check(DeviceIoControl(device,code,data,size,data,size,&bytes,nullptr) && bytes==size,"Register transport failed");
    }
    DWORD pci(DWORD offset) {
        std::array<unsigned char,24> b{};
        put(b.data(),0,bus); put(b.data(),4,slot); put(b.data(),8,function);
        put(b.data(),12,offset); put(b.data(),16,4); io(0x80002050,b.data(),24); return get(b.data(),20);
    }
    void locate() {
        HDEVINFO list=SetupDiGetClassDevsW(&GUID_DEVCLASS_DISPLAY,nullptr,nullptr,DIGCF_PRESENT);
        check(list!=INVALID_HANDLE_VALUE,"GPU enumeration failed");
        unsigned matches=0; SP_DEVINFO_DATA item{}; item.cbSize=sizeof item;
        for (DWORD i=0; SetupDiEnumDeviceInfo(list,i,&item); ++i) {
            wchar_t id[512]{};
            if (!SetupDiGetDeviceInstanceIdW(list,&item,id,512,nullptr)) continue;
            unsigned vendor=0, deviceId=0;
            if(swscanf_s(id,L"PCI\\VEN_%4x&DEV_%4x",&vendor,&deviceId)!=2 || !polaris::accepts_device(vendor,deviceId)) continue;
            DWORD address=0, candidate=0;
            if (!SetupDiGetDeviceRegistryPropertyW(list,&item,SPDRP_BUSNUMBER,nullptr,reinterpret_cast<BYTE*>(&candidate),4,nullptr) ||
                !SetupDiGetDeviceRegistryPropertyW(list,&item,SPDRP_ADDRESS,nullptr,reinterpret_cast<BYTE*>(&address),4,nullptr)) continue;
            bus=candidate; slot=address>>16; function=address&0xffff; ++matches;
            wchar_t description[512]{};
            if(SetupDiGetDeviceRegistryPropertyW(list,&item,SPDRP_DEVICEDESC,nullptr,reinterpret_cast<BYTE*>(description),sizeof description,nullptr)) board.name=description;
            wchar_t driverKey[512]{};
            if(SetupDiGetDeviceRegistryPropertyW(list,&item,SPDRP_DRIVER,nullptr,reinterpret_cast<BYTE*>(driverKey),sizeof driverKey,nullptr)){
                HKEY key=nullptr;auto path=std::wstring(L"SYSTEM\\CurrentControlSet\\Control\\Class\\")+driverKey;
                if(RegOpenKeyExW(HKEY_LOCAL_MACHINE,path.c_str(),0,KEY_READ,&key)==ERROR_SUCCESS){
                    wchar_t modules[32768]{};DWORD bytes=sizeof modules;
                    if(RegGetValueW(key,nullptr,L"UserModeDriverName",RRF_RT_REG_MULTI_SZ|RRF_RT_REG_SZ,nullptr,modules,&bytes)==ERROR_SUCCESS){std::wstring module=modules;auto slash=module.find_last_of(L"\\/");if(slash!=std::wstring::npos)adlPath=module.substr(0,slash+1)+L"atiadlxx.dll";}
                    RegCloseKey(key);
                }
            }
        }
        SetupDiDestroyDeviceInfoList(list);
        check(matches==1,"Exactly one Polaris GPU is required");
    }
    void openDevice() { device=CreateFileW(L"\\\\.\\RTCore64",GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr); }
    static std::wstring regString(HKEY key,const wchar_t* name) {
        wchar_t value[32768]{}; DWORD bytes=sizeof value;
        if(RegGetValueW(key,nullptr,name,RRF_RT_REG_SZ,nullptr,value,&bytes)!=ERROR_SUCCESS) return {};
        return value;
    }
    static void addDirectory(std::vector<std::wstring>& paths,std::wstring folder) {
        if(folder.empty()) return;
        if(folder.back()!=L'\\' && folder.back()!=L'/') folder+=L'\\';
        paths.push_back(folder+L"RTCore64.sys");
    }
    static std::vector<std::wstring> driverPaths() {
        std::vector<std::wstring> paths; wchar_t filename[MAX_PATH]{};
        DWORD length=GetModuleFileNameW(nullptr,filename,MAX_PATH);
        check(length>0 && length<MAX_PATH,"Executable path unavailable");
        std::wstring folder=filename; addDirectory(paths,folder.substr(0,folder.find_last_of(L"\\/")));
        for(REGSAM view:{KEY_WOW64_32KEY,KEY_WOW64_64KEY}) {
            HKEY root=nullptr;
            if(RegOpenKeyExW(HKEY_LOCAL_MACHINE,L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall",0,KEY_READ|view,&root)!=ERROR_SUCCESS) continue;
            for(DWORD i=0;;++i) {
                wchar_t name[256]{}; DWORD size=256;
                LONG status=RegEnumKeyExW(root,i,name,&size,nullptr,nullptr,nullptr,nullptr);
                if(status==ERROR_NO_MORE_ITEMS) break;
                if(status!=ERROR_SUCCESS) continue;
                HKEY key=nullptr; if(RegOpenKeyExW(root,name,0,KEY_READ|view,&key)!=ERROR_SUCCESS) continue;
                if(regString(key,L"DisplayName").find(L"MSI Afterburner")==0) {
                    addDirectory(paths,regString(key,L"InstallLocation"));
                    auto command=regString(key,L"UninstallString");
                    if(!command.empty()) {
                        int argc=0; auto argv=CommandLineToArgvW(command.c_str(),&argc);
                        if(argv && argc>0) { std::wstring exe=argv[0]; auto slash=exe.find_last_of(L"\\/"); if(slash!=std::wstring::npos) addDirectory(paths,exe.substr(0,slash)); }
                        if(argv) LocalFree(argv);
                    }
                }
                RegCloseKey(key);
            }
            RegCloseKey(root);
        }
        for(auto variable:{L"ProgramFiles(x86)",L"ProgramW6432",L"ProgramFiles"}) {
            wchar_t value[MAX_PATH]{}; DWORD n=GetEnvironmentVariableW(variable,value,MAX_PATH);
            if(n>0 && n<MAX_PATH) addDirectory(paths,std::wstring(value)+L"\\MSI Afterburner");
        }
        return paths;
    }
    static bool compatible(const std::wstring& path) {
        std::ifstream input(path,std::ios::binary|std::ios::ate);
        if(!input || input.tellg()<=0 || input.tellg()>1024*1024) return false;
        input.seekg(0);
        std::string bytes((std::istreambuf_iterator<char>(input)),{});
        if(bytes.empty()) return false;
        unsigned char digest[32]{};
        if(BCryptHash(BCRYPT_SHA256_ALG_HANDLE,nullptr,0,reinterpret_cast<PUCHAR>(&bytes[0]),static_cast<ULONG>(bytes.size()),digest,32)!=0) return false;
        const unsigned char expected[32]={0xf1,0xc8,0xca,0x23,0x27,0x89,0xc2,0xf1,0x1a,0x51,0x1c,0x8c,0xd9,0x5a,0x9f,0x38,0x30,0xdd,0x71,0x9c,0xad,0x5a,0xa2,0x2c,0xb7,0xc3,0x53,0x9a,0xb8,0xcb,0x4d,0xc3};
        return std::memcmp(digest,expected,32)==0;
    }
    void startDriver() {
        std::wstring path;
        for(const auto& candidate:driverPaths()) if(compatible(candidate)) { path=candidate; break; }
        check(!path.empty(),"Compatible RTCore64.sys not found in MSI Afterburner or beside this EXE");
        manager=OpenSCManagerW(nullptr,nullptr,SC_MANAGER_CREATE_SERVICE);
        check(manager!=nullptr,"Administrator access required");
        service=CreateServiceW(manager,L"PolarisHotspotRTCore",L"Polaris Hotspot register access",SERVICE_START|SERVICE_STOP|DELETE|SERVICE_QUERY_STATUS,SERVICE_KERNEL_DRIVER,SERVICE_DEMAND_START,SERVICE_ERROR_NORMAL,path.c_str(),nullptr,nullptr,nullptr,nullptr,nullptr);
        check(service!=nullptr,"Cannot create temporary driver service; check for another instance");
        check(StartServiceW(service,0,nullptr)!=FALSE,"Windows could not load the register driver");
        openDevice(); check(device!=INVALID_HANDLE_VALUE,"Driver device unavailable");
    }
public:
    BoardInfo board;
    DWORD identity=0;
    bool tested=false;
    void open() {
        locate(); openDevice(); if (device==INVALID_HANDLE_VALUE) startDriver();
        identity=pci(0);
        check(polaris::accepts_device(identity&0xFFFF,identity>>16) && (pci(4)&2),"GPU identity or memory decoding invalid");
        tested=polaris::tested_profile(identity>>16,pci(0x2c),pci(8)&0xFF);
        wchar_t rev[8];swprintf_s(rev,L"%02X",pci(8)&0xFF);board.revision=rev;
        board.bios=biosFromDriver(adlPath,bus,board);
        DWORD bar=pci(0x24); check(bar && bar!=0xffffffff && (bar&15)==0,"Unsupported BAR5");
        std::uint64_t physical=bar; std::memcpy(mapping.data(),&physical,8);
        put(mapping.data(),16,0x40000); put(mapping.data(),32,bus); put(mapping.data(),36,slot); put(mapping.data(),40,function); put(mapping.data(),44,5);
        io(0x80002040,mapping.data(),48); mapped=true;
        mutex=CreateMutexW(nullptr,FALSE,L"Global\\Access_PCI"); check(mutex!=nullptr,"PCI mutex unavailable");
        identifyOnce();
        identifyMemoryOnce();
    }
    void identifyMemoryOnce(){
        check(board.memoryAcquisitions++==0,"Memory identification must run once");
        DWORD wait=WaitForSingleObject(mutex,200);if(wait!=WAIT_OBJECT_0&&wait!=WAIT_ABANDONED){board.memoryError="PCI access busy";return;}
        DWORD original=0,romIndex=0;bool selectorSaved=false,indexSaved=false;
        auto restore=[&](){if(indexSaved){write(0x210,0xC0600010);check(read(0x210)==0xC0600010,"ROM selector mismatch");write(0x214,romIndex);check(read(0x214)==romIndex,"ROM index restore failed");}if(selectorSaved){write(0x210,original);check(read(0x210)==original,"ROM selector restore failed");}};
        try{
            board.memoryModule=(read(0x1734)>>16)&255;
            original=read(0x210);check(original!=0xFFFFFFFF,"ROM selector unavailable");selectorSaved=true;
            write(0x210,0xC0600010);check(read(0x210)==0xC0600010,"ROM selector mismatch");romIndex=read(0x214);check(romIndex!=0xFFFFFFFF,"ROM index unavailable");indexSaved=true;
            write(0x214,0);write(0x210,0xC0600014);check(read(0x210)==0xC0600014,"ROM data selector mismatch");
            std::vector<unsigned char> rom(65536);auto start=GetTickCount64();
            for(size_t p=0;p<rom.size();p+=4){DWORD value=read(0x214);std::memcpy(rom.data()+p,&value,4);if(p==0)check((value&65535)==0xAA55,"ROM signature unavailable");if((p&1023)==0)check(GetTickCount64()-start<2000,"ROM read timed out");}
            check(read(0x210)==0xC0600014,"ROM selector changed during read");
            restore();selectorSaved=indexSaved=false;
            auto module=polaris::memory_module(rom,board.memoryModule);
            if(!module.part.empty())board.memoryModel=std::wstring(module.part.begin(),module.part.end())+L" (VBIOS)";
            auto vendor=memoryVendorName(module.vendor);
            if(board.memoryVendor==L"N/A" && vendor!=L"N/A")board.memoryVendor=vendor+L" (VBIOS)";
            ReleaseMutex(mutex);
        }catch(const std::exception& e){board.memoryError=e.what();try{restore();}catch(...){ReleaseMutex(mutex);throw;}ReleaseMutex(mutex);}
    }
    void identifyOnce(){
        check(board.acquisitions++==0,"Identification must run once");
        DWORD wait=WaitForSingleObject(mutex,200);check(wait==WAIT_OBJECT_0||wait==WAIT_ABANDONED,"PCI access busy");
        DWORD smu=0,grbm=0;bool saved=false;
        auto restore=[&](){write(grbmIndex,grbm);write(0x210,smu);check(read(grbmIndex)==grbm&&read(0x210)==smu,"Identification selector restoration failed");};
        try{
            smu=read(0x210);grbm=read(grbmIndex);check(smu!=0xFFFFFFFF&&grbm!=0xFFFFFFFF,"Identification selectors unavailable");saved=true;
            unsigned words[19]{};
            for(unsigned word:{12u,13u,14u,16u,17u,18u}){
                DWORD target=0xC0100000+word*4;write(0x210,target);check(read(0x210)==target,"SID selector mismatch");words[word]=read(0x214);check(read(0x210)==target,"SID selector changed");
            }
            auto bits=[&](unsigned first,unsigned last){unsigned long long value=0;for(unsigned b=first;b<=last;b++)value|=static_cast<unsigned long long>((words[b/32]>>(b%32))&1)<<(b-first);return value;};
            auto sid=bits(400,449),extended=bits(538,580);
            if(words[14]!=0 && words[14]!=0xFFFFFFFF){
                unsigned qualityRaw=(words[14]>>4)&0xFFFu;
                board.qualityRaw=qualityRaw;
                double quality=qualityRaw*0.25/1023.0*100.0;
                if(qualityRaw>0 && quality<=100.0){wchar_t text[32];swprintf_s(text,L"%.1f %%",quality);board.quality=text;}
            }
            if(sid&&sid!=0x3FFFFFFFFFFFFULL&&words[14]!=0xFFFFFFFF){
                wchar_t text[64];swprintf_s(text,L"%013llX / %011llX",sid,extended);board.sid=text;
                if(extended!=0x7FFFFFFFFFFULL){auto slt=polaris::sid_to_slt(sid,extended);board.slt.assign(slt.begin(),slt.end());}
            }
            unsigned dev=identity>>16;bool small=(dev&0xFF00)==0x6900;bool mid=dev>=0x67E0&&dev<=0x67FF;
            unsigned engines=(small||mid)?2:4, perEngine=small?5:(mid?8:9),total=0;
            for(unsigned se=0;se<engines;se++){
                DWORD selected=(grbm&0x5F000000)|0x40000000|(se<<16);write(grbmIndex,selected);check(read(grbmIndex)==selected,"CU selector mismatch");
                DWORD cc=read(0x89BC),user=read(0x89C0);check(cc!=0xFFFFFFFF&&user!=0xFFFFFFFF,"CU mask unavailable");
                unsigned active=(~((cc|user)>>16))&((1u<<perEngine)-1);for(;active;active>>=1)total+=active&1;
            }
            if(total){board.units=std::to_wstring(total)+L" CUs / "+std::to_wstring(total*64)+L" shaders";}
            restore();ReleaseMutex(mutex);
        }catch(const std::exception& e){board.error=e.what();board.units=L"N/A";board.sid=L"N/A";board.slt=L"N/A";board.quality=L"N/A";try{if(saved)restore();}catch(...){ReleaseMutex(mutex);throw;}ReleaseMutex(mutex);}
    }
    DWORD read(DWORD offset) {
        check(offset==0x210 || offset==0x214 || offset==grbmIndex || offset==0x89BC || offset==0x89C0 || offset==0x1734,"Unexpected read offset");
        auto b=mapping; put(b.data(),20,offset); put(b.data(),24,4); io(0x80002048,b.data(),48); return get(b.data(),28);
    }
    void write(DWORD offset,DWORD value) {
        check(offset==0x210 || offset==grbmIndex || offset==0x214,"Only register selectors may be written");
        if(offset==0x214)check(read(0x210)==0xC0600010,"Only ROM read index may be written");
        auto b=mapping; put(b.data(),20,offset); put(b.data(),24,4); put(b.data(),28,value); io(0x8000204c,b.data(),48);
    }
    DWORD sample() {
        DWORD wait=WaitForSingleObject(mutex,200);
        check(wait==WAIT_OBJECT_0 || wait==WAIT_ABANDONED,"PCI access busy");
        try {
            auto raw=polaris::read_thermal_word([this](DWORD p){return read(p);},[this](DWORD p,DWORD v){write(p,v);});
            check(polaris::decode_hotspot(raw)>0 && polaris::decode_hotspot(raw)<=150,"Invalid hotspot value");
            ReleaseMutex(mutex);
            return raw;
        } catch (...) { ReleaseMutex(mutex); throw; }
    }
    ~Reader() {
        if(mapped) { DWORD bytes=0; DeviceIoControl(device,0x80002044,mapping.data(),48,nullptr,0,&bytes,nullptr); }
        if(device!=INVALID_HANDLE_VALUE) CloseHandle(device);
        if(mutex) CloseHandle(mutex);
        if(service) { SERVICE_STATUS status{}; ControlService(service,SERVICE_CONTROL_STOP,&status); DeleteService(service); CloseServiceHandle(service); }
        if(manager) CloseServiceHandle(manager);
    }
};

static std::unique_ptr<Reader> reader;
static HWND valueLabel,coreLabel,deltaLabel,statusLabel;
static HWND advancedButton,details;
static HFONT detailsFont;
static bool expanded=false;
static std::string startupError;
static HFONT font;
static FILE* logFile=nullptr;
static unsigned sampleLimit=0, samples=0;
static int result=0;
static void acquire(HWND window) {
    try {
        if(!startupError.empty()) throw std::runtime_error(startupError);
        DWORD raw=reader->sample(); unsigned temp=polaris::decode_hotspot(raw);
        unsigned core=(raw>>9)&0x1FFu;
        check(core>0 && core<=150,"Invalid GPU temperature");
        int delta=static_cast<int>(temp)-static_cast<int>(core);
        wchar_t text[32]; swprintf_s(text,L"%u \x00B0" L"C",temp); SetWindowTextW(valueLabel,text);
        swprintf_s(text,L"%u \x00B0" L"C",core);SetWindowTextW(coreLabel,text);
        swprintf_s(text,L"%+d \x00B0" L"C",delta);SetWindowTextW(deltaLabel,text);
        SetWindowTextW(statusLabel,reader->tested?L"Tested profile":L"Untested profile");
        if(logFile) { fprintf(logFile,"%llu,0x%08lX,%u,true,%u,%d\n",GetTickCount64(),raw,temp,core,delta);fflush(logFile); }
        if(sampleLimit && ++samples>=sampleLimit) DestroyWindow(window);
    } catch(const std::exception& e) {
        KillTimer(window,1); SetWindowTextW(valueLabel,L"N/A");SetWindowTextW(coreLabel,L"N/A");SetWindowTextW(deltaLabel,L"N/A"); result=1;
        SetWindowTextW(statusLabel,L"Reading unavailable");
        if(logFile) { fprintf(logFile,"ERROR,%s\n",e.what()); fflush(logFile); }
        if(sampleLimit) DestroyWindow(window);
    }
}
static LRESULT CALLBACK windowProc(HWND window,UINT message,WPARAM w,LPARAM l) {
    switch(message) {
    case WM_CREATE:
        font=CreateFontW(-26,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
        for(int row=0;row<3;row++){
            const wchar_t* labels[]={L"GPU",L"Hotspot",L"Delta"};
            HWND label=CreateWindowW(L"STATIC",labels[row],WS_CHILD|WS_VISIBLE,16,16+row*44,100,34,window,nullptr,nullptr,nullptr);
            SendMessageW(label,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);
        }
        coreLabel=CreateWindowW(L"STATIC",L"--",WS_CHILD|WS_VISIBLE|SS_RIGHT,122,16,140,34,window,nullptr,nullptr,nullptr);
        valueLabel=CreateWindowW(L"STATIC",L"--",WS_CHILD|WS_VISIBLE|SS_RIGHT,122,60,140,34,window,nullptr,nullptr,nullptr);
        deltaLabel=CreateWindowW(L"STATIC",L"--",WS_CHILD|WS_VISIBLE|SS_RIGHT,122,104,140,34,window,nullptr,nullptr,nullptr);
        SendMessageW(coreLabel,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);
        SendMessageW(valueLabel,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);
        SendMessageW(deltaLabel,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);
        statusLabel=CreateWindowW(L"STATIC",L"",WS_CHILD|WS_VISIBLE|SS_CENTER,16,147,246,22,window,nullptr,nullptr,nullptr);
        advancedButton=CreateWindowW(L"BUTTON",L"Advanced \x25BE",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_FLAT,16,172,246,28,window,reinterpret_cast<HMENU>(200),nullptr,nullptr);
        detailsFont=CreateFontW(-14,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
        SendMessageW(advancedButton,WM_SETFONT,reinterpret_cast<WPARAM>(detailsFont),TRUE);
        {
            const auto& b=reader->board;
            std::wstring text=L"GPU name\r\n"+b.name+L"\r\n\r\nActive units   "+b.units+L"\r\nRevision         "+b.revision+L"\r\nASIC Quality  "+b.quality+L"\r\n\r\nMemory vendor   "+b.memoryVendor+L"\r\nMemory size        "+b.memorySize+L"\r\nMemory model\r\n"+b.memoryModel+L"\r\n\r\nVBIOS\r\n"+b.bios+L"\r\n\r\nSLT   "+b.slt+L"\r\nSID (primary / extended)\r\n"+b.sid;
            details=CreateWindowW(L"EDIT",text.c_str(),WS_CHILD|ES_MULTILINE|ES_READONLY,16,211,370,355,window,nullptr,nullptr,nullptr);
            SendMessageW(details,WM_SETFONT,reinterpret_cast<WPARAM>(detailsFont),TRUE);
        }
        SetTimer(window,1,1000,nullptr); PostMessageW(window,WM_APP,0,0); return 0;
    case WM_COMMAND:
        if(LOWORD(w)==200){
            expanded=!expanded;SetWindowTextW(advancedButton,expanded?L"Advanced \x25B4":L"Advanced \x25BE");
            ShowWindow(details,expanded?SW_SHOW:SW_HIDE);
            SetWindowPos(window,nullptr,0,0,expanded?420:296,expanded?620:247,SWP_NOMOVE|SWP_NOZORDER);
            int valueWidth=expanded?264:140,fullWidth=expanded?370:246;
            MoveWindow(coreLabel,122,16,valueWidth,34,TRUE);MoveWindow(valueLabel,122,60,valueWidth,34,TRUE);MoveWindow(deltaLabel,122,104,valueWidth,34,TRUE);
            MoveWindow(statusLabel,16,147,fullWidth,22,TRUE);MoveWindow(advancedButton,16,172,fullWidth,28,TRUE);
            InvalidateRect(window,nullptr,TRUE);return 0;
        }break;
    case WM_TIMER: case WM_APP: acquire(window); return 0;
    case WM_DESTROY: KillTimer(window,1); PostQuitMessage(result); return 0;
    }
    return DefWindowProcW(window,message,w,l);
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show) {
    try {
        int count=0; auto args=CommandLineToArgvW(GetCommandLineW(),&count); check(args!=nullptr,"Arguments unavailable");
        for(int i=1;i<count;++i) {
            if(std::wstring(args[i])==L"--samples" && i+1<count) { sampleLimit=std::stoul(args[++i]); check(sampleLimit>0,"Sample count must be positive"); }
            else if(std::wstring(args[i])==L"--log" && i+1<count) { check(logFile==nullptr,"Duplicate log option"); check(_wfopen_s(&logFile,args[++i],L"wx")==0,"Cannot create new log file"); }
            else throw std::runtime_error("Options: --samples N --log new-file.csv");
        }
        LocalFree(args);
        reader=std::make_unique<Reader>();
        try{reader->open();}catch(const std::exception& e){startupError=e.what();}
        if(logFile) { fprintf(logFile,"# pci=0x%08lX;identification_reads=%u\n",reader->identity,reader->board.acquisitions);fprintf(logFile,"# name=%ls;units=%ls;revision=%ls;vbios=%ls;sid=%ls\n",reader->board.name.c_str(),reader->board.units.c_str(),reader->board.revision.c_str(),reader->board.bios.c_str(),reader->board.sid.c_str());fprintf(logFile,"# asic_quality=%ls;quality_raw=%u\n",reader->board.quality.c_str(),reader->board.qualityRaw);fprintf(logFile,"uptime_ms,raw,hotspot_c,selector_restored,gpu_c,delta_c\n"); fflush(logFile); }
        if(logFile){fprintf(logFile,"# slt=%ls\n",reader->board.slt.c_str());fprintf(logFile,"# memory_reads=%u;vendor=%ls;model=%ls;bytes=%llu;module=%u;memory_error=%s\n",reader->board.memoryAcquisitions,reader->board.memoryVendor.c_str(),reader->board.memoryModel.c_str(),reader->board.memoryBytes,reader->board.memoryModule,reader->board.memoryError.c_str());fflush(logFile);}
        WNDCLASSW cls{}; cls.lpfnWndProc=windowProc; cls.hInstance=instance; cls.lpszClassName=L"PolarisHotspotWindow"; cls.hCursor=LoadCursor(nullptr,IDC_ARROW); cls.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
        cls.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(101));
        check(RegisterClassW(&cls)!=0,"Window registration failed");
        HWND window=CreateWindowW(cls.lpszClassName,L"Polaris Hotspot",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,CW_USEDEFAULT,CW_USEDEFAULT,296,247,nullptr,nullptr,instance,nullptr);
        check(window!=nullptr,"Window creation failed");
        HICON smallIcon=static_cast<HICON>(LoadImageW(instance,MAKEINTRESOURCEW(101),IMAGE_ICON,GetSystemMetrics(SM_CXSMICON),GetSystemMetrics(SM_CYSMICON),LR_SHARED));
        SendMessageW(window,WM_SETICON,ICON_SMALL,reinterpret_cast<LPARAM>(smallIcon));
        ShowWindow(window,show);
        MSG msg{}; int rc; while((rc=GetMessageW(&msg,nullptr,0,0))>0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        if(rc<0) result=1;
    } catch(const std::exception& e) {
        result=1; if(logFile) { fprintf(logFile,"ERROR,%s\n",e.what()); fflush(logFile); }
        if(!sampleLimit) MessageBoxA(nullptr,e.what(),"Polaris Hotspot",MB_ICONERROR);
    }
    reader.reset(); if(font) DeleteObject(font);if(detailsFont)DeleteObject(detailsFont); if(logFile) fclose(logFile); return result;
}
