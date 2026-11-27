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
 // The sixteen factory patterns are there from the start, none empty.
 {std::unique_ptr<juce::AudioProcessor> q(createPluginFilter());juce::MemoryBlock data;q->getStateInformation(data);auto xml=juce::AudioProcessor::getXmlFromBinary(data.getData(),int(data.getSize()));
  for(int i=1;i<=16;++i){oxide::Pattern pt;pt.fromString(xml->getChildByName("Patterns")->getStringAttribute("p"+juce::String(i)));if(!require(!pt.empty(),"factory pattern "+juce::String(i)+" empty"))return false;}}
 // The patterns are saved with the session.
 {auto q=make("3|0,0,1,0,1;7,0,0,1,1;12,0,0,0,1");juce::MemoryBlock data;q->getStateInformation(data);
  auto xml=juce::AudioProcessor::getXmlFromBinary(data.getData(),int(data.getSize()));
  if(!require(xml->getChildByName("Patterns")!=nullptr&&xml->getChildByName("Patterns")->getStringAttribute("p1").startsWith("3|0,0,1,0,1;7,0,0,1,1;12,0,0,0,1"),"patterns kept with the session"))return false;}
 // MIDI out: the sequencer's notes leave the plugin (a host records them), accents louder, a slide overlaps the
 // next note; the keys played in are not sent back; stopping sends the last note off.
 {auto q=make("4|0,0,1,0,1;7,0,0,1,1;12,0,0,0,1;0,0,0,0,0");std::vector<std::pair<long,juce::MidiMessage>> out;juce::AudioBuffer<float> b(2,500);
  for(int k=0;k<96;++k){juce::MidiBuffer m;if(k==5)m.addEvent(juce::MidiMessage::noteOn(1,60,.6f),0);b.clear();q->processBlock(b,m);for(const auto e:m)out.push_back({long(k)*500+e.samplePosition,e.getMessage()});}
  setValue(*q,"seqPlay",0);{juce::MidiBuffer m;b.clear();q->processBlock(b,m);for(const auto e:m)out.push_back({96L*500+e.samplePosition,e.getMessage()});}
  int ons=0,offs=0;bool accent=false,overlap=false,stray=false;int held=0,maxHeld=0;
  for(const auto& [t,m]:out){if(m.isNoteOn()){++ons;++held;accent=accent||m.getVelocity()==127;}else if(m.isNoteOff()){++offs;--held;}maxHeld=std::max(maxHeld,held);(void)t;}
  overlap=maxHeld==2;stray=ons==0;
  if(!require(!stray&&ons>=6&&ons==offs&&held==0&&accent&&overlap,"MIDI out: "+juce::String(ons)+" on / "+juce::String(offs)+" off, max held "+juce::String(maxHeld)))return false;
  {const auto first=std::find_if(out.begin(),out.end(),[](const auto& x){return x.second.isNoteOn();});if(!require(first!=out.end()&&first->first<5&&first->second.getNoteNumber()==36&&first->second.getVelocity()==127,"MIDI out: step 1 at once, C2 accented"))return false;}}
 // .MID: the pattern as a file: notes on sixteenths from C2, accents, a slide overlaps the next note, a tie carries on.
 {oxide::Pattern p;p.fromString("4|0,0,1,1,1;7,0,0,0,1;0,0,0,0,2;12,0,0,0,0");const auto f=p.toMidi(0,"t",120);
  juce::MidiMessageSequence s(*f.getTrack(0));s.updateMatchedPairs();std::vector<std::tuple<double,double,int,int>> notes;
  for(const auto* e:s)if(e->message.isNoteOn())notes.push_back({e->message.getTimeStamp(),e->noteOffObject?e->noteOffObject->message.getTimeStamp():-1.0,e->message.getNoteNumber(),e->message.getVelocity()});
  const bool ok=notes.size()==2&&std::get<0>(notes[0])==0&&std::get<2>(notes[0])==36&&std::get<3>(notes[0])==127&&std::get<1>(notes[0])>24 // slide: held into step 2
   &&std::get<0>(notes[1])==24&&std::get<2>(notes[1])==43&&std::abs(std::get<1>(notes[1])-60)<1e-6; // G, tied through step 3, off half way
  if(!require(ok,"pattern as a MIDI file: "+juce::String((int)notes.size())+" notes"))return false;}
 std::cout<<"PASS sequencer: steps and rests in time, slide, transpose from the keys, patterns kept with the session\n";return true;
}
