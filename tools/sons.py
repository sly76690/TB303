# OXIDE - writes the factory sounds (Presets/Sounds), embedded in the plugin and installed with it.
# Each sound only lists what differs from the defaults: Oxide fills in the rest when it loads it.
# Every sound is programmed from scratch on Oxide; no patch of any other instrument is copied.
# Usage:  python3 tools/sons.py
import os, glob
from xml.sax.saxutils import quoteattr

# wave: 0 saw, 1 square. cutoff in Hz, decay / slideTime / ampDecay in seconds, the rest 0..1.
SOUNDS = {
    "ACID - First Light": dict(wave=0, cutoff=380, resonance=.55, envMod=.5, decay=.6, accent=.55),
    "ACID - Rubber Squelch": dict(wave=1, cutoff=320, resonance=.78, envMod=.62, decay=.45, accent=.7),
    "ACID - Night Shift": dict(wave=0, cutoff=300, resonance=.7, envMod=.55, decay=.8, accent=.6,
        delaySync=5, delayFeedback=.42, delayMix=.18, reverbMix=.1),
    "ACID - Hard Rain": dict(wave=0, cutoff=420, resonance=.68, envMod=.6, decay=.5, accent=.75, drive=.55, driveTone=.5, gain=.6),
    "ACID - Soft Wood": dict(wave=1, cutoff=260, resonance=.35, envMod=.35, decay=.9, accent=.4, ampDecay=1.6),
    "ACID - Screamer": dict(wave=0, cutoff=500, resonance=.92, envMod=.75, decay=.35, accent=.85, drive=.75, driveTone=.65, gain=.55,
        delaySync=6, delayFeedback=.3, delayMix=.1),
    "BASS - Deep Sub": dict(wave=1, cutoff=180, resonance=.2, envMod=.2, decay=1.2, accent=.3, ampDecay=4),
    "ACID - Dub Echo": dict(wave=0, cutoff=340, resonance=.66, envMod=.45, decay=.7, accent=.6,
        delaySync=2, delayFeedback=.62, delayMod=.3, delayMix=.28, reverbType=2, reverbSize=.7, reverbMix=.18),
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
