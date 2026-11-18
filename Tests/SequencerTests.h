#pragma once
// OXIDE pattern sequencer: steps and rests in time, slide holds the note into the next step, a key transposes the
// pattern while it plays, the patterns come back with a session.
static bool sequencerTests(){
 auto require=[](bool ok,const juce::String& what){if(!ok)std::cerr<<"SEQUENCER: "<<what<<"\n";return ok;};
 {oxide::Pattern p;p.fromString("4|0,0,1,0,1;3,1,0,1,1;0,0,0,0,0;7,-1,0,0,2");oxide::Pattern q;q.fromString(p.toString());
  if(!require(q.toString()==p.toString()&&q.length==4&&q.steps[1].octave==1&&q.steps[1].slide&&q.steps[3].gate==oxide::tie,"pattern text form"))return false;
  juce::Random r(7);oxide::Pattern rnd;rnd.randomise(r);if(!require(!rnd.empty(),"random pattern"))return false;}
 // A processor playing pattern 1 set from a session, at 120 BPM: one step lasts 6000 samples at 48 kHz.
 auto make=[](const juce::String& pattern){std::unique_ptr<juce::AudioProcessor> q(createPluginFilter());
  juce::MemoryBlock data;q->getStateInformation(data);auto xml=juce::AudioProcessor::getXmlFromBinary(data.getData(),int(data.getSize()));
  xml->getChildByName("Patterns")->setAttribute("p1",pattern);juce::AudioProcessor::copyXmlToBinary(*xml,data);q->setStateInformation(data.getData(),int(data.getSize()));
  setValue(*q,"tempo",120);setValue(*q,"reverbMix",0);setValue(*q,"delayMix",0);setValue(*q,"pattern",0);
  q->setRateAndBufferSizeDetails(48000,500);q->prepareToPlay(48000,500);setValue(*q,"seqPlay",1);return q;};
 auto run=[](juce::AudioProcessor& q,int blocks,int keyAt=-1,int key=0){std::vector<float> out;juce::AudioBuffer<float> b(2,500);
  for(int k=0;k<blocks;++k){juce::MidiBuffer m;if(k==keyAt)m.addEvent(juce::MidiMessage::noteOn(1,key,.6f),0);b.clear();q.processBlock(b,m);out.insert(out.end(),b.getReadPointer(0),b.getReadPointer(0)+500);}return out;};
 auto level=[](const std::vector<float>& x,int from,int to){double e=0;for(int i=from;i<to;++i)e+=double(x[size_t(i)])*x[size_t(i)];return std::sqrt(e/double(to-from));};
 // Notes on steps 1 and 3, rests on 2 and 4: loud, silent, loud, silent.
 {auto q=make("4|0,0,0,0,1;0,0,0,0,0;0,0,0,0,1;0,0,0,0,0");const auto x=run(*q,96);
  const double a=level(x,500,2500),rest1=level(x,8000,11500),b=level(x,12500,14500),rest2=level(x,20000,23500);
  if(!require(a>.01&&b>.01&&rest1<a*.05&&rest2<b*.05,"notes and rests in time"))return false;}
 // SLIDE: the note is held through the next step (no gap); without it, the note stops half way through its step.
 {auto gapOf=[&](bool slide){auto q=make(juce::String("2|0,0,0,")+(slide?"1":"0")+",1;5,0,0,0,1");const auto x=run(*q,40);return level(x,4500,5900)/level(x,500,2500);};
  if(!require(gapOf(true)>.3&&gapOf(false)<.05,"slide holds the note into the next step"))return false;}
 // TRANSPOSE: a key while the pattern plays moves it (C3 = as written; C4 = an octave up).
 {auto q=make("1|0,0,0,1,1");const auto x=run(*q,480,240,60);
  auto crossings=[&](int from,int to){int n=0;for(int i=from+1;i<to;++i)if(x[size_t(i-1)]<0&&x[size_t(i)]>=0)++n;return n;};
  const int low=crossings(24000,72000),high=crossings(168000,216000); // 1 s each: C2 is 65.4 Hz
  if(!require(std::abs(low-65)<=3&&std::abs(high-131)<=4,"transpose: "+juce::String(low)+" / "+juce::String(high)))return false;}
 // The patterns are saved with the session.
 {auto q=make("3|0,0,1,0,1;7,0,0,1,1;12,0,0,0,1");juce::MemoryBlock data;q->getStateInformation(data);
  auto xml=juce::AudioProcessor::getXmlFromBinary(data.getData(),int(data.getSize()));
  if(!require(xml->getChildByName("Patterns")!=nullptr&&xml->getChildByName("Patterns")->getStringAttribute("p1").startsWith("3|0,0,1,0,1;7,0,0,1,1;12,0,0,0,1"),"patterns kept with the session"))return false;}
 std::cout<<"PASS sequencer: steps and rests in time, slide, transpose from the keys, patterns kept with the session\n";return true;
}
