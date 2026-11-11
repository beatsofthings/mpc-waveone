#!/usr/bin/env python3
"""Generate a 3D-look wavetable morph filmstrip for Open-MPC skin knobs.
Frames are vertically stacked, matching the skin studio's strip= format.
These are parameter-driven frames, not continuous GPU animation.
"""
import math
from pathlib import Path
from PIL import Image, ImageDraw
SIZE=192
FRAMES=64
out=Path("assets/wavetable_3d_strip.png")
out.parent.mkdir(parents=True,exist_ok=True)
sheet=Image.new("RGBA",(SIZE,SIZE*FRAMES),(0,0,0,0))
for frame in range(FRAMES):
    im=Image.new("RGBA",(SIZE,SIZE),(0,0,0,0))
    d=ImageDraw.Draw(im)
    d.rounded_rectangle((2,2,SIZE-3,SIZE-3),radius=16,fill=(10,22,36,255),outline=(43,94,112,255),width=2)
    pos=2*frame/(FRAMES-1)
    for depth in range(17,-1,-1):
        yoff=(depth-8)*5
        xoff=(depth-8)*2.4
        wavepos=2*depth/17
        points=[]
        for x in range(150):
            phase=x/150
            sine=math.sin(phase*math.tau*2)
            saw=2*((phase*2)%1)-1
            square=1 if (phase*2)%1<0.5 else -1
            if wavepos<1: amp=sine*(1-wavepos)+saw*wavepos
            else: amp=saw*(2-wavepos)+square*(wavepos-1)
            points.append((20+x+xoff*.5,95+yoff-amp*23))
        active=abs(wavepos-pos)<.075
        color=(95,242,245,255) if active else (33+int(depth*2),95+int(depth*4),145+int(depth*3),210)
        d.line(points,fill=color,width=3 if active else 1,joint="curve")
    d.text((13,12),"WAVEONE  /  WT 3D",fill=(135,231,243,255))
    d.text((13,170),f"POSITION {pos:.2f}",fill=(120,190,205,255))
    sheet.paste(im,(0,frame*SIZE))
sheet.save(out,optimize=True)
print(out, sheet.size)
