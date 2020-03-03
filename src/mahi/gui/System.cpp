#include <mahi/gui/System.hpp>
#include <nfd.h>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <filesystem>
#include <cassert>
#include <sstream>
#include <iostream>
#include <algorithm>

#ifdef _WIN32
#include <pdh.h>
#include <psapi.h>
#include <tchar.h>
#include <windows.h>
#else
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <sys/sysctl.h>
#endif

namespace fs = std::filesystem;

namespace mahi::gui::System
{

DialogResult saveDialog(const std::string &filterList, const std::string &defaultPath, std::string &outPath)
{
    nfdchar_t *savePath = NULL;
    nfdresult_t result = NFD_SaveDialog(filterList.c_str(), defaultPath.length() > 0 ? defaultPath.c_str() : NULL, &savePath);
    if (result == NFD_OKAY)
    {
        outPath = savePath;
        free(savePath);
        return DialogResult::Okay;
    }
    else if (result == NFD_CANCEL)
        return DialogResult::Cancel;
    else
        return DialogResult::Error;
}

DialogResult openDialog(const std::string &filterList, const std::string &defaultPath, std::string &outPath)
{
    nfdchar_t *openPath = NULL;
    nfdresult_t result = NFD_OpenDialog(filterList.c_str(), defaultPath.length() > 0 ? defaultPath.c_str() : NULL, &openPath);
    if (result == NFD_OKAY)
    {
        outPath = openPath;
        free(openPath);
        return DialogResult::Okay;
    }
    else if (result == NFD_CANCEL)
        return DialogResult::Cancel;
    else
        return DialogResult::Error;
}

DialogResult openDialog(const std::string &filterList, const std::string &defaultPath, std::vector<std::string> &outPaths)
{
    nfdpathset_t pathSet;
    nfdresult_t result = NFD_OpenDialogMultiple(filterList.c_str(), defaultPath.length() > 0 ? defaultPath.c_str() : NULL, &pathSet);
    if (result == NFD_OKAY)
    {
        std::size_t n = NFD_PathSet_GetCount(&pathSet);
        outPaths.resize(n);
        for (std::size_t i = 0; i < n; ++i)
            outPaths[i] = NFD_PathSet_GetPath(&pathSet, i);
        NFD_PathSet_Free(&pathSet);
        return DialogResult::Okay;
    }
    else if (result == NFD_CANCEL)
        return DialogResult::Cancel;
    else
        return DialogResult::Error;
}

DialogResult pickFolder(const std::string &defaultPath, std::string &outPath)
{
    nfdchar_t *pickPath = NULL;
    nfdresult_t result = NFD_PickFolder(defaultPath.length() > 0 ? defaultPath.c_str() : NULL, &pickPath);
    if (result == NFD_OKAY)
    {
        outPath = pickPath;
        free(pickPath);
        return DialogResult::Okay;
    }
    else if (result == NFD_CANCEL)
        return DialogResult::Cancel;
    else
        return DialogResult::Error;
}

///////////////////////////////////////////////////////////////////////////////
// WINDOWS
///////////////////////////////////////////////////////////////////////////////
#ifdef _WIN32

bool openFolder(const std::string &path)
{
    fs::path p(path);
    if (fs::exists(p) && fs::is_directory(p))
    {
        ShellExecuteA(NULL, "open", p.generic_string().c_str(), NULL, NULL, SW_SHOWDEFAULT);
        return true;
    }
    return false;
}

bool openFile(const std::string &path)
{
    fs::path p(path);
    if (fs::exists(p) && fs::is_regular_file(p))
    {
        ShellExecuteA(NULL, "open", p.generic_string().c_str(), NULL, NULL, SW_SHOWDEFAULT);
        return true;
    }
    return false;
}

void openUrl(const std::string &url)
{
    ShellExecuteA(0, 0, url.c_str(), 0, 0, 5);
}

void openEmail(const std::string &address, const std::string &subject)
{
    std::string str = "mailto:" + address;
    if (!subject.empty())
        str += "?subject=" + subject;
    ShellExecuteA(0, 0, str.c_str(), 0, 0, 5);
}

// for CPU usage total
static PDH_HQUERY cpuQuery;
static PDH_HCOUNTER cpuTotal;

// for CPU usage process
static ULARGE_INTEGER lastCPU, lastSysCPU, lastUserCPU;
static int numProcessors;
static HANDLE self;

// We need to initialize a few things for Windows functions, so we create a
// simple class with the the init code in its constructor and create an isntance
struct PerformanceInitializer
{
    PerformanceInitializer()
    {
        // for CPU usage total
        PdhOpenQuery(0, 0, &cpuQuery);
        PdhAddCounter(cpuQuery, "\\Processor(_Total)\\% Processor Time", 0, &cpuTotal);
        PdhCollectQueryData(cpuQuery);
        // for CPU usage process
        SYSTEM_INFO sysInfo;
        FILETIME ftime, fsys, fuser;

        GetSystemInfo(&sysInfo);
        numProcessors = sysInfo.dwNumberOfProcessors;

        GetSystemTimeAsFileTime(&ftime);
        memcpy(&lastCPU, &ftime, sizeof(FILETIME));

        self = GetCurrentProcess();
        GetProcessTimes(self, &ftime, &ftime, &fsys, &fuser);
        memcpy(&lastSysCPU, &fsys, sizeof(FILETIME));
        memcpy(&lastUserCPU, &fuser, sizeof(FILETIME));
    }
    ~PerformanceInitializer() {}
};

// create an instance, calls the init code in constructor
PerformanceInitializer global_initializer;

double cpuUsageTotal()
{
    PDH_FMT_COUNTERVALUE counterVal;
    PdhCollectQueryData(cpuQuery);
    PdhGetFormattedCounterValue(cpuTotal, PDH_FMT_DOUBLE, NULL, &counterVal);
    return counterVal.doubleValue * 0.01;
}

double cpuUsageProcess()
{
    FILETIME ftime, fsys, fuser;
    ULARGE_INTEGER now, sys, user;
    double percent;

    GetSystemTimeAsFileTime(&ftime);
    memcpy(&now, &ftime, sizeof(FILETIME));

    GetProcessTimes(self, &ftime, &ftime, &fsys, &fuser);
    memcpy(&sys, &fsys, sizeof(FILETIME));
    memcpy(&user, &fuser, sizeof(FILETIME));
    percent = (double)((sys.QuadPart - lastSysCPU.QuadPart) +
                       (user.QuadPart - lastUserCPU.QuadPart));
    percent /= (now.QuadPart - lastCPU.QuadPart);
    percent /= numProcessors;
    lastCPU = now;
    lastUserCPU = user;
    lastSysCPU = sys;

    return percent;
}

std::size_t virtMemAvailable()
{
    MEMORYSTATUSEX mem_info;
    mem_info.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&mem_info);
    return mem_info.ullTotalPageFile;
}

std::size_t virtMemUsedTotal()
{
    MEMORYSTATUSEX mem_info;
    mem_info.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&mem_info);
    return mem_info.ullTotalPageFile - mem_info.ullAvailPageFile;
}

std::size_t virtMemUsedProcess()
{
    PROCESS_MEMORY_COUNTERS_EX pmc;
    GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS *)&pmc, sizeof(pmc));
    return pmc.PrivateUsage;
}

std::size_t ramAvailable()
{
    MEMORYSTATUSEX mem_info;
    mem_info.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&mem_info);
    return mem_info.ullTotalPhys;
}

std::size_t ramUsedTotal()
{
    MEMORYSTATUSEX mem_info;
    mem_info.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&mem_info);
    return mem_info.ullTotalPhys - mem_info.ullAvailPhys;
}

std::size_t ramUsedProcess()
{
    PROCESS_MEMORY_COUNTERS_EX pmc;
    GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS *)&pmc, sizeof(pmc));
    return pmc.WorkingSetSize;
}

struct VersionGetter
{
    VersionGetter()
    {
        const auto system = L"kernel32.dll";
        DWORD dummy;
        const auto cbInfo =
            ::GetFileVersionInfoSizeExW(FILE_VER_GET_NEUTRAL, system, &dummy);
        std::vector<char> buffer(cbInfo);
        ::GetFileVersionInfoExW(FILE_VER_GET_NEUTRAL, system, dummy, buffer.size(), &buffer[0]);
        void *p = nullptr;
        UINT size = 0;
        ::VerQueryValueW(buffer.data(), L"\\", &p, &size);
        assert(size >= sizeof(VS_FIXEDFILEINFO));
        assert(p != nullptr);
        auto pFixed = static_cast<const VS_FIXEDFILEINFO *>(p);
        std::stringstream ss;
        ss << HIWORD(pFixed->dwFileVersionMS) << '.'
           << LOWORD(pFixed->dwFileVersionMS) << '.'
           << HIWORD(pFixed->dwFileVersionLS) << '.'
           << LOWORD(pFixed->dwFileVersionLS) << '\n';
        ver = ss.str();
    }
    
    std::string ver;
};

const std::string& osName() {
    static std::string name = "Windows";
    return name;
}

const std::string& osVersion()
{
    static VersionGetter getter;
    return getter.ver;
}

void sleep(double seconds) {
    if (seconds >= 0) {
        int usecs = seconds * 1000000;
        TIMECAPS tc;
        timeGetDevCaps(&tc, sizeof(TIMECAPS));
        timeBeginPeriod(tc.wPeriodMin);
        // ::Sleep(duration.as_milliseconds()); // low-resolution method
        HANDLE timer;
        LARGE_INTEGER ft;
        ft.QuadPart = -(10 * usecs);
        timer = CreateWaitableTimer(NULL, TRUE, NULL);
        SetWaitableTimer(timer, &ft, 0, NULL, NULL, 0);
        WaitForSingleObject(timer, INFINITE);
        CloseHandle(timer);
        // timeEndPeriod(tc.wPeriodMin); // to much overhead, not necessary?
    }
}


#elif (__APPLE__)

///////////////////////////////////////////////////////////////////////////////
// macOS
///////////////////////////////////////////////////////////////////////////////

bool openFolder(const std::string &path)
{
    fs::path p(path);
    if (fs::exists(p) && fs::is_directory(p))
    {
        std::string command = "open " + p.generic_string();
        system(command.c_str());
        return true;
    }
    return false;
}

void openUrl(const std::string &url)
{
    std::string command = "open " + url;
    system(command.c_str());
}

void openEmail(const std::string &address, const std::string &subject)
{
    std::string mailTo = "mailto:" + address + "?subject=" + subject; // + "\\&body=" + bodyMessage;
    std::string command = "open " + mailTo;
    system(command.c_str());
}

double cpuUsageTotal()
{
    return 0; // TODO
}

double cpuUsageProcess()
{
    return 0; // TODO
}

std::size_t virtMemAvailable()
{
    return 0; // TODO
}

std::size_t virtMemUseTotal()
{
    return 0; // TODO
}

std::size_t virtMemUsedProcess()
{
    return 0; // TODO
}

std::size_t ramAvailable()
{
    return 0; // TODO
}

std::size_t ramUsedTotal()
{
    return 0; // TODO
}

std::size_t ramUsedProcess()
{
    return 0; // TODO
}

struct NameGetter {
    NameGetter() {
        FILE* stdoutFile = popen("sw_vers -productName","r");
        if (stdoutFile) {
            char buff[32];
            char* stdout = fgets(buff, sizeof(buff), stdoutFile);
            name = stdout;
            if (!name.empty() && name[name.length()-1] == '\n')
                name.erase(name.length()-1);
            pclose(stdoutFile);
        }
    }
    std::string name = "N/A";
};

struct VersionGetter {
    VersionGetter() {
        FILE* stdoutFile = popen("sw_vers -productVersion","r");
        if (stdoutFile) {
            char buff[32];
            char* stdout = fgets(buff, sizeof(buff), stdoutFile);
            ver = stdout;
            if (!ver.empty() && ver[ver.length()-1] == '\n')
                ver.erase(ver.length()-1);
            pclose(stdoutFile);
        }
    }
    std::string ver = "";
};

const std::string& osName() {
    static NameGetter getter;
    return getter.name;
}

const std::string& osVersion() {
    static VersionGetter getter;
    return getter.ver;
}


void sleep(double seconds) {
    if (seconds >= 0) {
        uint64 usecs = seconds * 1000000;
        // Construct the time to wait
        timespec ti;
        ti.tv_nsec = (usecs % 1000000) * 1000;
        ti.tv_sec = usecs / 1000000;
        // If nanosleep returns -1, we check errno. If it is EINTR
        // nanosleep was interrupted and has set ti to the remaining
        // duration. We continue sleeping until the complete duration
        // has passed. We stop sleeping if it was due to an error.
        while ((nanosleep(&ti, &ti) == -1) && (errno == EINTR)) {
    }
}

#endif

} // namespace mahi::gui::System

// Links/Resources
// https://stackoverflow.com/questions/63166/how-to-determine-cpu-and-memory-consumption-from-inside-a-process