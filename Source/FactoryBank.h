#pragma once
#include <juce_core/juce_core.h>
#include <FactoryData.h>

// OXIDE factory bank: the factory sounds (Presets/Sounds) embedded in the binary, played by BANK and PATCH 1-8 in the
// order below, so the program buttons give Oxide's own sounds even before the installer ran.
class FactoryBank {
public:
 static const juce::StringArray& order() {
  static const juce::StringArray names{
   // A: the first acid sounds
   "ACID - First Light","ACID - Rubber Squelch","ACID - Night Shift","ACID - Hard Rain",
   "ACID - Soft Wood","ACID - Screamer","BASS - Deep Sub","ACID - Dub Echo"};
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
