// Opt-in bounded frame-time capture. No player names, network addresses or tokens.
#ifndef KFX_MACOS_FRAME_PROFILE_H
#define KFX_MACOS_FRAME_PROFILE_H
#ifdef __APPLE__
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <string>
#include <sys/resource.h>
#include "bflib_datetm.h"
struct KfxFrameProfile {
    const char *output=std::getenv("KFX_FRAME_PROFILE");
    using Clock=std::chrono::steady_clock;
    Clock::time_point previous=Clock::now(),start=previous;
    std::vector<double> intervals;
    unsigned long count=0;
    double cpu_start=cpu_seconds();
    static double cpu_seconds(){struct rusage usage={};getrusage(RUSAGE_SELF,&usage);return usage.ru_utime.tv_sec+usage.ru_utime.tv_usec/1e6+usage.ru_stime.tv_sec+usage.ru_stime.tv_usec/1e6;}
    void flush(){
        if(!output||intervals.empty())return;
        auto sorted=intervals;std::sort(sorted.begin(),sorted.end());
        struct rusage usage={};getrusage(RUSAGE_SELF,&usage);
        std::string pending=std::string(output)+".pending";FILE *file=fopen(pending.c_str(),"w");if(!file)return;
        double elapsed=std::chrono::duration<double>(Clock::now()-start).count();
        fprintf(file,"{\"frames\":%lu,\"samples\":%zu,\"elapsed_seconds\":%.3f,\"p50_ms\":%.3f,\"p95_ms\":%.3f,\"p99_ms\":%.3f,\"cpu_seconds\":%.3f,\"peak_rss_bytes\":%ld,\"logic_ms\":%.3f,\"draw_ms\":%.3f}\n",count,sorted.size(),elapsed,sorted[sorted.size()/2],sorted[(sorted.size()-1)*95/100],sorted[(sorted.size()-1)*99/100],cpu_seconds()-cpu_start,usage.ru_maxrss,frametime_measurements.frametime_current[Frametime_Logic],frametime_measurements.frametime_current[Frametime_Draw]);
        if(fclose(file)==0)rename(pending.c_str(),output);
    }
    void frame(){if(!output)return;auto now=Clock::now();double ms=std::chrono::duration<double,std::milli>(now-previous).count();previous=now;if(count++>0&&ms>0&&ms<10000){if(intervals.size()<4096)intervals.push_back(ms);else intervals[count%4096]=ms;}if(count%120==0)flush();}
    ~KfxFrameProfile(){flush();}
};
inline void kfx_profile_present(){static KfxFrameProfile profile;profile.frame();}
#else
inline void kfx_profile_present(){}
#endif
#endif
