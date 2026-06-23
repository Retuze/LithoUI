#!/usr/bin/env python3
"""spin_report.py — assemble viz_spin frames into a GIF + PNGs and flag flicker.

Usage: python3 tools/spin_report.py [frames_dir]   (default build/spin_frames)

Outputs (next to the frames dir):
    spin.gif          one full rotation as a flip-book (24 fps)
    png/frame_NNN.png individual frames for scrubbing
Prints a per-frame opaque-pixel count and flags angles where the count jumps
sharply between adjacent frames — the signature of edge/coverage flicker.
"""
import sys, glob, os
from PIL import Image

frames_dir = sys.argv[1] if len(sys.argv) > 1 else "build/spin_frames"
ppms = sorted(glob.glob(os.path.join(frames_dir, "frame_*.ppm")))
if not ppms:
    print(f"no frames in {frames_dir}")
    sys.exit(1)

png_dir = os.path.join(frames_dir, "png")
os.makedirs(png_dir, exist_ok=True)

imgs, counts = [], []
# background colour written by viz_spin (RGB565 0x18E3 -> RGB888)
BG = (0x18, 0x1C, 0x18)
for p in ppms:
    im = Image.open(p).convert("RGB")
    imgs.append(im)
    im.save(os.path.join(png_dir, os.path.basename(p).replace(".ppm", ".png")))
    counts.append(sum(1 for px in im.getdata() if px != BG))

out_gif = os.path.join(os.path.dirname(frames_dir) or ".", "spin.gif")
imgs[0].save(out_gif, save_all=True, append_images=imgs[1:],
             duration=1000 // 24, loop=0, disposal=2)
print(f"wrote {out_gif}  ({len(imgs)} frames)")
print(f"wrote {len(imgs)} PNGs to {png_dir}/")

# Flicker heuristic: opaque pixel count should vary smoothly as the icon spins.
# A sharp jump versus the local trend = visible flicker at that angle.
n = len(counts)
print("\nflicker scan (Δ opaque pixels vs previous frame):")
flagged = []
for i in range(n):
    d = counts[i] - counts[(i - 1) % n]
    if abs(d) >= 40:               # tune: ~edge-row's worth of pixels
        flagged.append((i, counts[i], d))
if flagged:
    for ang, c, d in flagged:
        print(f"  angle {ang:3d}°  opaque={c:5d}  Δ={d:+d}")
else:
    print("  no sharp jumps (>=40 px) — rotation coverage looks stable")
print(f"\nopaque px: min={min(counts)} max={max(counts)} "
      f"spread={max(counts)-min(counts)}")
