#include "bflib_fmvids.h"
extern "C" TbBool play_smk(const char*,int){return false;}
extern "C" short anim_stop(){return 0;}
extern "C" short anim_record(){return 0;}
extern "C" TbBool anim_record_frame(unsigned char*,unsigned char*){return false;}
