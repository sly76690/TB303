# OXIDE - writes the factory sounds (Presets/Sounds), embedded in the plugin and installed with it.
# Each sound only lists what differs from the defaults: Oxide fills in the rest when it loads it.
# Every sound is programmed from scratch on Oxide; no patch of any other instrument is copied.
# The sound is the knobs only: play it with any of the 16 patterns (they live with the session, not with the sounds).
# Usage:  python3 tools/sons.py   (the order of the BANK / PATCH keys is in Source/FactoryBank.h)
import os, glob
from xml.sax.saxutils import quoteattr

# wave: 0 saw, 1 square. cutoff in Hz; decay, slideTime, ampDecay in seconds; driveMode 0 OD, 1 DIST; the rest 0..1.
# delaySync: 0 free, 2 1/4 D, 3 1/4, 5 1/8 D, 6 1/8, 8 1/16 D, 9 1/16. reverbType: 0 VINT, 1 PLATE, 2 SPACE.
ECHO8D = dict(delaySync=5, delayFeedback=.4, delayMix=.16)
ECHO4D = dict(delaySync=2, delayFeedback=.55, delayMod=.25, delayMix=.22)
ROOM = dict(reverbType=1, reverbSize=.35, reverbMix=.08)
HALL = dict(reverbType=2, reverbSize=.75, reverbMix=.18)
OD = lambda d, t=.5: dict(driveMode=0, drive=d, driveTone=t)
DIST = lambda d, t=.45: dict(driveMode=1, drive=d, driveTone=t)

SOUNDS = {
    # ==== A: the acid classics
    "ACID - First Light": dict(wave=0, cutoff=380, resonance=.55, envMod=.5, decay=.6, accent=.55),
    "ACID - Rubber Squelch": dict(wave=1, cutoff=320, resonance=.78, envMod=.62, decay=.45, accent=.7),
    "ACID - Night Shift": dict(wave=0, cutoff=300, resonance=.7, envMod=.55, decay=.8, accent=.6, **ECHO8D, reverbMix=.1),
    "ACID - Hard Rain": dict(wave=0, cutoff=420, resonance=.68, envMod=.6, decay=.5, accent=.75, **OD(.55)),
    "ACID - Soft Wood": dict(wave=1, cutoff=260, resonance=.35, envMod=.35, decay=.9, accent=.4, ampDecay=1.6),
    "ACID - Screamer": dict(wave=0, cutoff=500, resonance=.92, envMod=.75, decay=.35, accent=.85, **OD(.75, .65), delaySync=6, delayFeedback=.3, delayMix=.1),
    "BASS - Deep Sub": dict(wave=1, cutoff=180, resonance=.2, envMod=.2, decay=1.2, accent=.3, ampDecay=4),
    "ACID - Dub Echo": dict(wave=0, cutoff=340, resonance=.66, envMod=.45, decay=.7, accent=.6, **ECHO4D, reverbType=2, reverbSize=.7, reverbMix=.18),
    # ==== B: through the DISTORTION
    "DIST - Rust Belt": dict(wave=0, cutoff=360, resonance=.72, envMod=.58, decay=.5, accent=.7, **DIST(.6)),
    "DIST - Acid Rain": dict(wave=0, cutoff=300, resonance=.85, envMod=.7, decay=.4, accent=.8, **DIST(.8, .4), **ECHO8D),
    "DIST - Iron Lung": dict(wave=1, cutoff=280, resonance=.7, envMod=.5, decay=.55, accent=.65, **DIST(.7, .35)),
    "DIST - Burnt Fuse": dict(wave=0, cutoff=450, resonance=.95, envMod=.8, decay=.3, accent=.9, **DIST(1.0, .5)),
    "DIST - Gravel Road": dict(wave=1, cutoff=220, resonance=.45, envMod=.4, decay=.8, accent=.5, **DIST(.45, .3), **ROOM),
    "DIST - Warehouse": dict(wave=0, cutoff=330, resonance=.78, envMod=.62, decay=.45, accent=.75, **DIST(.65, .55), **HALL),
    "DIST - Short Circuit": dict(wave=1, cutoff=600, resonance=.6, envMod=.65, decay=.25, accent=.8, slideTime=.03, **DIST(.85, .6)),
    "DIST - Black Smoke": dict(wave=0, cutoff=200, resonance=.82, envMod=.7, decay=.6, accent=.7, **DIST(.9, .25), delaySync=3, delayFeedback=.35, delayMix=.12),
    # ==== C: basses, deep and dub
    "BASS - Round Floor": dict(wave=1, cutoff=200, resonance=.25, envMod=.3, decay=.9, accent=.35, ampDecay=3),
    "BASS - Rubber Sub": dict(wave=1, cutoff=160, resonance=.5, envMod=.35, decay=.6, accent=.4, **OD(.3, .35)),
    "BASS - Dub Pressure": dict(wave=0, cutoff=180, resonance=.4, envMod=.3, decay=1.0, accent=.45, ampDecay=2.5, **ECHO4D, **HALL),
    "BASS - Short Pluck": dict(wave=0, cutoff=260, resonance=.35, envMod=.55, decay=.25, accent=.5, ampDecay=.4),
    "BASS - Warm Tube": dict(wave=1, cutoff=240, resonance=.3, envMod=.4, decay=.7, accent=.4, **OD(.45, .3)),
    "BASS - Slide Walker": dict(wave=0, cutoff=230, resonance=.45, envMod=.35, decay=.8, accent=.35, slideTime=.12),
    "BASS - Minimal Thump": dict(wave=1, cutoff=140, resonance=.15, envMod=.5, decay=.2, accent=.6, ampDecay=.5),
    "BASS - Basement": dict(wave=0, cutoff=170, resonance=.6, envMod=.45, decay=.9, accent=.5, **DIST(.35, .25), **ROOM),
    # ==== D: high and squelchy
    "SQUELCH - Glass Duck": dict(wave=0, cutoff=700, resonance=.9, envMod=.6, decay=.35, accent=.7, tune=12),
    "SQUELCH - Wet Rubber": dict(wave=1, cutoff=550, resonance=.88, envMod=.7, decay=.3, accent=.75, **ECHO8D),
    "SQUELCH - Laser Worm": dict(wave=0, cutoff=900, resonance=.94, envMod=.5, decay=.5, accent=.6, slideTime=.15, tune=12, **ECHO8D),
    "SQUELCH - Chirp": dict(wave=1, cutoff=1200, resonance=.8, envMod=.75, decay=.2, accent=.8, ampDecay=.6, tune=12),
    "SQUELCH - Acid Bird": dict(wave=0, cutoff=800, resonance=.92, envMod=.8, decay=.28, accent=.9, tune=12, delaySync=9, delayFeedback=.35, delayMix=.14),
    "SQUELCH - Hot Wire": dict(wave=0, cutoff=650, resonance=.86, envMod=.65, decay=.4, accent=.85, tune=12, **OD(.6, .7)),
    "SQUELCH - Neon Tube": dict(wave=1, cutoff=750, resonance=.75, envMod=.55, decay=.5, accent=.6, tune=12, **ECHO8D, **HALL),
    "SQUELCH - Scream Lead": dict(wave=0, cutoff=500, resonance=.9, envMod=.7, decay=.6, accent=.8, slideTime=.1, tune=12, **DIST(.7, .55), **ECHO8D),
    # ==== E: textures and effects
    "FX - Echo Chamber": dict(wave=0, cutoff=380, resonance=.75, envMod=.55, decay=.5, accent=.6, delaySync=5, delayFeedback=.8, delayMod=.4, delayMix=.3, reverbType=2, reverbSize=.85, reverbMix=.25),
    "FX - Radio Ghost": dict(wave=1, cutoff=900, resonance=.6, envMod=.3, decay=1.5, accent=.4, tune=12, delaySync=2, delayFeedback=.7, delayMod=.6, delayMix=.3, **HALL),
    "FX - Long Fall": dict(wave=0, cutoff=250, resonance=.8, envMod=.9, decay=2.5, accent=.5, slideTime=.4, ampDecay=6, **HALL),
    "FX - Spring Coil": dict(wave=1, cutoff=420, resonance=.93, envMod=.4, decay=.15, accent=.9, ampDecay=.35, delaySync=8, delayFeedback=.5, delayMix=.18),
    "FX - Overload": dict(wave=0, cutoff=600, resonance=.9, envMod=.85, decay=.4, accent=1, **DIST(1.0, .7), delaySync=3, delayFeedback=.5, delayMix=.15, reverbMix=.12),
    "FX - Low Orbit": dict(wave=1, cutoff=120, resonance=.85, envMod=.6, decay=1.8, accent=.5, slideTime=.3, tune=-12, **ECHO4D),
    "FX - Static Storm": dict(wave=0, cutoff=1500, resonance=.7, envMod=.4, decay=.3, accent=.7, tune=12, **DIST(.8, .8), delaySync=9, delayFeedback=.6, delayMix=.2),
    "FX - Night Drive": dict(wave=0, cutoff=320, resonance=.68, envMod=.5, decay=.7, accent=.6, slideTime=.08, **OD(.35, .45), **ECHO8D, reverbType=2, reverbSize=.6, reverbMix=.14),
}

folder = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Presets", "Sounds")
os.makedirs(folder, exist_ok=True)
for old in glob.glob(os.path.join(folder, "*.orbitpreset")):   # sounds no longer in the list are removed; the others are rewritten
    if os.path.basename(old)[:-len(".orbitpreset")] not in SOUNDS:
        os.remove(old)
for name, settings in SOUNDS.items():
    lines = ['<?xml version="1.0" encoding="UTF-8"?>', '<OrbitState stateVersion="1" program="0" modified="0">']
    lines += ['  <PARAM id=%s value="%s"/>' % (quoteattr(key), repr(float(value))) for key, value in settings.items()]
    lines.append('</OrbitState>')
    with open(os.path.join(folder, name + ".orbitpreset"), "w", encoding="utf-8") as handle:
        handle.write("\n".join(lines) + "\n")
    print("écrit :", name)
print(len(SOUNDS), "sons")
