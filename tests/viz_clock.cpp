// viz_clock.cpp — offline clock renderer for verifying hand pivot/orientation.
// Draws a face + minute/second hands at a given time, dumps PPM.
//   ./viz_clock <out.ppm> <hh> <mm> <ss> [minPivotY] [secPivotY]
#include "core/painter.hpp"
#include "core/tile.hpp"
#include "res_images.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
using namespace litho;

static const double PI = 3.14159265358979323846;

static const int CV = 320, CX = 160, CY = 160;

static void aabbTopLeft(int sw,int sh,int px,int py,int ang,int&mnx,int&mny){
    int32_t c,s; switch(((ang%360)+360)%360){
    case 0:c=65536;s=0;break; case 90:c=0;s=65536;break;
    case 180:c=-65536;s=0;break; case 270:c=0;s=-65536;break;
    default:c=(int32_t)cosDeg(ang)<<1;s=(int32_t)sinDeg(ang)<<1;}
    int cn[4][2]={{0,0},{sw,0},{sw,sh},{0,sh}};mnx=0x7FFFFFFF;mny=0x7FFFFFFF;
    for(int i=0;i<4;i++){int rx=((int32_t)(cn[i][0]-px)*c-(int32_t)(cn[i][1]-py)*s)>>16;
        int ry=((int32_t)(cn[i][0]-px)*s+(int32_t)(cn[i][1]-py)*c)>>16;
        if(rx<mnx)mnx=rx; if(ry<mny)mny=ry;}
}

int main(int argc,char**argv){
    const char* out = argc>1?argv[1]:"build/clock.ppm";
    int hh=argc>2?atoi(argv[2]):10, mm=argc>3?atoi(argv[3]):8;
    double ss=argc>4?atof(argv[4]):30.0;     // fractional seconds → smooth
    int minPivY = argc>5?atoi(argv[5]):126;  // black-dot pivot in min hand
    int secPivY = argc>6?atoi(argv[6]):108;  // black-dot pivot in sec hand

    uint16_t* buf=(uint16_t*)malloc(sizeof(uint16_t)*CV*CV);
    Tile tile; Painter p;

    auto setpx=[&](int x,int y,uint16_t c){ if(x>=0&&x<CV&&y>=0&&y<CV) buf[y*CV+x]=c; };
    auto fillCircle=[&](int cx,int cy,int r,uint16_t c){
        for(int y=-r;y<=r;y++)for(int x=-r;x<=r;x++) if(x*x+y*y<=r*r) setpx(cx+x,cy+y,c);
    };
    auto ring=[&](int cx,int cy,int r,int th,uint16_t c){
        for(int y=-r;y<=r;y++)for(int x=-r;x<=r;x++){int d=x*x+y*y; if(d<=r*r&&d>=(r-th)*(r-th)) setpx(cx+x,cy+y,c);}
    };

    // ── face ──
    uint16_t bg   = RGB565::fromRGB(18,20,26).value;
    uint16_t face = RGB565::fromRGB(34,38,52).value;
    uint16_t rim  = RGB565::fromRGB(90,100,130).value;
    uint16_t tick = RGB565::fromRGB(210,215,230).value;
    uint16_t hub  = RGB565::fromRGB(230,170,60).value;
    for(int i=0;i<CV*CV;i++) buf[i]=bg;
    fillCircle(CX,CY,150,face);
    ring(CX,CY,150,4,rim);
    // 12 ticks
    for(int k=0;k<12;k++){
        double a=k*PI/6.0; int rr=(k%3==0)?9:5;
        int tx=CX+(int)(132*sin(a)), ty=CY-(int)(132*cos(a));
        fillCircle(tx,ty,rr/2+2,tick);
    }

    tile.attach(buf,CV,CV);

    auto drawHand=[&](ImageId id,int pivY,int angleDeci,const RGB565* tintc){
        const ImageEntry* e=imageEntry(id);
        const void* src=(const void*)imagePixels(id);
        const uint8_t* al=imageAlpha(id);
        int sw=e->width, sh=e->height, pivX=sw/2;
        int mnx,mny; aabbTopLeft(sw,sh,pivX,pivY,(angleDeci+5)/10,mnx,mny);
        p.setTile(tile,0,0);
        p.setScreenOrigin(CX+mnx, CY+mny);
        p.setScreenClip(0,0,CV,CV);
        p.setAlpha(255);
        p.drawImageRotatedDeci(src,e->format,sw,sh,0,0,pivX,pivY,angleDeci,al,tintc);
    };

    int minDeci = (int)((mm + ss/60.0)*60.0 + 0.5);   // 0.1° units
    int secDeci = (int)(ss*60.0 + 0.5);
    (void)hh;
    drawHand(IMG_R_MIN, minPivY, minDeci, nullptr);
    RGB565 red = RGB565::fromRGB(220,70,70);
    drawHand(IMG_R_SEC, secPivY, secDeci, &red);
    // hub = the black dot baked into the sprites; no extra dot drawn.

    FILE* f=fopen(out,"wb");
    fprintf(f,"P6\n%d %d\n255\n",CV,CV);
    for(int i=0;i<CV*CV;i++){uint16_t v=buf[i];
        int r=(v>>11)&0x1F,g=(v>>5)&0x3F,b=v&0x1F;
        uint8_t o3[3]={(uint8_t)((r<<3)|(r>>2)),(uint8_t)((g<<2)|(g>>4)),(uint8_t)((b<<3)|(b>>2))};
        fwrite(o3,1,3,f);}
    fclose(f);
    printf("wrote %s  time=%02d:%02d:%06.3f secDeci=%d minDeci=%d\n",
           out,hh,mm,ss,secDeci,minDeci);
    free(buf);
    return 0;
}
