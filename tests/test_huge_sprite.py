"""Run the production campaign frame decoder under address/undefined sanitizers."""
from pathlib import Path
import subprocess
import os
import tempfile

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT/'src/kfx/renderer/software/bflib_vidraw.c').read_text()
start = source.index('static TbResult LbHugeSpriteDrawUsingScalingUpData(')
end = source.index('/** Draws a huge sprite', start)
preamble = r"""
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
typedef unsigned char uchar;
typedef int TbResult;
#define Lb_FAIL -1
#define Lb_SUCCESS 1
#define SPRITE_SCALING_XSTEPS 8192
#define SPRITE_SCALING_YSTEPS 8192
struct TbHugeSprite { unsigned char *Data; int32_t *Lines; unsigned long SWidth, SHeight; };
"""
harness = r"""
int main(int argc, char **argv) {
    unsigned char data[] = {4,0,0,0,1,2,3,4};
    int32_t lines[] = {0}, xs[] = {-2,2,0,3,3,3,6,3}, ys[] = {-1,3};
    struct TbHugeSprite sprite = {data,lines,4,1};
    unsigned char buf[28]; memset(buf, 99, sizeof(buf));
    assert(LbHugeSpriteDrawUsingScalingUpData(buf+2,6,4,xs,ys,&sprite,8)==Lb_SUCCESS);
    for(int y=0;y<2;y++) for(int x=0;x<6;x++) assert(buf[2+y*6+x]==(x<3?2:3));
    assert(buf[0]==99 && buf[1]==99 && buf[26]==99 && buf[27]==99);
    for(int i=14;i<26;i++) assert(buf[i]==99);
    assert(LbHugeSpriteDrawUsingScalingUpData(buf+2,6,4,xs,ys,&sprite,7)==Lb_FAIL);
    data[0]=255;
    assert(LbHugeSpriteDrawUsingScalingUpData(buf+2,6,4,xs,ys,&sprite,8)==Lb_FAIL);
    lines[0]=-1;
    assert(LbHugeSpriteDrawUsingScalingUpData(buf+2,6,4,xs,ys,&sprite,8)==Lb_FAIL);
    if(argc>1) {
        FILE *f=fopen(argv[1],"rb"); assert(f); fseek(f,0,SEEK_END); long size=ftell(f); rewind(f);
        unsigned char *raw=malloc(size); assert(fread(raw,1,size,f)==(size_t)size); fclose(f);
        sprite=(struct TbHugeSprite){raw+720*4,(int32_t*)raw,960,720};
        int32_t xsteps[1920], ysteps[1440];
        int dims[][2]={{640,480},{1736,1133},{3840,2160}};
        for(int mode=0;mode<3;mode++) for(int shift=-300;shift<=300;shift+=300) {
            int w=dims[mode][0],h=dims[mode][1];
            for(int x=0;x<960;x++){xsteps[2*x]=x*w/960+shift;xsteps[2*x+1]=(x+1)*w/960-x*w/960;}
            for(int y=0;y<720;y++){ysteps[2*y]=y*h/720+shift;ysteps[2*y+1]=(y+1)*h/720-y*h/720;}
            unsigned char *out=calloc((size_t)w*h,1);
            assert(LbHugeSpriteDrawUsingScalingUpData(out,w,h,xsteps,ysteps,&sprite,size-720*4)==Lb_SUCCESS);
            free(out);
        }
        free(raw);
    }
    puts("Campaign frame clipping and malformed-data checks passed.");
}
"""
with tempfile.TemporaryDirectory() as directory:
    tmp=Path(directory)
    (tmp/'test.c').write_text(preamble+source[start:end]+harness)
    subprocess.run(['cc','-fsanitize=address,undefined','-g','-O1',str(tmp/'test.c'),'-o',str(tmp/'test')],check=True)
    args=[str(tmp/'test')]
    frame=Path(os.environ.get('KFX_TEST_FRAME', ROOT/'.local/imported-client/campgns/keeporig_lnd/viframe00.dat'))
    if frame.is_file() and (ROOT/'tools/local/bin/dernc').is_file():
        subprocess.run([str(ROOT/'tools/local/bin/dernc'),'-o',str(frame),str(tmp/'frame.dat')],check=True)
        args.append(str(tmp/'frame.dat'))
    subprocess.run(args,check=True)
