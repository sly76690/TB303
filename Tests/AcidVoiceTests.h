#pragma once
// OXIDE voice: pitch, slide, accent, envelope modulation, the square, resonance at the top, release.
static bool acidVoiceTests(){
 auto require=[](bool ok,const char* what){if(!ok)std::cerr<<"ACID VOICE: "<<what<<"\n";return ok;};
 constexpr double rate=48000;constexpr int fs=96000; // the voice runs at twice the rate
 auto settings=[]{oxide::Settings s;s.cutoff=6000;s.resonance=0;s.envMod=0;s.accent=0;return s;};
 auto crossings=[](const std::vector<float>& x,int from,int to){int n=0;for(int i=std::max(1,from);i<to;++i)if(x[size_t(i-1)]<0&&x[size_t(i)]>=0)++n;return n;};
 auto energy=[](const std::vector<float>& x,int from,int to){double e=0;for(int i=from;i<to;++i)e+=double(x[size_t(i)])*x[size_t(i)];return e/double(to-from);};
 auto bright=[](const std::vector<float>& x,int from,int to){double d=0,l=0;for(int i=from+1;i<to;++i){const double a=x[size_t(i)]-x[size_t(i-1)];d+=a*a;l+=double(x[size_t(i)])*x[size_t(i)];}return d/std::max(l,1e-12);};
 // Pitch: A2 is 110 Hz.
 {oxide::Voice v;v.prepare(rate);auto s=settings();std::vector<float> x(static_cast<size_t>(fs));v.noteOn(45,false,false);v.render(x.data(),fs,s);
  const int n=crossings(x,fs/10,fs);if(!require(std::abs(n-99)<=2,"pitch"))return false;}
 // SLIDE: a slid note glides; the same note played without slide jumps.
 {auto run=[&](bool slide){oxide::Voice v;v.prepare(rate);auto s=settings();s.slideTime=.4f;std::vector<float> x(static_cast<size_t>(fs));
   v.noteOn(45,false,false);v.render(x.data(),fs/2,s);v.noteOn(57,false,slide);v.render(x.data()+fs/2,fs/2,s);return crossings(x,fs/2,fs/2+fs/10);};
  const int slid=run(true),jumped=run(false);if(!require(slid<jumped*.85&&jumped>=20,"slide glides, a played note jumps"))return false;}
 // ACCENT: louder and brighter.
 {auto run=[&](bool acc){oxide::Voice v;v.prepare(rate);auto s=settings();s.cutoff=300;s.envMod=.5f;s.resonance=.5f;s.accent=1;std::vector<float> x(static_cast<size_t>(fs/4));
   v.noteOn(40,acc,false);v.render(x.data(),fs/4,s);return std::make_pair(energy(x,0,fs/8),bright(x,0,fs/8));};
  const auto plain=run(false),hit=run(true);if(!require(hit.first>plain.first*1.6&&hit.second>plain.second*1.2,"accent: louder and brighter"))return false;}
 // ENV MOD: the filter opens at the start of the note and closes as the envelope decays.
 {oxide::Voice v;v.prepare(rate);auto s=settings();s.cutoff=200;s.envMod=1;s.decay=.3f;s.resonance=.3f;std::vector<float> x(static_cast<size_t>(fs));
  v.noteOn(40,false,false);v.render(x.data(),fs,s);if(!require(bright(x,0,fs/20)>bright(x,fs/2,fs/2+fs/20)*3,"envelope modulation"))return false;}
 // The square differs from the saw; resonance at the top stays finite and bounded; a released key fades in milliseconds.
 {auto run=[&](int wave,float res){oxide::Voice v;v.prepare(rate);auto s=settings();s.wave=wave;s.cutoff=500;s.resonance=res;s.envMod=.7f;std::vector<float> x(static_cast<size_t>(fs*2));
   v.noteOn(36,true,false);v.render(x.data(),fs,s);v.noteOff();v.render(x.data()+fs,fs,s);return x;};
  const auto saw=run(0,.2f),square=run(1,.2f),scream=run(0,1.f);double d=0;for(int i=0;i<fs/2;++i)d+=std::abs(saw[size_t(i)]-square[size_t(i)]);
  if(!require(d/(fs/2)>.01,"square differs from saw"))return false;
  for(float y:scream)if(!require(std::isfinite(y)&&std::abs(y)<4.f,"resonance at the top"))return false;
  if(!require(energy(saw,fs+fs/20,fs+fs/10)<energy(saw,fs-fs/20,fs)*1e-4,"release"))return false;}
 std::cout<<"PASS acid voice: pitch, slide, accent, envelope modulation, square, resonance, release\n";return true;
}
