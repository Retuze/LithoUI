// viz_clockapp.cpp — render hands through the REAL ImageView/ViewGroup path
// to verify the pivot stays pinned at the clock centre across angles.
#include "core/painter.hpp"
#include "core/tile.hpp"
#include "framework/view/view_group.hpp"
#include "framework/widget/image_view.hpp"
#include "res_images.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace litho;

static const int CV=320, CX=160, CY=160;

static void dump(const char* path, uint16_t* b){
    FILE* f=fopen(path,"wb"); fprintf(f,"P6\n%d %d\n255\n",CV,CV);
    for(int i=0;i<CV*CV;i++){uint16_t v=b[i];int r=(v>>11)&0x1F,g=(v>>5)&0x3F,bl=v&0x1F;
        uint8_t o[3]={(uint8_t)((r<<3)|(r>>2)),(uint8_t)((g<<2)|(g>>4)),(uint8_t)((bl<<3)|(bl>>2))};
        fwrite(o,1,3,f);} fclose(f);
}

int main(int argc,char**argv){
    int mm=argc>1?atoi(argv[1]):8;
    double ss=argc>2?atof(argv[2]):30.0;     // fractional seconds
    const char* out=argc>3?argv[3]:"build/clockapp.ppm";

    uint16_t* buf=(uint16_t*)malloc(sizeof(uint16_t)*CV*CV);
    for(int i=0;i<CV*CV;i++) buf[i]=RGB565::fromRGB(30,34,46).value;
    for(int x=0;x<CV;x++) buf[CY*CV+x]=RGB565::fromRGB(70,70,90).value;
    for(int y=0;y<CV;y++) buf[y*CV+CX]=RGB565::fromRGB(70,70,90).value;
    Tile tile; tile.attach(buf,CV,CV);

    ViewGroup root; root.bounds()={0,0,CV,CV};

    const ImageEntry* em=imageEntry(IMG_R_MIN);
    ImageView* mn=new ImageView(em->width,em->height);
    mn->bounds()={(int16_t)(CX-8),(int16_t)(CY-126),(int16_t)em->width,(int16_t)em->height};
    mn->setImageId(IMG_R_MIN);
    root.addView(mn);

    const ImageEntry* es=imageEntry(IMG_R_SEC);
    ImageView* sc=new ImageView(es->width,es->height);
    sc->bounds()={(int16_t)(CX-7),(int16_t)(CY-108),(int16_t)es->width,(int16_t)es->height};
    sc->setImageId(IMG_R_SEC);
    RGB565 red=RGB565::fromRGB(220,70,70); sc->setTintColor(red);
    root.addView(sc);

    int minDeci=(int)((mm + ss/60.0)*60.0 + 0.5);   // 0.1° units (6°/min ×10)
    int secDeci=(int)(ss*60.0 + 0.5);               // 6°/s ×10
    mn->setRotationAngleDeci(minDeci, 8, 126);
    sc->setRotationAngleDeci(secDeci, 7, 108);

    Painter p; p.setTile(tile,0,0); p.setScreenOrigin(0,0);
    p.setScreenClip(0,0,CV,CV); p.setAlpha(255);
    root.onDraw(p);

    // tip of the second hand = reddest pixel farthest from centre
    int tx=-1,ty=-1; double best=-1;
    for(int y=0;y<CV;y++)for(int x=0;x<CV;x++){uint16_t v=buf[y*CV+x];
        int r=((v>>11)&0x1F)<<3,g=((v>>5)&0x3F)<<2,b=(v&0x1F)<<3;
        if(r>120&&r>g+40&&r>b+40){double d=(x-CX)*(x-CX)+(y-CY)*(y-CY);if(d>best){best=d;tx=x;ty=y;}}}

    dump(out,buf);
    printf("ss=%.3f secDeci=%d (%.1f°)  sec-tip=(%d,%d)\n",ss,secDeci,secDeci/10.0,tx,ty);
    free(buf); return 0;
}
