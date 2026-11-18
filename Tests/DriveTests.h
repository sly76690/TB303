#pragma once
// DRIVE: both modes clean (finite, under full scale), at about the loudness of each other; DISTORTION harder-edged.
static bool driveTests(){
 auto require=[](bool ok,const juce::String& what){if(!ok)std::cerr<<"DRIVE: "<<what<<"\n";return ok;};
 auto render=[](int mode,float drive){std::unique_ptr<juce::AudioProcessor> q(createPluginFilter());setValue(*q,"driveMode",float(mode));setValue(*q,"drive",drive);setValue(*q,"delayMix",0);setValue(*q,"reverbMix",0);
  q->setRateAndBufferSizeDetails(48000,512);q->prepareToPlay(48000,512);std::vector<float> out;juce::AudioBuffer<float> b(2,512);
  for(int k=0;k<120;++k){juce::MidiBuffer m;if(k%12==0)m.addEvent(juce::MidiMessage::noteOn(1,36+(k/12)%3*7,k%24?.6f:1.f),0);if(k%12==8)m.addEvent(juce::MidiMessage::allNotesOff(1),0);b.clear();q->processBlock(b,m);out.insert(out.end(),b.getReadPointer(0),b.getReadPointer(0)+512);}return out;};
 auto rms=[](const std::vector<float>& x){double e=0;for(float v:x)e+=double(v)*v;return std::sqrt(e/double(x.size()));};
 auto edge=[](const std::vector<float>& x){double d=0,l=0;for(size_t i=1;i<x.size();++i){d+=double(x[i]-x[i-1])*(x[i]-x[i-1]);l+=double(x[i])*x[i];}return d/std::max(l,1e-12);};
 const double dry=rms(render(0,0));
 for(float drive:{.25f,.6f,1.f}){const auto od=render(0,drive),dist=render(1,drive);
  for(const auto* x:{&od,&dist})for(float v:*x)if(!require(std::isfinite(v)&&std::abs(v)<1.f,"clean output"))return false;
  const double a=juce::Decibels::gainToDecibels(rms(od)/dry),b=juce::Decibels::gainToDecibels(rms(dist)/dry);
  std::cout<<"  drive "<<drive<<": overdrive "<<a<<" dB, distortion "<<b<<" dB, edge "<<edge(od)<<" / "<<edge(dist)<<"\n";
  if(!require(std::abs(a)<6&&std::abs(b)<6,"level kept within 6 dB of the clean sound"))return false;
  if(drive>.5f&&!require(edge(dist)>edge(od)*1.3,"distortion harder-edged than overdrive"))return false;}
 std::cout<<"PASS drive: overdrive and distortion, clean, level-matched, distortion the harder one\n";return true;
}
