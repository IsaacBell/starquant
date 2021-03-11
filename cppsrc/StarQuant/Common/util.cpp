/*****************************************************************************
 * Copyright [2019] 
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *****************************************************************************/

#include <Common/util.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>
#include <execinfo.h>
#include <signal.h>
#include <unistd.h>
#include <stddef.h>
#include <mutex>
#include <iostream>
#include <chrono>
#include <thread>
#include <fstream>
#include <chrono>
#include <boost/locale.hpp>
#include <boost/date_time.hpp>
#include <boost/date_time/local_time/local_time.hpp>
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>

using namespace boost::posix_time;
using namespace boost::local_time;
using namespace std;

namespace StarQuant {

// signal handler

Except_frame g_except_stack = Except_frame();
void errorDump() {
    void* array[30];
    size_t size;
    char** strings;
    size_t i;

    size = backtrace(array, 30);
    strings = backtrace_symbols(array, size);
    if (NULL == strings) {
        perror("backtrace_symbols");
    }

    printf("Obtained %zd stack frames.\n", size);

    for (i = 0 ; i < size; i++) {
        std::cout << array[i] << std::endl;
        printf("%s\n", strings[i]);
    }

    free(strings);
    strings = NULL;
}
void recvSignal(int32_t sig) {
    printf("StarQuant received signal %d !\n", sig);
    errorDump();
    siglongjmp(g_except_stack.env, 1);
}

// console related
std::atomic<bool> gShutdown{ false };

#if defined(_WIN64) || defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
    BOOL CtrlHandler(DWORD fdwCtrlType) {
        switch (fdwCtrlType) {
            case CTRL_C_EVENT:
            case CTRL_CLOSE_EVENT:
                PRINT_SHUTDOWN_MESSAGE;
                gShutdown = true;
                return(TRUE);

            case CTRL_BREAK_EVENT:
            case CTRL_LOGOFF_EVENT:
            case CTRL_SHUTDOWN_EVENT:
                PRINT_SHUTDOWN_MESSAGE;
                gShutdown = true;
                return FALSE;

            default:
                return FALSE;
        }
    }

    std::atomic<bool>* setconsolecontrolhandler(void) {
        bool b = SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE);
        if (!b) {
            printf("\nERROR: Could not set control handler");
        }
        return &gShutdown;
    }

#elif defined(__linux__)
    void ConsoleControlHandler(int32_t sig) {
        gShutdown = true;
        PRINT_SHUTDOWN_MESSAGE;
        // throw runtime_error("crl c");
    }

// std::atomic<bool>* setconsolecontrolhandler(void) {
// signal(SIGINT, ConsoleControlHandler);
// signal(SIGPWR, ConsoleControlHandler);
// return &gShutdown;
// }

#endif
// int32_t check_gshutdown(bool force) {
// atomic_bool* g = setconsolecontrolhandler();
// while (!*g) {
// msleep(1 * 1000);
// }
// // ctrl-c
// if (force) {
// throw runtime_error("Throw an exception to trigger shutdown");
// }
// return 0;
// }



// string related

string extractExchangeID(const string& fullsym) {
    string ex;
    stringstream ss(fullsym);
    getline(ss, ex, ' ');
    return ex;
}


vector<string> stringsplit(const string &s, char delim) {
    vector<string> elems;

    stringstream ss(s);
    string item;
    while (getline(ss, item, delim)) {
        elems.push_back(item);
    }
    string& last = elems.back();
    if (last.back() == '\0')
        last.pop_back();
    return elems;
}

bool startwith(const string& x, const string& y) {
    return x.find(y) == 0;
}




bool endwith(const std::string &str, const std::string &suffix) {
    return str.size() >= suffix.size() &&
        str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}



string UTF8ToGBK(const std::string & strUTF8) {
    string stroutGBK = "";
    stroutGBK = boost::locale::conv::between(strUTF8, "GB18030", "UTF-8");
    return stroutGBK;
}

string GBKToUTF8(const std::string & strGBK) {
    string strOutUTF8 = "";
    strOutUTF8 = boost::locale::conv::between(strGBK, "UTF-8", "GB18030");
    return strOutUTF8;
}


// numerical

double rounded(double x, int32_t n) {
    char out[64];
    double xrounded;
    sprintf(out, "%.*f", n, x);
    xrounded = strtod(out, 0);
    return xrounded;
}

// time related
uint64_t getMicroTime() {
    uint64_t t = 0;

#if defined(_WIN32) || defined(_WIN64)
        FILETIME tm;
#if defined(NTDDI_WIN8) && NTDDI_VERSION >= NTDDI_WIN8
        /* Windows 8, Windows Server 2012 and later. ---------------- */
        GetSystemTimePreciseAsFileTime(&tm);
#else
        /* Windows 2000 and later. ---------------------------------- */
        GetSystemTimeAsFileTime(&tm);
#endif
        t = ((uint64_t)tm.dwHighDateTime << 32) | (uint64_t)tm.dwLowDateTime;
        return t;

#elif (defined(__hpux) || defined(hpux)) || ((defined(__sun__) || defined(__sun) || defined(sun)) && (defined(__SVR4) || defined(__svr4__)))
        /* HP-UX, Solaris. ------------------------------------------ */
        return (double)gethrtime() / 1000000000.0;

#elif defined(__MACH__) && defined(__APPLE__)
        /* OSX. ----------------------------------------------------- */
        static double timeConvert = 0.0;
        if (timeConvert == 0.0) {
            mach_timebase_info_data_t timeBase;
            (void)mach_timebase_info(&timeBase);
            timeConvert = (double)timeBase.numer /
                (double)timeBase.denom /
                1000000000.0;
        }
        return (double)mach_absolute_time() * timeConvert;

#elif defined(_POSIX_VERSION)
        /* POSIX. --------------------------------------------------- */
#if defined(_POSIX_TIMERS) && (_POSIX_TIMERS > 0)
        {
            struct timespec ts;
#if defined(CLOCK_MONOTONIC_PRECISE)
            /* BSD. --------------------------------------------- */
            const clockid_t id = CLOCK_MONOTONIC_PRECISE;
#elif defined(CLOCK_MONOTONIC_RAW)
            /* Linux. ------------------------------------------- */
            const clockid_t id = CLOCK_MONOTONIC_RAW;
#elif defined(CLOCK_HIGHRES)
            /* Solaris. ----------------------------------------- */
            const clockid_t id = CLOCK_HIGHRES;
#elif defined(CLOCK_MONOTONIC)
            /* AIX, BSD, Linux, POSIX, Solaris. ----------------- */
            const clockid_t id = CLOCK_MONOTONIC;
#elif defined(CLOCK_REALTIME)
            /* AIX, BSD, HP-UX, Linux, POSIX. ------------------- */
            const clockid_t id = CLOCK_REALTIME;
#else
            const clockid_t id = (clockid_t)-1; /* Unknown. */
#endif /* CLOCK_* */
            if (id != (clockid_t)-1 && clock_gettime(id, &ts) != -1) {
                t = ts.tv_sec*__NANOO_MULTIPLE__ + ts.tv_nsec;
                return t / 1000;
            }
            /* Fall thru. */
        }
#endif /* _POSIX_TIMERS */

        /* AIX, BSD, Cygwin, HP-UX, Linux, OSX, POSIX, Solaris. ----- */
        struct timeval tm;
        gettimeofday(&tm, nullptr);
        t = __MICRO_MULTIPLE__*tm.tv_sec + tm.tv_usec;
        return t;
#else
        return 0;  /* Failed. */
#endif
}

int32_t getMilliSeconds() {
    struct timeval tm;
    int32_t milli = 0;
    gettimeofday(&tm, nullptr);
    milli = int32_t(tm.tv_usec / 1000);
    return milli;
}

string ymd() {
    char buf[128] = { 0 };
    const size_t sz = sizeof("0000-00-00");
    {
        time_t timer;
        struct tm tm_info;
        time(&timer);
        localtime_r(&timer, &tm_info);
        strftime(buf, sz, DATE_FORMAT, &tm_info);
    }
    return string(buf);
}
string ymdcompact() {
    char buf[128] = { 0 };
    const size_t sz = sizeof("00000000");
    {
        time_t timer;
        struct tm tm_info;
        time(&timer);
        localtime_r(&timer, &tm_info);
        strftime(buf, sz, DATE_FORMAT_COMPACT, &tm_info);
    }
    return string(buf);
}

string ymdhms() {
    char buf[128] = { 0 };
    const size_t sz = sizeof("0000-00-00 00-00-00");
    {
        time_t timer;
        time(&timer);
        struct tm tm_info;
        localtime_r(&timer, &tm_info);
        strftime(buf, sz, DATE_TIME_FORMAT, &tm_info);
    }
    return string(buf);
}

string ymdhmsf() {
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    std::chrono::system_clock::duration tp = now.time_since_epoch();
    tp -= std::chrono::duration_cast<std::chrono::seconds>(tp);
    time_t tt = std::chrono::system_clock::to_time_t(now);

    // tm t = *gmtime(&tt);
    struct tm t;
    localtime_r(&tt, &t);

    char buf[64];
    std::sprintf(buf, "%04u-%02u-%02u %02u:%02u:%02u.%03u", t.tm_year + 1900,
        t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec,
        static_cast<unsigned>(tp / std::chrono::milliseconds(1)));

    return string(buf);
}
string ymdhmsf6() {
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    std::chrono::system_clock::duration tp = now.time_since_epoch();
    tp -= std::chrono::duration_cast<std::chrono::seconds>(tp);
    time_t tt = std::chrono::system_clock::to_time_t(now);

    struct tm t;
    localtime_r(&tt, &t);

    char buf[64];
    std::sprintf(buf, "%04u-%02u-%02u %02u:%02u:%02u.%06u", t.tm_year + 1900,
        t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec,
        static_cast<unsigned>(tp / std::chrono::microseconds(1)));

    return string(buf);
}




string hmsf() {
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    std::chrono::system_clock::duration tp = now.time_since_epoch();
    tp -= std::chrono::duration_cast<std::chrono::seconds>(tp);
    time_t tt = std::chrono::system_clock::to_time_t(now);

    struct tm t;
    localtime_r(&tt, &t);

    char buf[64];
    std::sprintf(buf, "%02u:%02u:%02u.%03u", t.tm_hour, t.tm_min, t.tm_sec,
        static_cast<unsigned>(tp / std::chrono::milliseconds(1)));

    return string(buf);
}

int32_t hmsf2inttime(string hmsf) {
    return std::stoi(hmsf.substr(0, 2)) * 10000 + std::stoi(hmsf.substr(3, 2)) * 100 + std::stoi(hmsf.substr(6, 2));
}

void msleep(uint64_t _ms) {
    if (_ms == 0) { return; }
    this_thread::sleep_for(chrono::milliseconds(_ms));
}

    string nowMS() {
        char buf[128] = {};
#ifdef __linux__
        struct timespec ts = { 0, 0 };
        struct tm tm = {};
        char timbuf[64] = {};
        clock_gettime(CLOCK_REALTIME, &ts);
        time_t tim = ts.tv_sec;
        localtime_r(&tim, &tm);
        strftime(timbuf, sizeof(timbuf), "%F %T", &tm);
        snprintf(buf, 128, "%s.%03d", timbuf, (int32_t)(ts.tv_nsec / 1000000));
#elif defined(__MACH__) && defined(__APPLE__)
        struct timespec ts = { 0, 0 };
        struct tm tm = {};
        char timbuf[64] = {};
        clock_gettime(CLOCK_REALTIME, &ts);
        time_t tim = ts.tv_sec;
        localtime_r(&tim, &tm);
        strftime(timbuf, sizeof(timbuf), "%F %T", &tm);
        snprintf(buf, 128, "%s.%03d", timbuf, (int32_t)(ts.tv_nsec / 1000000));
#else
        SYSTEMTIME SystemTime;
        GetLocalTime(&SystemTime);
        sprintf(buf, "%04u-%02u-%02u %02u:%02u:%02u.%03u",
            SystemTime.wYear, SystemTime.wMonth, SystemTime.wDay,
            SystemTime.wHour, SystemTime.wMinute, SystemTime.wSecond, SystemTime.wMilliseconds);
#endif
        return buf;
    }

    time_t ptime2time(ptime t) {
        static ptime epoch(boost::gregorian::date(1970, 1, 1));
        time_duration::sec_type x = (t - epoch).total_seconds() - 3600 * NYC_TZ_OFFSET;
        // hours(4).total_seconds() = 3600 * 4
        // ... check overflow here ...
        return time_t(x);
    }

    string ptime2str(const ptime& pt) {
        time_zone_ptr tz_cet(new boost::local_time::posix_time_zone(NYC_TZ_STR));
        local_date_time dt_with_zone(pt, tz_cet); // glocale::instance()._ny_tzone);
#if 1
        tm _t = to_tm(dt_with_zone);
        char buf[32] = { 0 };
        strftime(buf, 32, DATE_TIME_FORMAT, &_t);
        return buf;
#else
// using stringstream only for logging
        stringstream strm;
        strm.imbue(*glocale::instance()._s_loc);
        strm << dt_with_zone;
        // strm << pt;
        return strm.str();
#endif
    }


// http://stackoverflow.com/questions/4461586/how-do-i-convert-boostposix-timeptime-to-time-t
time_t str2time_t(const string& s) {
    ptime pt(time_from_string(s));
    return ptime2time(pt);
}

string time_t2str(time_t tt) {
    ptime pt = from_time_t(tt);
    // return ptime2str(pt);
    return to_simple_string(pt);
}

int32_t tointdate() {
    time_t current_time;
    time(&current_time);
    return tointdate(current_time);
}

int32_t tointtime() {
    time_t current_time;
    time(&current_time);
    return tointtime(current_time);
}

int32_t tointdate(time_t time) {
    struct tm timeinfo;
    LOCALTIME_S(&timeinfo, &time);

    return ((timeinfo.tm_year + 1900) * 10000) + ((timeinfo.tm_mon + 1) * 100) + timeinfo.tm_mday;
}

int32_t tointtime(time_t time) {
    // std::time_t rawtime;
    // std::tm* timeinfo;
    // char queryTime[80];
    // std::time(&rawtime);
    // timeinfo = std::localtime(&rawtime);
    // std::strftime(queryTime, 80, "%Y%m%d %H:%M:%S", timeinfo);
    struct tm timeinfo;
    LOCALTIME_S(&timeinfo, &time);

    return (timeinfo.tm_hour * 10000) + (timeinfo.tm_min * 100) + (timeinfo.tm_sec);
}

// convert to # of seconds
int32_t inttimetointtimespan(int32_t time) {
    int32_t s1 = time % 100;
    int32_t m1 = ((time - s1) / 100) % 100;
    int32_t h1 = (int32_t)((time - (m1 * 100) - s1) / 10000);

    return h1 * 3600 + m1 * 60 + s1;
}

// # of seconds to int32_t time
int32_t inttimespantointtime(int32_t timespan) {
    int32_t hour = timespan / 3600;
    int32_t second = timespan % 3600;
    int32_t minute = second / 60;
    second = second % 60;
    return (hour * 10000 + minute * 100 + second);
}

// adds inttime and int32_t timespan (in seconds).  does not rollover 24hr periods.
int32_t inttimeadd(int32_t firsttime, int32_t timespaninseconds) {
    int32_t s1 = firsttime % 100;
    int32_t m1 = ((firsttime - s1) / 100) % 100;
    int32_t h1 = (int32_t)((firsttime - m1 * 100 - s1) / 10000);
    s1 += timespaninseconds;
    if (s1 >= 60) {
        m1 += (int32_t)(s1 / 60);
        s1 = s1 % 60;
    }
    if (m1 >= 60) {
        h1 += (int32_t)(m1 / 60);
        m1 = m1 % 60;
    }
    int32_t sum = h1 * 10000 + m1 * 100 + s1;
    return sum;
}

int32_t inttimediff(int32_t firsttime, int32_t latertime) {
    int32_t span1 = inttimetointtimespan(firsttime);
    int32_t span2 = inttimetointtimespan(latertime);
    return span2 - span1;
}




int64_t string2unixtimems(const string& s) {
        struct tm tm_;
        int64_t unixtimems;
        int32_t year, month, day, hour, minute, second, millisec;
        sscanf(s.c_str(),"%d-%d-%d %d:%d:%d.%d", &year, &month, &day, &hour, &minute, &second,&millisec);
        tm_.tm_year  = year-1900;
        tm_.tm_mon   = month-1;
        tm_.tm_mday  = day;
        tm_.tm_hour  = hour;
        tm_.tm_min   = minute;
        tm_.tm_sec   = second;
        tm_.tm_isdst = 0;

        time_t t_ = mktime(&tm_);
        unixtimems = t_*1000+millisec;
        return unixtimems;
}


}  // namespace StarQuant
