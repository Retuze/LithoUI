// viz_uniform.cpp — decisive test: rotate a UNIFORM-colour image.
// Bilinear of four identical texels must equal that texel, so a solid
// image rotated by any angle must keep its exact interior colour.
// If the interior comes out darker, the blend has a real bug.
#include "core/painter.hpp"
#include "core/tile.hpp"
#include <cstdio>
#include <cstring>
#include <cstdlib>
using namespace litho;

int main() {
    const int S = 60, C = 100;
    const uint16_t COL = 0x8430;        // a mid grey-green RGB565
    uint16_t src[S*S];
    uint8_t  alpha[S*S];
    for (int i=0;i<S*S;i++){ src[i]=COL; alpha[i]=255; }

    uint16_t* canvas=(uint16_t*)malloc(sizeof(uint16_t)*C*C);
    Tile tile; Painter p;

    auto interiorColor = [&](int angle)->uint16_t{
        for(int i=0;i<C*C;i++) canvas[i]=0x0000;
        tile.attach(canvas,C,C);
        p.setTile(tile,0,0); p.setScreenOrigin(20,20);
        p.setScreenClip(0,0,C,C); p.setAlpha(255);
        p.drawImageRotated(src,1,S,S,0,0,S/2,S/2,(int16_t)angle,alpha,nullptr);
        // sample dead-centre pixel (deep interior)
        return canvas[(C/2)*C + (C/2)];
    };

    auto luma=[&](uint16_t v){
        int r=(v>>11)&0x1F,g=(v>>5)&0x3F,b=v&0x1F;
        return 0.299*((r<<3)|(r>>2))+0.587*((g<<2)|(g>>4))+0.114*((b<<3)|(b>>2));
    };

    printf("source COL=0x%04X luma=%.2f\n", COL, luma(COL));
    int angs[] = {0,30,45,60,90,135};
    for (int k=0;k<6;k++) { int a=angs[k];
        uint16_t v=interiorColor(a);
        printf("  angle %3d: interior=0x%04X luma=%.2f  %s\n",
               a, v, luma(v), v==COL? "EXACT":"<-- CHANGED");
    }
    free(canvas);
    return 0;
}
