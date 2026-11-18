# OXIDE icon, in the family of the O6, O8 and NO badges: "OX", the ring O with its satellite, then a condensed X.
# python3 tools/oxide-logo.py  -> Source/Assets/orbit-icon.png (1024 px, the app icon and the panel badge)
from PIL import Image, ImageDraw, ImageFilter
import os
S=4; W=1024*S
night=(132,158,40,255); grey=(200,206,212,255); dark=(29,30,31,255); edge=(58,58,60,255) # night = the acid green
def r(v): return int(v*S)
im=Image.new('RGBA',(W,W),(0,0,0,0))
sh=Image.new('RGBA',(W,W),(0,0,0,0)); ImageDraw.Draw(sh).rounded_rectangle([r(102),r(114),r(922),r(934)],radius=r(176),fill=(0,0,0,70))
im=Image.alpha_composite(im,sh.filter(ImageFilter.GaussianBlur(r(6))))
box=[r(102),r(102),r(920),r(920)]
tile=Image.new('RGBA',(W,W),(0,0,0,0)); t=ImageDraw.Draw(tile)
t.rectangle([0,0,W,r(730)],fill=dark); t.rectangle([0,r(730),W,r(826)],fill=grey); t.rectangle([0,r(826),W,W],fill=night)
mask=Image.new('L',(W,W),0); ImageDraw.Draw(mask).rounded_rectangle(box,radius=r(176),fill=255)
im.paste(tile,(0,0),mask); d=ImageDraw.Draw(im); d.rounded_rectangle(box,radius=r(176),outline=edge,width=r(5))
# the O: the ring in light grey, its satellite in acid green with a dark halo (where the 6 and the 8 have theirs)
cx,cy=377,421
d.ellipse([r(cx-167),r(cy-167),r(cx+167),r(cy+167)],fill=grey); d.ellipse([r(cx-110),r(cy-110),r(cx+110),r(cy+110)],fill=dark)
d.ellipse([r(474-44),r(324-44),r(474+44),r(324+44)],fill=dark); d.ellipse([r(474-28),r(324-28),r(474+28),r(324+28)],fill=night)
# the X: two condensed strokes, same height and stroke as the 6 and the 8, in acid green
x0,x1,top,bot,st=585,748,258,582,58
d.polygon([(r(x0),r(top)),(r(x0+st),r(top)),(r(x1),r(bot)),(r(x1-st),r(bot))],fill=night)
d.polygon([(r(x1-st),r(top)),(r(x1),r(top)),(r(x0+st),r(bot)),(r(x0),r(bot))],fill=night)
root=os.path.join(os.path.dirname(__file__),'..')
im.resize((1024,1024),Image.LANCZOS).save(os.path.join(root,'Source/Assets/orbit-icon.png'))
