#include "core/painter.hpp"
#include "core/tile.hpp"
#include <cstdio>
#include <cstring>
using namespace litho;

int main() {
    int srcW = 8, srcH = 4;
    uint16_t src[32];
    for (int y = 0; y < srcH; y++)
        for (int x = 0; x < srcW; x++)
            src[y*srcW+x] = 0x4100 + y*srcW + x;

    uint16_t out[40*40];
    Tile tile;
    Painter p;
    memset(out, 0xCD, sizeof(out));
    tile.attach(out, 40, 40);

    printf("=== 45° rotation, 8×4 source, pivot(0,0) ===\n\n");

    p.setTile(tile, 0, 0);
    p.setScreenOrigin(0, 0);
    p.setScreenClip(0, 0, 40, 40);
    p.setAlpha(255);
    p.drawImageRotated(src, 0, srcW, srcH, 5, 5, 0, 0, 45);

    printf("Bounding box scan:\n");
    int minX=40,minY=40,maxX=-1,maxY=-1;
    for (int y=0;y<40;y++) for (int x=0;x<40;x++)
        if (out[y*40+x] != 0xCDCD) {
            if(x<minX)minX=x;if(y<minY)minY=y;
            if(x>maxX)maxX=x;if(y>maxY)maxY=y;
        }

    printf("  bbox=(%d,%d)-(%d,%d) %dx%d\n",minX,minY,maxX,maxY,maxX-minX+1,maxY-minY+1);

    printf("\nHex dump of rendered area:\n");
    for (int y=minY-1; y<=maxY+1; y++) {
        printf("%2d: ", y);
        for (int x=minX-1; x<=maxX+1; x++) {
            uint16_t v = out[y*40+x];
            if (v == 0xCDCD) printf(" -- ");
            else printf("%04x ", v);
        }
        printf("\n");
    }

    // Also check: what source pixel does output(0,0) correspond to?
    printf("\nSource layout (row, col → value):\n");
    for (int y=0; y<srcH; y++) {
        for (int x=0; x<srcW; x++)
            printf("%04x ", src[y*srcW+x]);
        printf("\n");
    }

    // Map rendered output pixels back to source indices
    printf("\nOutput → source mapping:\n");
    for (int y=0; y<=maxY-minY; y++) {
        for (int x=0; x<=maxX-minX; x++) {
            uint16_t v = out[(minY+y)*40+(minX+x)];
            if (v == 0xCDCD) { printf("  -- "); continue; }
            int idx = v - 0x4100;
            int sx = idx % srcW, sy = idx / srcW;
            printf("(%d,%d) ", sx, sy);
        }
        printf("\n");
    }

    return 0;
}
