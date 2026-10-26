#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace fs=std::filesystem;
struct Handle { HANDLE h{}; ~Handle(){if(h&&h!=INVALID_HANDLE_VALUE)CloseHandle(h);} };
uintptr_t remoteModule(DWORD pid,const wchar_t* name,std::wstring* path=nullptr){
 Handle snap{CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,pid)};MODULEENTRY32W m{sizeof(m)};
 if(Module32FirstW(snap.h,&m))do{if(!_wcsicmp(m.szModule,name)){if(path)*path=m.szExePath;return reinterpret_cast<uintptr_t>(m.modBaseAddr);}}while(Module32NextW(snap.h,&m));
 return 0;
}
uintptr_t remoteFunction(DWORD pid,FARPROC function){
 HMODULE owner{};if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(function),&owner))throw std::runtime_error("Cannot locate a Windows loader function.");
 wchar_t path[32768]{};GetModuleFileNameW(owner,path,32768);auto base=remoteModule(pid,fs::path(path).filename().c_str());if(!base)throw std::runtime_error("Windows loader module is missing in the game.");
 return base+(reinterpret_cast<uintptr_t>(function)-reinterpret_cast<uintptr_t>(owner));
}
DWORD thread(HANDLE process,uintptr_t function,void* argument){
 Handle h{CreateRemoteThread(process,nullptr,0,reinterpret_cast<LPTHREAD_START_ROUTINE>(function),argument,0,nullptr)};
 if(!h.h||WaitForSingleObject(h.h,30000)!=WAIT_OBJECT_0)throw std::runtime_error("The game did not finish initialization. Close it before retrying.");
 DWORD code{};if(!GetExitCodeThread(h.h,&code))throw std::runtime_error("Cannot read initialization status.");return code;
}
int wmain(int argc,wchar_t** argv){
 try{
  bool probe=argc==2&&!wcscmp(argv[1],L"--probe");if(argc>1&&!probe)throw std::runtime_error("Usage: TK2-Antigravity.exe [--probe]");
  wchar_t path[32768]{};GetModuleFileNameW(nullptr,path,32768);auto folder=fs::path(path).parent_path(),dll=folder/L"TK2-Antigravity.dll";
  if(!fs::is_regular_file(dll))throw std::runtime_error("Keep TK2-Antigravity.dll beside this launcher.");
  DWORD pid{};Handle snap{CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0)};PROCESSENTRY32W e{sizeof(e)};
  if(Process32FirstW(snap.h,&e))do{if(!_wcsicmp(e.szExeFile,L"TheKarters2.exe")){if(pid)throw std::runtime_error("More than one game process is running.");pid=e.th32ProcessID;}}while(Process32NextW(snap.h,&e));
  if(!pid)throw std::runtime_error("Start The Karters 2 normally and stay in its offline main menu, then run this launcher.");
  Handle process{OpenProcess(PROCESS_CREATE_THREAD|PROCESS_QUERY_INFORMATION|PROCESS_VM_OPERATION|PROCESS_VM_WRITE|PROCESS_VM_READ,FALSE,pid)};
  if(!process.h)throw std::runtime_error("Cannot access the game. Run both applications under the same Windows account and privilege level.");
  wchar_t gamePath[32768]{};DWORD pathLength=32768;
  if(!QueryFullProcessImageNameW(process.h,0,gamePath,&pathLength))throw std::runtime_error("Cannot verify the game path.");
  auto gameFolder=fs::path(gamePath).parent_path();
  if(!fs::is_regular_file(gameFolder/L"GameAssembly.dll")||!fs::is_regular_file(gameFolder/L"UnityPlayer.dll"))throw std::runtime_error("The process is not an installed IL2CPP game.");
  auto config=gameFolder/L"BepInEx/config";
  if(!probe&&fs::is_directory(config))for(auto& file:fs::directory_iterator(config))if(file.path().extension()==L".cfg"){
   wchar_t enabled[16]{};GetPrivateProfileStringW(L"Antigravity",L"Enabled",L"false",enabled,16,file.path().c_str());
   if(!_wcsicmp(enabled,L"true")||!wcscmp(enabled,L"1"))throw std::runtime_error("Turn off the existing Antigravity module in Studio before enabling this companion. --probe can run without disabling it.");
  }
  USHORT gameMachine{},nativeMachine{};if(!IsWow64Process2(process.h,&gameMachine,&nativeMachine)||gameMachine!=IMAGE_FILE_MACHINE_UNKNOWN||nativeMachine!=IMAGE_FILE_MACHINE_AMD64)throw std::runtime_error("An x64 game process is required.");
  std::wstring loadedPath;auto base=remoteModule(pid,L"TK2-Antigravity.dll",&loadedPath);bool loadedHere=!base;
  if(base&&_wcsicmp(fs::weakly_canonical(dll).c_str(),fs::weakly_canonical(loadedPath).c_str()))throw std::runtime_error("A different copy of the runtime is already loaded. Close the game before switching copies.");
  if(!base){
   auto load=remoteFunction(pid,GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"LoadLibraryW"));
   std::wstring name=dll.wstring();size_t bytes=(name.size()+1)*sizeof(wchar_t);void* memory=VirtualAllocEx(process.h,nullptr,bytes,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
   if(!memory)throw std::runtime_error("Cannot allocate the DLL path in the game.");
   SIZE_T written{};if(!WriteProcessMemory(process.h,memory,name.c_str(),bytes,&written)||written!=bytes){VirtualFreeEx(process.h,memory,0,MEM_RELEASE);throw std::runtime_error("Cannot pass the DLL path to the game.");}
   thread(process.h,load,memory);
   VirtualFreeEx(process.h,memory,0,MEM_RELEASE);base=remoteModule(pid,L"TK2-Antigravity.dll");if(!base)throw std::runtime_error("Windows could not load the companion DLL.");
  }
  HMODULE image=LoadLibraryExW(dll.c_str(),nullptr,DONT_RESOLVE_DLL_REFERENCES);if(!image)throw std::runtime_error("Cannot inspect the DLL entry point.");
  auto entry=GetProcAddress(image,"Initialize");uintptr_t offset=reinterpret_cast<uintptr_t>(entry)-reinterpret_cast<uintptr_t>(image);FreeLibrary(image);
  if(!entry)throw std::runtime_error("Missing Initialize export.");
  auto result=thread(process.h,base+offset,reinterpret_cast<void*>(uintptr_t(probe?1:0)));
  if(probe&&loadedHere&&result!=2)thread(process.h,remoteFunction(pid,GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"FreeLibrary")),reinterpret_cast<void*>(base));
  if(!result)throw std::runtime_error("Compatibility or initialization failed. Read TK2-Antigravity.log beside this launcher.");
  std::cout<<(result==1?"Compatibility check passed; no hooks installed.\n":"Native companion active for marked surfaces only.\n")
           <<"No Steam files changed. Close the game to unload it.\nLog: TK2-Antigravity.log\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
