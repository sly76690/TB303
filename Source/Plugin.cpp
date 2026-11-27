#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <map>
#include <optional>

using namespace juce;
#include "FactoryBank.h"
#include "PresetStorage.h"
#include "Reverbs.h"
#include "CharacterDelay.h"
#include "ForceTouch.h"
#include "AcidVoice.h"
#include "Pattern.h"

// OXIDE: a monophonic acid synthesizer. One voice (AcidVoice.h), played from the keys: a note pressed while another is
// still held slides to it, a hard-struck note is accented. Then a pedal-style DRIVE, the echo and the reverb.
// The class keeps the name Orbit: the panel, preset and packaging code shared with the other SF Instruments synths use it.
class Orbit : public AudioProcessor, private AudioProcessorValueTreeState::Listener, private AudioProcessorListener {
public:
 AudioProcessorValueTreeState state;
 MidiKeyboardState keyboard;
 Orbit() : AudioProcessor(BusesProperties().withOutput("Stereo",AudioChannelSet::stereo(),true)), state(*this,nullptr,"OrbitState",parameters()) {
  for(auto* param:getParameters()) if(auto* identified=dynamic_cast<AudioProcessorParameterWithID*>(param)) state.addParameterListener(identified->paramID,this);
  state.state.setProperty("stateVersion",1,nullptr);
  for(auto& slot:ccTarget)slot.store(-1);addListener(this);
  for(auto& p:patterns)p.clear();
  for(int i=0;i<int(starterPatterns().size());++i)patterns[size_t(i)].fromString(starterPatterns()[size_t(i)]);
 }
 ~Orbit() override {removeListener(this);for(auto* param:getParameters()) if(auto* identified=dynamic_cast<AudioProcessorParameterWithID*>(param)) state.removeParameterListener(identified->paramID,this);}
 String userPresetName() const {return state.state.getProperty("userPresetName").toString();}
 // User presets live in the user's library, so they survive moving or rebuilding the project.
 static File presetRoot(){return File::getSpecialLocation(File::userApplicationDataDirectory).getChildFile("Application Support").getChildFile("SF Instruments").getChildFile("Oxide");}
 StringArray userPresetNames() const {return PresetStorage(presetRoot()).names();}
 // Sounds: complete presets shipped by the installer (shared library), plus any the user drops in their own Presets/Sounds.
 static File sharedRoot(){return File::getSpecialLocation(File::commonApplicationDataDirectory).getChildFile("Application Support").getChildFile("SF Instruments").getChildFile("Oxide");}
 String soundName() const {return state.state.getProperty("soundName").toString();}
 StringArray soundNames() const {auto names=PresetStorage(sharedRoot(),"Sounds").names();names.mergeArray(PresetStorage(presetRoot(),"Sounds").names());names.sort(true);return names;}
 Result loadSound(const String& name){auto xml=PresetStorage(presetRoot(),"Sounds").load(name);if(!xml)xml=PresetStorage(sharedRoot(),"Sounds").load(name);if(!xml)return Result::fail("Son absent ou invalide dans Presets/Sounds.");
  MemoryBlock data;copyXmlToBinary(*xml,data);loadingPreset.store(true);setStateInformation(data.getData(),int(data.getSize()));loadingPreset.store(false);state.state.removeProperty("userPresetName",nullptr);state.state.setProperty("soundName",name,nullptr);modified.store(false);return Result::ok();}
 bool userPresetExists(const String& name) const {return PresetStorage(presetRoot()).exists(name);}
 Result saveUserPreset(const String& name,bool replace=false){auto saved=state.copyState();saved.setProperty("program",currentProgram.load(),nullptr);saved.setProperty("modified",false,nullptr);saved.setProperty("userPresetName",name,nullptr);saved.removeProperty("soundName",nullptr);
  auto xml=saved.createXml();auto result=PresetStorage(presetRoot()).save(name,*xml,replace);if(result.wasOk()){state.state.setProperty("userPresetName",name,nullptr);state.state.removeProperty("soundName",nullptr);modified.store(false);}return result;}
 Result loadUserPreset(const String& name){auto xml=PresetStorage(presetRoot()).load(name);if(!xml)return Result::fail("Preset absent, invalide ou inaccessible dans Presets/User.");MemoryBlock data;copyXmlToBinary(*xml,data);loadingPreset.store(true);setStateInformation(data.getData(),int(data.getSize()));loadingPreset.store(false);
  state.state.removeProperty("soundName",nullptr);state.state.setProperty("userPresetName",name,nullptr);modified.store(false);return Result::ok();}
 bool isPatchModified() const {return modified.load();}
 // The note sounding (-1: none) and whether it is accented or sliding, for the panel.
 int soundingNote() const {return shownNote.load();}
 bool accentLit() const {return shownAccent.load();}
 bool slideLit() const {return shownSlide.load();}
 // ---- PATTERN SEQUENCER: sixteen patterns of up to sixteen steps, kept with the session (not with the sounds, so a
 // pattern plays with any sound). The panel reads and edits them under the same lock as the audio thread.
 int patternIndex() const {return jlimit(0,15,int(value("pattern")));}
 String patternText(int index){const SpinLock::ScopedLockType lock(patternLock);return patterns[size_t(jlimit(0,15,index))].toString();}
 void editPattern(const std::function<void(oxide::Pattern&)>& change){const SpinLock::ScopedLockType lock(patternLock);change(patterns[size_t(patternIndex())]);modified.store(true);}
 int playingStep() const {return shownStep.load();}
 int transposition() const {return transpose.load();}
 std::atomic<int> editStep{0};std::atomic<bool> writing{false}; // WRITE: the keys fill the selected step, then the next
 // The sixteen factory patterns (our own lines), written over the memories when the plugin starts.
 static const std::array<const char*,16>& starterPatterns(){static const std::array<const char*,16> p{{
  "16|0,0,1,0,1;0,0,0,0,1;12,0,0,1,1;0,0,0,0,1;3,0,0,0,1;0,0,1,0,1;0,0,0,0,0;7,0,0,1,1;5,0,0,0,1;0,0,1,0,1;0,0,0,0,1;10,-1,0,0,1;0,0,0,1,1;3,0,0,0,1;0,0,1,0,1;7,0,0,0,1",
  "16|0,0,1,0,1;0,0,0,1,1;0,1,0,0,1;0,0,0,0,0;0,0,1,0,1;3,0,0,1,1;5,0,0,0,1;0,0,0,0,2;0,0,1,0,1;0,0,0,0,1;10,0,0,1,1;12,0,1,0,1;0,0,0,0,0;7,0,0,1,1;5,0,0,0,1;3,0,1,0,1",
  "16|0,0,1,1,1;12,0,0,0,1;0,0,0,0,1;0,0,1,1,1;7,0,0,0,1;0,0,0,0,0;3,1,1,0,1;0,0,0,1,1;0,0,1,0,1;0,0,0,0,1;5,0,0,1,1;6,0,0,0,1;7,0,1,0,1;0,0,0,0,0;10,-1,0,1,1;0,0,1,0,1",
  "8|0,0,1,0,1;0,0,0,0,1;1,0,0,1,1;0,0,0,0,1;0,0,1,0,1;7,-1,0,0,1;0,0,0,1,1;12,0,1,0,1;0,0,0,0,1;0,0,0,0,1;0,0,0,0,1;0,0,0,0,1;0,0,0,0,1;0,0,0,0,1;0,0,0,0,1;0,0,0,0,1",
  "16|0,0,0,0,1;0,1,0,0,1;0,0,0,0,1;0,0,1,0,1;0,0,0,0,1;0,1,0,1,1;3,0,0,0,1;0,0,0,0,1;0,0,0,0,1;0,1,0,0,1;0,0,1,0,1;7,0,0,0,1;0,0,0,0,1;5,0,0,1,1;3,0,0,0,1;0,0,0,0,1",
  "16|0,0,1,0,1;1,0,0,0,1;0,0,0,0,1;0,0,0,0,0;0,0,0,0,1;1,0,0,1,1;0,0,0,0,1;7,0,0,0,1;0,0,1,0,1;0,0,0,0,0;0,0,0,0,1;1,1,0,0,1;0,0,0,0,1;0,0,0,0,1;8,-1,0,1,1;0,0,0,0,1",
  "8|0,0,1,1,1;7,0,0,1,1;0,1,0,0,1;0,0,0,0,0;3,0,1,1,1;5,0,0,1,1;7,0,0,0,1;0,0,0,0,0;0,0,0,0,0;0,0,0,0,0;0,0,0,0,0;0,0,0,0,0;0,0,0,0,0;0,0,0,0,0;0,0,0,0,0;0,0,0,0,0",
  "16|0,0,0,0,0;0,0,1,0,1;0,0,0,0,0;0,1,0,0,1;0,0,0,0,0;0,0,1,0,1;0,0,0,0,0;3,0,0,0,1;0,0,0,0,0;0,0,1,0,1;0,0,0,0,0;0,1,0,0,1;0,0,0,0,0;7,0,1,0,1;0,0,0,0,0;5,0,0,0,1",
  "12|0,0,0,0,1;0,1,0,0,1;7,0,0,0,1;0,0,1,0,1;0,1,0,0,1;3,0,0,0,1;0,0,0,0,1;0,1,0,0,1;7,0,0,0,1;0,0,1,0,1;10,-1,0,1,1;0,0,0,0,1;0,0,0,0,0;0,0,0,0,0;0,0,0,0,0;0,0,0,0,0",
  "16|0,0,0,0,1;3,0,0,0,1;5,0,0,0,1;7,0,0,0,1;10,0,0,0,1;12,0,0,0,1;7,0,0,0,1;5,0,0,0,1;3,0,0,0,1;0,0,1,0,1;0,1,0,0,1;0,0,0,0,0;0,0,0,1,1;3,0,0,1,1;5,0,0,1,1;7,0,1,0,1",
  "16|0,0,1,0,1;0,0,0,0,0;0,0,0,0,0;0,1,0,0,1;0,0,0,0,0;0,0,0,0,0;8,-1,0,1,1;0,0,0,0,0;0,0,1,0,1;0,0,0,0,0;0,0,0,0,0;0,1,0,0,1;0,0,0,0,0;5,0,0,1,1;3,0,0,0,1;0,0,0,0,0",
  "16|0,0,1,0,1;0,0,0,0,1;0,0,0,0,1;0,1,0,1,1;0,0,0,0,1;0,0,0,0,1;0,0,1,0,1;0,0,0,0,1;0,0,0,0,1;0,0,0,0,1;0,1,0,0,1;0,0,0,0,1;0,0,1,0,1;0,0,0,0,1;0,1,0,1,1;0,0,0,0,1",
  "16|0,0,0,0,1;3,0,0,0,1;7,0,0,0,1;10,0,0,0,1;12,0,0,0,1;10,0,0,1,1;7,0,0,0,1;3,0,0,0,1;0,0,1,0,1;3,1,0,0,1;7,0,0,0,1;10,0,0,1,1;12,0,0,0,1;10,0,0,0,1;7,0,0,1,1;3,0,0,0,1",
  "16|0,0,1,0,1;0,0,0,0,1;3,0,0,1,1;0,0,0,0,1;0,0,0,0,0;7,0,0,0,1;0,1,1,1,1;0,0,0,0,1;0,0,0,0,1;0,0,0,0,1;3,0,0,1,1;5,0,0,1,1;7,0,1,0,1;0,0,0,0,0;5,0,0,0,1;3,0,0,0,1",
  "7|0,0,1,0,1;0,1,0,0,1;7,0,0,1,1;0,0,0,0,1;3,0,0,0,1;0,1,1,0,1;0,0,0,0,0;0,0,0,0,0;0,0,0,0,0;0,0,0,0,0;0,0,0,0,0;0,0,0,0,0;0,0,0,0,0;0,0,0,0,0;0,0,0,0,0;0,0,0,0,0",
  "16|0,0,1,1,1;0,0,0,0,2;0,0,0,0,2;0,0,0,1,2;7,0,0,1,1;0,0,0,0,2;0,0,0,0,2;0,0,0,1,2;3,0,0,1,1;0,0,0,0,2;0,0,0,0,2;0,0,0,1,2;5,0,0,1,1;0,0,0,0,2;7,0,1,0,1;0,0,0,0,0"}};return p;}

 // Host pages of eight, in the order of the front panel.
 static constexpr const char* hostPages[]={
  "wave","tune","cutoff","resonance","envMod","decay","accent","slideTime",            // VCO, VCF, ACCENT, SLIDE
  "ampDecay","driveMode","drive","driveTone","bendRange","tempo","accentVelocity","seqPlay",  // VCA, DRIVE, OUTPUT, PLAY
  "gain","seqSync","pattern","swing","delayOn",                                                // SEQUENCER
  "delayTime","delaySync","delayFeedback","delayMod","delayMix","reverbOn","reverbType","reverbSize",
  "reverbDamp","reverbColor","reverbMix"};
 // Short names for the host and controller displays.
 static String label(const String& id,const String& longName){
  static const std::map<String,String> names{
   {"wave","Waveform"},{"tune","Tune"},{"cutoff","Cutoff"},{"resonance","Resonance"},{"envMod","Env Mod"},{"decay","Decay"},{"accent","Accent"},{"slideTime","Slide Time"},
   {"ampDecay","VCA Decay"},{"driveMode","Drive Mode"},{"drive","Drive"},{"driveTone","Drive Tone"},{"gain","Volume"},{"bendRange","Bend Range"},{"tempo","Tempo"},{"accentVelocity","Accent Vel"},{"seqPlay","Seq Play"},{"seqSync","Seq Sync"},{"pattern","Pattern"},{"swing","Swing"},
   {"delayOn","Dly On"},{"delayTime","Dly Time"},{"delaySync","Dly Sync"},{"delayFeedback","Dly Fdbk"},{"delayMod","Dly Mod"},{"delayMix","Dly Level"},
   {"reverbOn","Rev On"},{"reverbType","Rev Type"},{"reverbSize","Rev Size"},{"reverbDamp","Rev Damp"},{"reverbColor","Rev Tone"},{"reverbMix","Rev Level"}};
  const auto found=names.find(id);return found!=names.end()?found->second:longName;
 }
 static AudioProcessorValueTreeState::ParameterLayout parameters() {
  std::vector<std::unique_ptr<RangedAudioParameter>> made;
  auto keep=[&made](auto parameter){made.push_back(std::move(parameter));};
  // Values read on the 0-10 scale printed around the knob (-5..+5 when centred on zero); a real unit is added in brackets.
  auto add=[&](const char* id,const char* name,float lo,float hi,float def,float skew=1.f,bool octaves=false){
   const NormalisableRange<float> range=octaves?NormalisableRange<float>(lo,hi,[](float s,float e,float x){return s*std::pow(e/s,x);},[](float s,float e,float v){return std::log(v/s)/std::log(e/s);},nullptr)
                                              :NormalisableRange<float>(lo,hi,0.f,skew);
   const bool centred=lo<0.f,whole=hi>=100.f;
   const String unit=String(name).fromFirstOccurrenceOf("(",false,false).upToFirstOccurrenceOf(")",false,false);
   auto text=[range,centred,whole,unit](float v,int){
    const float mark=range.convertTo0to1(range.snapToLegalValue(v))*10.f-(centred?5.f:0.f);
    String shown=(centred&&mark>.04f?"+":"")+String(mark,1);
    if(unit.isNotEmpty())shown+=" ("+(whole?String(roundToInt(v)):String(v,std::abs(v)<1.f?2:1))+" "+unit+")";
    return shown;};
   auto parse=[range,centred](const String& typed){return range.convertFrom0to1(jlimit(0.f,1.f,(typed.getFloatValue()+(centred?5.f:0.f))/10.f));};
   keep(std::make_unique<AudioParameterFloat>(ParameterID{id,1},label(id,name),range,def,AudioParameterFloatAttributes().withStringFromValueFunction(text).withValueFromStringFunction(parse)));};
  auto choice=[&](const char* id,const char* name,StringArray values,int initial=0){keep(std::make_unique<AudioParameterChoice>(ParameterID{id,1},label(id,name),values,initial));};
  auto toggle=[&](const char* id,const char* name,bool initial){keep(std::make_unique<AudioParameterBool>(ParameterID{id,1},label(id,name),initial));};
  choice("wave","Waveform",{"SAW","SQUARE"});
  keep(std::make_unique<AudioParameterFloat>(ParameterID{"tune",1},label("tune","Tune (semitones)"),NormalisableRange<float>(-12.f,12.f,.01f),0.f,
   AudioParameterFloatAttributes().withStringFromValueFunction([](float v,int){return (v>=0?"+":"")+String(v,2)+" st";})));
  add("cutoff","Cutoff (Hz)",40,6000,400,1.f,true);add("resonance","Resonance",0,1,.55f);add("envMod","Env mod",0,1,.5f);
  add("decay","Decay (s)",.2f,2.5f,.6f,.5f);add("accent","Accent",0,1,.55f);add("slideTime","Slide time (s)",.02f,.4f,.06f,.5f);
  add("ampDecay","VCA decay (s)",.2f,8,3,.45f);choice("driveMode","Drive mode",{"OVERDRIVE","DISTORTION"});add("drive","Drive",0,1,0);add("driveTone","Drive tone",0,1,.55f);add("gain","Output",0,1,.7f);
  add("bendRange","Pitch bend (semitones)",0,12,2);add("tempo","Tempo (BPM)",40,300,125);
  add("accentVelocity","Accent from velocity",.3f,1,.78f); // a note struck at least this hard is accented
  // SEQUENCER: PLAY, SYNC (to the host's transport and beats), the pattern played, SWING (every second step later).
  toggle("seqPlay","Sequencer play",false);toggle("seqSync","Sequencer host sync",false);
  {StringArray numbers;for(int i=1;i<=16;++i)numbers.add(String(i));choice("pattern","Pattern",numbers,0);}
  add("swing","Swing",0,.6f,0);
  toggle("delayOn","Delay on",true);add("delayTime","Delay time (s)",.03f,1.5f,.3f);
  choice("delaySync","Delay sync",{"FREE","1/2","1/4 D","1/4","1/4 T","1/8 D","1/8","1/8 T","1/16 D","1/16","1/16 T"},5);
  add("delayFeedback","Delay feedback",0,1.1f,.35f);add("delayMod","Delay modulation",0,1,.15f);add("delayMix","Delay mix",0,.65f,0);
  toggle("reverbOn","Reverb on",true);choice("reverbType","Reverb type",{"VINT","PLATE","SPACE"},1);
  add("reverbSize","Reverb size",0,1,.45f);add("reverbDamp","Reverb damping",0,1,.5f);add("reverbColor","Reverb color",0,1,.4f);add("reverbMix","Reverb mix",0,.65f,0);
  AudioProcessorValueTreeState::ParameterLayout l;
  for(const char* id:hostPages)for(auto& parameter:made)if(parameter&&parameter->paramID==id)l.add(std::move(parameter));
  for(auto& parameter:made)if(parameter)l.add(std::move(parameter)); // anything not placed on a page goes last, never lost
  return l;
 }
 const String getName() const override {return "Oxide";}
 bool acceptsMidi() const override {return true;} bool producesMidi() const override {return true;} // the sequencer's notes go out (a host can record them)
 double getTailLengthSeconds() const override {return 12;}
 int getNumPrograms() override {return jmax(1,FactoryBank::order().size());} int getCurrentProgram() override {return currentProgram.load();}
 // BANK / PATCH: the embedded factory sounds. A patch change keeps the performance settings as they are.
 void setCurrentProgram(int index) override {
  if(!isPositiveAndBelow(index,FactoryBank::order().size())) return;
  if(auto xml=FactoryBank::load(FactoryBank::order()[index])){
   static const StringArray performance{"gain","bendRange","tempo","accentVelocity","seqPlay","seqSync","pattern","swing"};
   std::array<float,8> kept{};for(int i=0;i<performance.size();++i)kept[size_t(i)]=state.getParameter(performance[i])->getValue();
   MemoryBlock data;copyXmlToBinary(*xml,data);loadingPreset.store(true);setStateInformation(data.getData(),int(data.getSize()));loadingPreset.store(false);
   for(int i=0;i<performance.size();++i)state.getParameter(performance[i])->setValueNotifyingHost(kept[size_t(i)]);
   currentProgram.store(index);state.state.removeProperty("userPresetName",nullptr);state.state.removeProperty("soundName",nullptr);modified.store(false);}
 }
 const String getProgramName(int index) override {return isPositiveAndBelow(index,FactoryBank::order().size())?FactoryBank::order()[index].replace(" - "," / "):String("INIT");}
 void changeProgramName(int,const String&) override {}
 void prepareToPlay(double sr,int block) override {
  sampleRate=sr;blockCapacity=jmax(block,8192);chunkMidi.ensureSize(8192);seqOut.ensureSize(8192);outNote=-1;
  voice.prepare(sr);held.clearQuick();held.ensureStorageAllocated(128);wheel=0;
  voicePass.reset();voicePass.initProcessing(size_t(blockCapacity));voiceBuffer.setSize(1,blockCapacity);
  drivePass.reset();drivePass.initProcessing(size_t(blockCapacity));for(int c=0;c<2;++c)driveLow[c]=distLow[c]=distMid[c]=distSlew[c]=0;
  echo.prepare(sr);reverb.prepare(sr);gain.reset(sr,.02);gain.setCurrentAndTargetValue(value("gain"));
 }
 void releaseResources() override {voice.reset();keyboard.reset();echo.reset();reverb.reset();held.clearQuick();}
 bool isBusesLayoutSupported(const BusesLayout& b) const override {return b.getMainOutputChannelSet()==AudioChannelSet::stereo();}
 // A block larger than prepared is played in pieces of the prepared size: nothing is ever allocated while playing.
 // MIDI out: only the notes the pattern sequencer plays (accent = velocity 127, slide = the next note starts before
 // this one stops), so a host can record the pattern as notes. The keys played in are not sent back.
 void processBlock(AudioBuffer<float>& buffer,MidiBuffer& midi) override {
  seqOut.clear();outBase=0;
  const int total=buffer.getNumSamples();
  if(total<=blockCapacity)processPiece(buffer,midi);
  else for(int done=0;done<total;done+=blockCapacity){const int count=jmin(blockCapacity,total-done);
   AudioBuffer<float> piece(buffer.getArrayOfWritePointers(),buffer.getNumChannels(),done,count);
   chunkMidi.clear();chunkMidi.addEvents(midi,done,count,-done);outBase=done;processPiece(piece,chunkMidi);}
  midi.clear();midi.addEvents(seqOut,0,-1,0);
 }
 void processPiece(AudioBuffer<float>& buffer,MidiBuffer& midi){
  ScopedNoDenormals noDenormals;buffer.clear();keyboard.processNextMidiBuffer(midi,0,buffer.getNumSamples(),true);
  if(const int bend=screenPitch.exchange(-1);bend>=0)midi.addEvent(MidiMessage::pitchWheel(1,bend),0);
  // MIDI learn and learned controls. The mod wheel, sustain, bank select and channel messages keep their own jobs.
  for(const auto event:midi){const auto message=event.getMessage();if(!message.isController())continue;
   const int cc=message.getControllerNumber();if(cc==0||cc==1||cc==32||cc==64||cc>=120)continue;
   if(learning.load()&&lastTouched.load()>=0){const int target=lastTouched.load();forget(target);ccTarget[size_t(cc)].store(target);learning.store(false);}
   const int target=ccTarget[size_t(cc)].load();if(isPositiveAndBelow(target,getParameters().size()))getParameters()[target]->setValueNotifyingHost(float(message.getControllerValue())/127.f);
  }
  oxide::Settings s;s.wave=int(value("wave"));s.tune=value("tune");s.cutoff=value("cutoff");s.resonance=value("resonance");s.envMod=value("envMod");
  s.decay=value("decay");s.accent=value("accent");s.slideTime=value("slideTime");s.ampDecay=value("ampDecay");
  // The voice, sample-accurately between MIDI events, at twice the rate.
  const int n=buffer.getNumSamples();
  dsp::AudioBlock<float> voiceBlock(voiceBuffer.getArrayOfWritePointers(),1,size_t(n));voiceBlock.clear();
  auto up=voicePass.processSamplesUp(voiceBlock);float* twice=up.getChannelPointer(0);
  const int stepsFound=runSequencer(n);
  int at=0,next=0;auto renderTo=[&](int when){if(when>at){s.bend=bendNow();voice.render(twice+2*at,2*(when-at),s);at=when;}};
  for(const auto event:midi){const int when=jlimit(0,n,event.samplePosition);
   while(next<stepsFound&&seqEvents[size_t(next)].offset<=when){renderTo(seqEvents[size_t(next)].offset);play(seqEvents[size_t(next)]);++next;}
   renderTo(when);handle(event.getMessage());}
  while(next<stepsFound){renderTo(seqEvents[size_t(next)].offset);play(seqEvents[size_t(next)]);++next;}
  renderTo(n);
  voicePass.processSamplesDown(voiceBlock);
  for(int c=0;c<buffer.getNumChannels();++c)buffer.copyFrom(c,0,voiceBuffer,0,0,n);
  shownNote.store(voice.active()?voice.note():-1);shownSlide.store(voice.active()&&voice.sliding());
  // DRIVE, at four times the rate, then its TONE (a low-pass from dark to open), at about the same loudness at any DRIVE.
  // OVERDRIVE: a soft, slightly lopsided clipper (a diode pair that does not quite match): warm, it keeps the bass.
  // DISTORTION: the hard-edged kind of pedal: a high-gain stage that pushes the mids and leaves the deep bass out, its
  // highs rounded off more as the gain rises (as a slow op-amp does), then hard clipping to ground. Our own values.
  if(const float drive=value("drive");drive>.001f&&buffer.getNumChannels()>=2){
   dsp::AudioBlock<float> block(buffer.getArrayOfWritePointers(),2,size_t(n));auto big=drivePass.processSamplesUp(block);
   const float rate=float(sampleRate)*float(big.getNumSamples())/float(jmax(1,n)),w=MathConstants<float>::twoPi/rate;
   const float toneHz=600.f*std::pow(20.f,value("driveTone")),lp=1.f-std::exp(-w*toneHz);
   if(int(value("driveMode"))==0){
    const float pre=1.f+drive*drive*30.f,post=std::pow(pre,-.55f);
    for(size_t c=0;c<2;++c){auto* x=big.getChannelPointer(c);for(size_t i=0;i<big.getNumSamples();++i){const float v=x[i]*pre;
      const float shaped=v>0?std::tanh(v):std::tanh(v*.8f)/.8f*.9f;
      driveLow[c]+=(shaped-driveLow[c])*lp;x[i]=driveLow[c]*post;}}}
   else{
    const float gain=1.f+drive*drive*1500.f,post=.42f*std::pow(gain,-.12f);
    const float bassCut=1.f-std::exp(-w*90.f),midShelf=1.f-std::exp(-w*720.f),slew=1.f-std::exp(-w*jlimit(2500.f,20000.f,3.0e6f/gain));
    for(size_t c=0;c<2;++c){auto* x=big.getChannelPointer(c);for(size_t i=0;i<big.getNumSamples();++i){
      distLow[c]+=(x[i]-distLow[c])*bassCut;const float tight=x[i]-distLow[c];          // the deep bass stays out of the clipper
      distMid[c]+=(tight-distMid[c])*midShelf;const float pushed=tight+(gain-1.f)*(tight-distMid[c])+std::sqrt(gain)*distMid[c]; // gain rises above the low mids
      distSlew[c]+=(pushed-distSlew[c])*slew;const float v=distSlew[c]*.8f;
      const float clipped=v/std::pow(1.f+std::pow(std::abs(v),8.f),.125f);              // hard knee, like silicon diodes to ground
      driveLow[c]+=(clipped-driveLow[c])*lp;x[i]=driveLow[c]*post;}}}
   drivePass.processSamplesDown(block);}
  // Delay time: the TIME knob when free, otherwise a note value at the host tempo (the TEMPO knob without a host).
  float delaySeconds=value("delayTime");
  if(const int syncStep=int(value("delaySync"));syncStep>0){double tempo=value("tempo");
   if(auto* playHead=getPlayHead())if(auto position=playHead->getPosition())if(auto hostBpm=position->getBpm())tempo=*hostBpm;
   static const double beats[]={0,2.,1.5,1.,2./3.,.75,.5,1./3.,.375,.25,1./6.};double seconds=beats[jlimit(1,10,syncStep)]*60./jmax(20.,tempo);
   while(seconds>4.)seconds*=.5;delaySeconds=float(seconds);}
  echo.process(buffer,delaySeconds,value("delayFeedback"),value("delayOn")>.5f?value("delayMix"):0.f,value("delayMod"));
  reverb.process(buffer,int(value("reverbType")),value("reverbSize"),value("reverbDamp"),value("reverbColor"),value("reverbOn")>.5f?value("reverbMix"):0.f);
  gain.setTargetValue(value("gain"));
  const auto limit=[](float x){constexpr float knee=.7f;const float a=std::abs(x);if(a<=knee)return x;return std::copysign(knee+(1.f-knee)*std::tanh((a-knee)/(1.f-knee)),x);};
  for(int i=0;i<n;++i){const float g=gain.getNextValue();for(int c=0;c<buffer.getNumChannels();++c)buffer.setSample(c,i,limit(buffer.getSample(c,i)*g));}
 }
 bool hasEditor() const override {return true;} AudioProcessorEditor* createEditor() override;
 void getStateInformation(MemoryBlock& dest) override {auto saved=state.copyState();saved.setProperty("program",currentProgram.load(),nullptr);saved.setProperty("modified",modified.load(),nullptr);auto xml=saved.createXml();
  {auto* list=xml->createNewChildElement("Patterns");const SpinLock::ScopedLockType lock(patternLock);for(int i=0;i<16;++i)list->setAttribute("p"+String(i+1),patterns[size_t(i)].toString());}
  auto* map=xml->createNewChildElement("MidiMap");for(int cc=0;cc<128;++cc)if(const int index=ccTarget[size_t(cc)].load();index>=0){auto* entry=map->createNewChildElement("Map");entry->setAttribute("cc",cc);entry->setAttribute("id",parameterId(index));}
  copyXmlToBinary(*xml,dest);}
 // ---- MIDI learn: one CC per control. Learning: touch a control on the panel, then move a knob on the controller.
 void startLearning(){lastTouched.store(-1);learning.store(true);}
 void stopLearning(){learning.store(false);}
 bool isLearning() const {return learning.load();}
 int lastTouchedParameter() const {return lastTouched.load();}
 int ccFor(int index) const {for(int cc=0;cc<128;++cc)if(ccTarget[size_t(cc)].load()==index)return cc;return -1;}
 void forget(int index){for(auto& slot:ccTarget)if(slot.load()==index)slot.store(-1);}
 void forgetAll(){for(auto& slot:ccTarget)slot.store(-1);}
 String parameterId(int index) const {if(auto* p=dynamic_cast<RangedAudioParameter*>(getParameters()[index]))return p->paramID;return {};}
 int parameterIndex(const String& id) const {for(int i=0;i<getParameters().size();++i)if(parameterId(i)==id)return i;return -1;}
 void setStateInformation(const void* data,int size) override {auto xml=getXmlFromBinary(data,size);if(!xml||!xml->hasTagName(state.state.getType()))return;
  // MIDI learn assignments belong to this instance, not to a sound: a session brings them back, a preset leaves them alone.
  if(auto* map=xml->getChildByName("MidiMap")){for(auto& slot:ccTarget)slot.store(-1);
   for(auto* entry:map->getChildIterator()){const int cc=entry->getIntAttribute("cc",-1);const int index=parameterIndex(entry->getStringAttribute("id"));if(isPositiveAndBelow(cc,128)&&index>=0)ccTarget[size_t(cc)].store(index);}
   xml->removeChildElement(map,true);}
  // The patterns come back with a session; a sound or preset (which never holds them) leaves them as they are.
  if(auto* list=xml->getChildByName("Patterns")){{const SpinLock::ScopedLockType lock(patternLock);for(int i=0;i<16;++i)if(list->hasAttribute("p"+String(i+1)))patterns[size_t(i)].fromString(list->getStringAttribute("p"+String(i+1)));}
   xml->removeChildElement(list,true);}
  auto restored=ValueTree::fromXml(*xml);restored.setProperty("stateVersion",1,nullptr);
  currentProgram.store(jlimit(0,getNumPrograms()-1,int(restored.getProperty("program",0))));
  for(auto* base:getParameters())if(auto* parameter=dynamic_cast<RangedAudioParameter*>(base)){ // a setting the preset does not list goes back to its default
   bool found=false;for(auto child:restored)if(child.getProperty("id").toString()==parameter->paramID){found=true;break;}
   if(!found){ValueTree child("PARAM");child.setProperty("id",parameter->paramID,nullptr);child.setProperty("value",parameter->convertFrom0to1(parameter->getDefaultValue()),nullptr);restored.addChild(child,-1,nullptr);}}
  state.replaceState(restored);modified.store(bool(restored.getProperty("modified",false)));}
 public: std::atomic<int> screenPitch{-1};
 public: std::atomic<int> screenPressure{-1}; // the panel keyboard's trackpad force (not used by this instrument's sound)
private:
 float value(const char* id) const {return state.getRawParameterValue(id)->load();}
 float bendNow() const {return wheel*value("bendRange");}
 // Monophonic play: the newest key sounds. A key pressed while another is held slides to it; letting go of the newest
 // key slides back to the one still held. A key struck at least ACCENT VEL hard is accented.
 void handle(const MidiMessage& m){
  if(m.isNoteOn()&&writing.load()&&!seqRunning)writeStep(m);
  if(seqRunning&&(m.isNoteOn()||m.isNoteOff())){ // while the pattern plays, a key transposes it (C3 plays it as written)
   if(m.isNoteOn())transpose.store(jlimit(-24,24,m.getNoteNumber()-48));return;}
  if(m.isNoteOn()){const int note=m.getNoteNumber();const bool legato=!held.isEmpty();held.removeFirstMatchingValue(note);held.add(note);
   const bool accented=m.getFloatVelocity()>=value("accentVelocity");voice.noteOn(note,accented,legato);shownAccent.store(accented);}
  else if(m.isNoteOff()){const int note=m.getNoteNumber();const bool wasNewest=!held.isEmpty()&&held.getLast()==note;held.removeFirstMatchingValue(note);
   if(wasNewest){if(held.isEmpty())voice.noteOff();else voice.noteOn(held.getLast(),false,true);}}
  else if(m.isAllNotesOff()||m.isAllSoundOff()){held.clearQuick();voice.noteOff();}
  else if(m.isPitchWheel())wheel=float(m.getPitchWheelValue()-8192)/8192.f;
 }
 // WRITE: the key goes into the selected step (its octave from where it is played around C2, ACCENT from how hard),
 // a key played while the last one is still held slides into it; then the next step is selected.
 void writeStep(const MidiMessage& m){
  const int rel=m.getNoteNumber()-36,octave=rel<0?-1:rel>12?1:0,note=jlimit(0,12,rel-12*octave);
  const bool accented=m.getFloatVelocity()>=value("accentVelocity"),legato=!held.isEmpty();
  const SpinLock::ScopedLockType lock(patternLock);auto& p=patterns[size_t(patternIndex())];const int i=jlimit(0,p.length-1,editStep.load());
  if(legato){auto& before=p.steps[size_t((i+p.length-1)%p.length)];if(before.gate!=oxide::rest)before.slide=true;}
  auto& step=p.steps[size_t(i)];step.note=note;step.octave=octave;step.accent=accented;step.slide=false;step.gate=oxide::play;
  editStep.store((i+1)%p.length);modified.store(true);
 }
 // The sequencer clock: the step boundaries that fall in this block (sixteenth notes, SWING delaying every second one),
 // in time with the host's beats when SYNC is on and the host plays. Returns how many events it wrote in seqEvents.
 struct SeqEvent{int offset;bool on;int note;bool accent,slide;};
 int runSequencer(int n){
  int count=0;auto emit=[&](int offset,bool on,int note,bool accent,bool slide){if(count<int(seqEvents.size()))seqEvents[size_t(count++)]={offset,on,note,accent,slide};};
  const bool playing=value("seqPlay")>.5f;
  if(!playing){if(seqRunning){if(seqNoteOn)emit(0,false,0,false,false);seqNoteOn=false;seqRunning=false;offIn=-1;}shownStep.store(-1);return count;}
  oxide::Pattern p;{const SpinLock::ScopedLockType lock(patternLock);p=patterns[size_t(patternIndex())];}
  double bpm=value("tempo");std::optional<double> ppq;bool hostRolling=false;
  if(auto* playHead=getPlayHead())if(auto position=playHead->getPosition()){if(auto hostBpm=position->getBpm())bpm=*hostBpm;hostRolling=position->getIsPlaying();if(hostRolling)if(auto beats=position->getPpqPosition())ppq=*beats;}
  const double sps=60./jmax(20.,bpm)/4.*sampleRate,swing=value("swing")*.5;
  const bool synced=value("seqSync")>.5f&&ppq.has_value();
  if(!seqRunning){seqRunning=true;stepCount=0;untilStep=0;offIn=-1;seqNoteOn=false;prevSlide=false;lastAbsolute=-1;}
  if(synced){ // the next step from the host's position: absolute step numbers, so no step is played twice
   const double pos=*ppq*4.;const long long k=(long long)std::floor(pos+1e-9);long long nextStep=k;double at=double(k)+((k&1)?swing:0.);
   if(at<pos-1e-9||k<=lastAbsolute){nextStep=k+1;at=double(nextStep)+((nextStep&1)?swing:0.);}
   untilStep=(at-pos)*sps;stepCount=nextStep;}
  for(int guard=0;guard<64;++guard){
   const double nextOn=untilStep,nextOff=offIn>=0?offIn:1e300;
   if(nextOff<=nextOn&&nextOff<double(n)){if(seqNoteOn)emit(int(nextOff),false,0,false,false);seqNoteOn=false;offIn=-1;continue;}
   if(nextOn>=double(n))break;
   const int offset=int(nextOn),i=int(((stepCount%p.length)+p.length)%p.length);const auto& st=p.steps[size_t(i)];const auto& after=p.steps[size_t((i+1)%p.length)];
   if(st.gate==oxide::play){emit(offset,true,p.midiNote(i,36,transpose.load()),st.accent,prevSlide&&seqNoteOn);seqNoteOn=true;}
   else if(st.gate==oxide::rest){if(seqNoteOn)emit(offset,false,0,false,false);seqNoteOn=false;}
   const bool sounding=seqNoteOn&&st.gate!=oxide::rest,hold=sounding&&(st.slide||after.gate==oxide::tie);
   offIn=sounding&&!hold?nextOn+sps*.5:-1;prevSlide=sounding&&st.slide;
   shownStep.store(i);lastAbsolute=stepCount;
   const double length=synced?sps*(1.+(((stepCount+1)&1)?swing:-swing)):sps*(1.+((stepCount&1)?-swing:swing));
   untilStep=nextOn+length;++stepCount;}
  untilStep-=n;if(offIn>=0)offIn-=n;
  return count;
 }
 void play(const SeqEvent& e){if(e.on){voice.noteOn(e.note,e.accent,e.slide);shownAccent.store(e.accent);}else voice.noteOff();send(e);}
 void send(const SeqEvent& e){const int at=outBase+e.offset;
  if(!e.on){if(outNote>=0)seqOut.addEvent(MidiMessage::noteOff(1,outNote),at);outNote=-1;return;}
  const int note=jlimit(0,127,e.note);if(e.slide&&outNote==note)return; // slide into the same note: it just goes on
  if(outNote>=0&&!e.slide)seqOut.addEvent(MidiMessage::noteOff(1,outNote),at);
  seqOut.addEvent(MidiMessage::noteOn(1,note,uint8(e.accent?127:96)),at);
  if(outNote>=0&&e.slide)seqOut.addEvent(MidiMessage::noteOff(1,outNote),at); // legato: the old note stops just after the new one starts
  outNote=note;}
 void parameterChanged(const String&,float) override {modified.store(true);}
 void audioProcessorParameterChanged(AudioProcessor*,int,float) override {}
 void audioProcessorChanged(AudioProcessor*,const AudioProcessorListener::ChangeDetails&) override {}
 void audioProcessorParameterChangeGestureBegin(AudioProcessor*,int index) override {lastTouched.store(index);} // the control just touched on the panel
 std::array<std::atomic<int>,128> ccTarget;std::atomic<bool> learning{false};std::atomic<int> lastTouched{-1};
 std::atomic<bool> modified{false},loadingPreset{false},shownAccent{false},shownSlide{false};std::atomic<int> currentProgram{0},shownNote{-1};
 double sampleRate=44100;int blockCapacity=8192;MidiBuffer chunkMidi;
 oxide::Voice voice;Array<int> held;float wheel=0;AudioBuffer<float> voiceBuffer;
 MidiBuffer seqOut;int outBase=0,outNote=-1; // sequencer notes sent out (see processBlock)
 std::array<oxide::Pattern,16> patterns;SpinLock patternLock;std::array<SeqEvent,96> seqEvents{};std::atomic<int> shownStep{-1},transpose{0};
 bool seqRunning=false,seqNoteOn=false,prevSlide=false;long long stepCount=0,lastAbsolute=-1;double untilStep=0,offIn=-1;
 dsp::Oversampling<float> voicePass{1,1,dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,true,false};
 dsp::Oversampling<float> drivePass{2,1,dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,true,false};float driveLow[2]{},distLow[2]{},distMid[2]{},distSlew[2]{};
 CharacterDelay echo;OrbitReverb reverb;SmoothedValue<float> gain;
};
#include "Editor.h"
AudioProcessorEditor* Orbit::createEditor(){return new Editor(*this);}
AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new Orbit();}
