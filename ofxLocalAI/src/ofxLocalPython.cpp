#include "ofxLocalPython.h"
#include <fstream>
#include <thread>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <iterator>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <spawn.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
extern char** environ;
#endif

namespace {
#ifdef _WIN32
std::wstring wide(const std::string& text) {
    int size=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),int(text.size()),nullptr,0);
    if (!size && !text.empty()) throw std::runtime_error("Ruta UTF-8 inválida");
    std::wstring result(size,L'\0');
    MultiByteToWideChar(CP_UTF8,0,text.data(),int(text.size()),result.data(),size);
    return result;
}
std::wstring quote(const std::string& text) {
    std::wstring out=L"\""; size_t slashes=0;
    for (auto c:wide(text)) {
        if (c==L'\\') { ++slashes; continue; }
        out.append(c==L'"' ? slashes*2+1 : slashes,L'\\'); slashes=0;
        out+=c;
    }
    out.append(slashes*2,L'\\'); return out+L'"';
}
#endif
}

ofxLocalPython::~ofxLocalPython() { close(); }

bool ofxLocalPython::start(const std::filesystem::path& workspace,const std::filesystem::path& executable,
                           const std::vector<std::string>& arguments) {
    update();
    if (running()) { failure="Ya hay un proceso local activo. Desconectarlo primero."; return false; }
    failure.clear(); code=0; stopRequested=false;
    try {
        if (!workspace.is_absolute() || !std::filesystem::is_directory(workspace))
            throw std::runtime_error("Configurar una carpeta de proyecto Python absoluta y existente");
        if (!executable.is_absolute() || !std::filesystem::is_regular_file(executable))
            throw std::runtime_error("No se encontró el ejecutable del entorno Python: "+executable.string());
        if (arguments.empty() || arguments[0]!="serve")
            throw std::runtime_error("Servicio local no admitido");
        for (const auto& arg:arguments) if (arg.find('\0')!=std::string::npos)
            throw std::runtime_error("Argumento con carácter nulo");
        auto directory=workspace/".local"/"managed";
        std::filesystem::create_directories(directory);
#ifdef _WIN32
        auto parent=GetCurrentProcessId();
#else
        auto parent=getpid();
#endif
        static unsigned serial=0;
        auto name=std::to_string(parent)+"-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"-"+std::to_string(++serial);
        logFile=directory/(name+".log"); stopFile=directory/(name+".stop");
        std::vector<std::string> args={ofxLocalPathUtf8(executable),"-u",ofxLocalPathUtf8(workspace/"scripts"/"run_service.py"),"--stop-file",ofxLocalPathUtf8(stopFile),"--parent-pid",std::to_string(parent)};
#ifdef _WIN32
        std::wstring command;
        for (const auto& arg:args) { if (!command.empty()) command+=L' '; command+=quote(arg); }
        SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES),nullptr,TRUE};
        HANDLE output=CreateFileW(logFile.wstring().c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,
                                   &security,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        HANDLE input=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&security,OPEN_EXISTING,0,nullptr);
        if (output==INVALID_HANDLE_VALUE || input==INVALID_HANDLE_VALUE) {
            if (output!=INVALID_HANDLE_VALUE) CloseHandle(output);
            if (input!=INVALID_HANDLE_VALUE) CloseHandle(input);
            throw std::runtime_error("No se pudo preparar el log del servicio");
        }
        STARTUPINFOW startup{}; startup.cb=sizeof(startup); startup.dwFlags=STARTF_USESTDHANDLES;
        startup.hStdInput=input; startup.hStdOutput=output; startup.hStdError=output;
        PROCESS_INFORMATION process{};
        bool ok=CreateProcessW(executable.wstring().c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,
                               nullptr,workspace.wstring().c_str(),&startup,&process);
        auto error=GetLastError(); CloseHandle(input); CloseHandle(output);
        if (!ok) throw std::runtime_error("No se pudo iniciar Python (Windows "+std::to_string(error)+")");
        CloseHandle(process.hThread); handle=process.hProcess; pid=process.dwProcessId;
#else
        posix_spawn_file_actions_t actions;
        int error=posix_spawn_file_actions_init(&actions);
        if (error) throw std::runtime_error(std::strerror(error));
        error=posix_spawn_file_actions_addopen(&actions,STDOUT_FILENO,logFile.c_str(),O_WRONLY|O_CREAT|O_TRUNC,0600);
        if (!error) error=posix_spawn_file_actions_adddup2(&actions,STDOUT_FILENO,STDERR_FILENO);
        if (!error) error=posix_spawn_file_actions_addopen(&actions,STDIN_FILENO,"/dev/null",O_RDONLY,0);
        std::vector<char*> argv;
        for (auto& arg:args) argv.push_back(arg.data());
        argv.push_back(nullptr);
        pid_t child=0;
        if (!error) error=posix_spawn(&child,executable.c_str(),&actions,nullptr,argv.data(),environ);
        posix_spawn_file_actions_destroy(&actions);
        if (error) throw std::runtime_error("No se pudo iniciar Python: "+std::string(std::strerror(error)));
        pid=static_cast<unsigned long>(child);
#endif
        return true;
    } catch (const std::exception& exc) { failure=exc.what(); return false; }
}

void ofxLocalPython::update() {
    if (!running()) return;
    bool done=false;
#ifdef _WIN32
    if (WaitForSingleObject(static_cast<HANDLE>(handle),0)==WAIT_OBJECT_0) {
        DWORD status=0; GetExitCodeProcess(static_cast<HANDLE>(handle),&status);
        code=int(status); CloseHandle(static_cast<HANDLE>(handle)); handle=nullptr; done=true;
    }
#else
    int status=0;
    auto result=waitpid(static_cast<pid_t>(pid),&status,WNOHANG);
    if (result>0) { code=WIFEXITED(status) ? WEXITSTATUS(status) : 128+WTERMSIG(status); done=true; }
    else if (result<0 && errno==ECHILD) { code=-1; done=true; }
#endif
    if (done) {
        pid=0;
        if (code!=0 && !stopRequested) failure="Python terminó (código "+std::to_string(code)+").\n"+logTail();
        stopRequested=false;
        std::error_code ignored; std::filesystem::remove(stopFile,ignored);
    } else if (stopRequested && std::chrono::steady_clock::now()-stopAt>std::chrono::seconds(5)) forceStop();
}

void ofxLocalPython::stop() {
    update();
    if (!running() || stopRequested) return;
    stopRequested=true; stopAt=std::chrono::steady_clock::now();
    std::ofstream(stopFile).put('\n'); // Python handles shutdown on its main thread.
}

void ofxLocalPython::forceStop() {
    if (!running()) return;
#ifdef _WIN32
    TerminateProcess(static_cast<HANDLE>(handle),130);
#else
    kill(static_cast<pid_t>(pid),SIGKILL);
#endif
}

void ofxLocalPython::close() {
    stop();
    auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    while (running() && std::chrono::steady_clock::now()<deadline) {
        update(); std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (!running()) return;
    forceStop();
#ifdef _WIN32
    WaitForSingleObject(static_cast<HANDLE>(handle),1000);
    CloseHandle(static_cast<HANDLE>(handle)); handle=nullptr;
#else
    int status; while (waitpid(static_cast<pid_t>(pid),&status,0)<0 && errno==EINTR) {}
#endif
    pid=0; stopRequested=false;
    std::error_code ignored; std::filesystem::remove(stopFile,ignored);
}

std::string ofxLocalPython::logTail() const {
    std::ifstream input(logFile,std::ios::binary);
    if (!input) return {};
    input.seekg(0,std::ios::end); auto length=input.tellg();
    if (length>4096) input.seekg(length-std::streamoff(4096)); else input.seekg(0);
    auto result=std::string(std::istreambuf_iterator<char>(input),{});
    // A bounded read may start halfway through a UTF-8 character.
    size_t start=0;
    while (start<result.size() && (static_cast<unsigned char>(result[start])&0xc0)==0x80) ++start;
    return result.substr(start);
}
