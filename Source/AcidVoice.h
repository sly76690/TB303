#pragma once
// OXIDE: the acid voice. One oscillator (saw, or a square drawn from it), a resonant four-pole low-pass whose resonance
// loop is high-passed (the bass thins out as the resonance rises), a filter envelope that only decays, an accent circuit
// that piles up when accents follow each other, and slide (the pitch glides to the next note without a new attack).
// Monophonic. Rendered at twice the sample rate; the processor brings it back down through a half-band filter.
// Principles from public descriptions of acid bass synthesizers; every value here is our own, tuned by ear.
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

namespace oxide {

struct Settings {
 int wave = 0;              // 0 saw, 1 square
 float tune = 0;            // semitones
 float cutoff = 400;        // Hz, the filter with the envelope at rest
 float resonance = .5f;     // 0..1
 float envMod = .5f;        // 0..1: how far the envelope opens the filter (up to about four octaves)
 float decay = .6f;         // seconds: the filter envelope (accented notes always use the shortest)
 float accent = .5f;        // 0..1: louder and brighter accented notes, and the "wow" when accents follow each other
 float slideTime = .06f;    // seconds: the glide of a slid note
 float ampDecay = 3.f;      // seconds: the level fades while the key is held
 float bend = 0;            // semitones, from the pitch wheel
};

class Voice {
public:
 void prepare(double sampleRate) { fs = 2. * sampleRate; reset(); }
 void reset() {
  phase = 0; for (auto& s : stage) s = 0; feedbackHp = feedbackIn = 0; dc = dcIn = 0; squareTilt = 0;
  filterEnv = 0; ampLevel = 0; ampTarget = 0; accentCap = 0; accentNow = 0; gate = false; current = -1; logPitch = targetPitch = std::log2(110.);
 }
 // A new note. slide: glide from the note sounding (no new envelope); otherwise the envelopes start again.
 void noteOn(int note, bool accented, bool slide) {
  targetPitch = std::log2(440. * std::pow(2., (note - 69) / 12.));
  const bool sliding = slide && gate && current >= 0;
  if (!sliding) { logPitch = targetPitch; filterEnv = 1.f; ampTarget = 1.f; attackLeft = int(fs * .003); accentNow = accented ? 1.f : 0.f; isAccented = accented; }
  else { gliding = true; } // a slid note keeps the accent and the envelopes of the note it comes from
  current = note; gate = true;
 }
 void noteOff() { gate = false; }
 bool active() const { return gate || ampLevel > 1.e-5f; }
 int note() const { return current; }
 bool sliding() const { return gliding; }

 // Renders count samples at twice the base rate into out (mono), adding nothing else.
 void render(float* out, int count, const Settings& p) {
  const double glideRate = 1. - std::exp(-1. / (juce::jmax(.005, double(p.slideTime)) * fs / 3.));
  const float decaySeconds = isAccented ? .2f : juce::jmax(.2f, p.decay);
  const float envFall = std::exp(-4.6f / (decaySeconds * float(fs)));            // to -40 dB over the decay time
  const float ampFall = std::exp(-4.6f / (juce::jmax(.1f, p.ampDecay) * float(fs)));
  const float release = std::exp(-1.f / (.006f * float(fs)));                    // a key let go: gone in a few ms
  const float capCharge = 1.f - std::exp(-1.f / (.025f * float(fs))), capLeak = 1.f - std::exp(-1.f / (.32f * float(fs)));
  const float k = 4.1f * std::pow(juce::jlimit(0.f, 1.f, p.resonance), .9f);    // a hair below the point where it would sing on its own
  const float hpCoef = 1.f - std::exp(-juce::MathConstants<float>::twoPi * 180.f / float(fs)); // the resonance loop's high-pass
  const float tiltCoef = 1.f - std::exp(-juce::MathConstants<float>::twoPi * 60.f / float(fs)); // the square droops a little
  const float dcCoef = 1.f - std::exp(-juce::MathConstants<float>::twoPi * 8.f / float(fs));
  const double tuneOffset = (p.tune + p.bend) / 12.;
  for (int i = 0; i < count; ++i) {
   // Pitch: a slid note glides on a curve; a played note jumps.
   if (gliding) { logPitch += (targetPitch - logPitch) * glideRate; if (std::abs(targetPitch - logPitch) < 1.e-5) { logPitch = targetPitch; gliding = false; } }
   const double hz = juce::jlimit(8., fs * .45, std::exp2(logPitch + tuneOffset)), dt = hz / fs;
   // Oscillator: a band-limited saw (polyBLEP); the square is shaped from the same ramp and droops like a coupled stage.
   phase += dt; if (phase >= 1.) phase -= 1.;
   float saw = float(2. * phase - 1.) - blep(phase, dt);
   float osc = saw;
   if (p.wave == 1) {
    double other = phase + .5; if (other >= 1.) other -= 1.;
    float square = (phase < .5 ? 1.f : -1.f) + blep(phase, dt) - blep(other, dt);
    squareTilt += (square - squareTilt) * tiltCoef; osc = .9f * (square - .55f * squareTilt);
   }
   // Envelopes. The amp opens in 3 ms, fades while held, and closes fast on release. The filter envelope only decays.
   if (gate) { if (attackLeft > 0) { --attackLeft; ampLevel += (1.f - ampLevel) * .02f; } else ampLevel *= ampFall; }
   else ampLevel *= release;
   filterEnv *= envFall;
   // ACCENT: the accented envelope charges a capacitor that leaks slowly, so accents in a row open the filter further.
   const float accentDrive = accentNow * p.accent * filterEnv;
   accentCap += (accentDrive - accentCap) * (accentDrive > accentCap ? capCharge : capLeak);
   const float octaves = p.envMod * 4.2f * filterEnv * (1.f + .35f * accentNow * p.accent) + 2.6f * accentCap;
   const float fc = juce::jlimit(20.f, float(fs * .42), p.cutoff * std::exp2(octaves));
   const float g = std::tan(juce::MathConstants<float>::pi * fc / float(fs)), G = g / (1.f + g);
   // Four one-pole stages; the feedback from the last one goes through a high-pass, and the input is soft-clipped.
   feedbackHp += (stage[3] - feedbackHp) * hpCoef; const float loop = stage[3] - feedbackHp;
   float x = std::tanh((osc * (1.f + .45f * k) - k * loop) * .8f) * 1.25f;
   for (auto& s : stage) { const float v = (x - s) * G; const float y = v + s; s = y + v; x = y; }
   float y = x * ampLevel * (1.f + accentNow * p.accent * .9f);
   dc += (y - dc) * dcCoef; out[i] = (y - dc) * .3f;
  }
 }

private:
 static float blep(double t, double dt) {
  if (t < dt) { const double x = t / dt; return float(x + x - x * x - 1.); }
  if (t > 1. - dt) { const double x = (t - 1.) / dt; return float(x * x + x + x + 1.); }
  return 0.f;
 }
 double fs = 96000, phase = 0, logPitch = 0, targetPitch = 0;
 float stage[4] {}; float feedbackHp = 0, feedbackIn = 0, dc = 0, dcIn = 0, squareTilt = 0;
 float filterEnv = 0, ampLevel = 0, ampTarget = 0, accentCap = 0, accentNow = 0;
 int attackLeft = 0, current = -1; bool gate = false, gliding = false, isAccented = false;
};

} // namespace oxide
