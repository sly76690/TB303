#pragma once
#include <juce_core/juce_core.h>
#include <FactoryData.h>

// OXIDE factory bank: the factory sounds (Presets/Sounds) embedded in the binary, played by BANK and PATCH 1-8 in the
// order below, so the program buttons give Oxide's own sounds even before the installer ran.
class FactoryBank {
public:
 static const juce::StringArray& order() {
  static const juce::StringArray names{
   // A: the acid classics
   "ACID - First Light","ACID - Rubber Squelch","ACID - Night Shift","ACID - Hard Rain",
   "ACID - Soft Wood","ACID - Screamer","BASS - Deep Sub","ACID - Dub Echo",
   // B: through the DISTORTION
   "DIST - Rust Belt","DIST - Acid Rain","DIST - Iron Lung","DIST - Burnt Fuse",
   "DIST - Gravel Road","DIST - Warehouse","DIST - Short Circuit","DIST - Black Smoke",
   // C: basses, deep and dub
   "BASS - Round Floor","BASS - Rubber Sub","BASS - Dub Pressure","BASS - Short Pluck",
   "BASS - Warm Tube","BASS - Slide Walker","BASS - Minimal Thump","BASS - Basement",
   // D: high and squelchy
   "SQUELCH - Glass Duck","SQUELCH - Wet Rubber","SQUELCH - Laser Worm","SQUELCH - Chirp",
   "SQUELCH - Acid Bird","SQUELCH - Hot Wire","SQUELCH - Neon Tube","SQUELCH - Scream Lead",
   // E: textures and effects
   "FX - Echo Chamber","FX - Radio Ghost","FX - Long Fall","FX - Spring Coil",
   "FX - Overload","FX - Low Orbit","FX - Static Storm","FX - Night Drive"};
  return names;
 }
 static std::unique_ptr<juce::XmlElement> load(const juce::String& name) {
  for(int i=0;i<FactoryData::namedResourceListSize;++i)
   if(juce::String::fromUTF8(FactoryData::originalFilenames[i]).upToLastOccurrenceOf(".orbitpreset",false,false)==name){
    int size=0;const auto* data=FactoryData::getNamedResource(FactoryData::namedResourceList[i],size);
    return data?juce::XmlDocument::parse(juce::String::fromUTF8(data,size)):nullptr;}
  return {};
 }
};
