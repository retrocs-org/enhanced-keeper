#include "bflib_basics.h"
extern "C" { extern TbClockMSec (*LbTimerClock)(void); TbResult LbTimerInit(void); TbBool LbSleepUntil(TbClockMSec); }
#include <cstdio>
#include <sys/resource.h>
#include <chrono>
static double cpu(){rusage u={};getrusage(RUSAGE_SELF,&u);return u.ru_utime.tv_sec+u.ru_utime.tv_usec/1e6+u.ru_stime.tv_sec+u.ru_stime.tv_usec/1e6;}
int main(){LbTimerInit();double before=cpu();auto start=std::chrono::steady_clock::now();TbClockMSec deadline=LbTimerClock();for(int i=0;i<1000;i++)LbSleepUntil(deadline+(i+1)*2);double elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();printf("{\"elapsed_seconds\":%.3f,\"cpu_seconds\":%.3f}\n",elapsed,cpu()-before);return elapsed>=1.5&&elapsed<8?0:1;}
