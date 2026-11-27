#pragma once
// OXIDE pattern: up to 16 steps, each a note of the octave (C to the next C) with its own octave (down, middle, up),
// ACCENT, SLIDE (the note is held into the next step, which glides to its own pitch) and a gate: NOTE, REST, or TIE
// (the previous note carries on). Sixteen patterns are kept with the session. Text form, one pattern:
// "16|n,o,a,s,g;n,o,a,s,g;..." (length, then per step note 0-12, octave -1..1, accent 0/1, slide 0/1, gate 0 rest 1 note 2 tie).
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>

namespace oxide {

struct Step { int note = 0, octave = 0; bool accent = false, slide = false; int gate = 1; };
enum Gate { rest = 0, play = 1, tie = 2 };

struct Pattern {
 static constexpr int maxSteps = 16;
 std::array<Step, maxSteps> steps {}; int length = 16;
 void clear() { for (auto& s : steps) s = Step(); for (auto& s : steps) s.gate = rest; length = 16; }
 bool empty() const { for (int i = 0; i < length; ++i) if (steps[size_t(i)].gate != rest) return false; return true; }
 // The MIDI note of a step, from the pattern's C (base) and the transposition.
 int midiNote(int index, int base, int transpose) const { const auto& s = steps[size_t(index)]; return juce::jlimit(0, 127, base + transpose + s.note + 12 * s.octave); }
 juce::String toString() const {
  juce::StringArray parts; for (int i = 0; i < maxSteps; ++i) { const auto& s = steps[size_t(i)];
   parts.add(juce::String(s.note) + "," + juce::String(s.octave) + "," + juce::String(int(s.accent)) + "," + juce::String(int(s.slide)) + "," + juce::String(s.gate)); }
  return juce::String(length) + "|" + parts.joinIntoString(";");
 }
 void fromString(const juce::String& text) {
  clear(); if (!text.containsChar('|')) return;
  length = juce::jlimit(1, maxSteps, text.upToFirstOccurrenceOf("|", false, false).getIntValue());
  const auto parts = juce::StringArray::fromTokens(text.fromFirstOccurrenceOf("|", false, false), ";", "");
  for (int i = 0; i < juce::jmin(maxSteps, parts.size()); ++i) { const auto v = juce::StringArray::fromTokens(parts[i], ",", ""); if (v.size() < 5) continue;
   auto& s = steps[size_t(i)]; s.note = juce::jlimit(0, 12, v[0].getIntValue()); s.octave = juce::jlimit(-1, 1, v[1].getIntValue());
   s.accent = v[2].getIntValue() != 0; s.slide = v[3].getIntValue() != 0; s.gate = juce::jlimit(0, 2, v[4].getIntValue()); }
 }
 // RANDOM: a line in a minor mode, leaning on the root, with rests, ties, octave jumps, accents and slides: something to
 // start from, never twice the same. Our own weights, chosen by ear.
 void randomise(juce::Random& r) {
  static const int scale[] = {0, 0, 0, 3, 5, 7, 7, 10, 12, 1, 6};
  for (int i = 0; i < maxSteps; ++i) { auto& s = steps[size_t(i)]; const float roll = r.nextFloat();
   s.gate = i == 0 ? play : roll < .14f ? rest : roll < .2f ? tie : play;
   s.note = scale[r.nextInt(i % 4 == 0 ? 3 : int(std::size(scale)))];
   const float o = r.nextFloat(); s.octave = o < .18f ? 1 : o < .26f ? -1 : 0; if (s.note == 12 && s.octave == 1) s.octave = 0;
   s.accent = r.nextFloat() < (i % 4 == 0 ? .38f : .2f); s.slide = r.nextFloat() < .2f; }
 }
 // The pattern as a MIDI file (one pass, sixteenth steps, from C2 = 36 + transposition): a note lasts half its step,
 // SLIDE holds it until the next note starts (legato), TIE carries it through the step, ACCENT = velocity 127 (else 96).
 // What a DAW imports (SlyDAW, Ableton): the same notes the sequencer plays.
 juce::MidiFile toMidi(int transpose, const juce::String& name, double bpm) const {
  constexpr int ppq = 96, step = ppq / 4; juce::MidiFile file; file.setTicksPerQuarterNote(ppq); juce::MidiMessageSequence seq;
  seq.addEvent(juce::MidiMessage::tempoMetaEvent(juce::roundToInt(60000000.0 / juce::jlimit(20.0, 300.0, bpm))), 0);
  seq.addEvent(juce::MidiMessage::timeSignatureMetaEvent(4, 4), 0); seq.addEvent(juce::MidiMessage::textMetaEvent(3, name), 0);
  int sounding = -1; double end = 0;
  auto stop = [&](double at) { if (sounding >= 0) { juce::MidiMessage off = juce::MidiMessage::noteOff(1, sounding); off.setTimeStamp(at); seq.addEvent(off); sounding = -1; } };
  for (int i = 0; i < length; ++i) { const auto& s = steps[size_t(i)]; const double at = i * step;
   if (s.gate == tie) { if (sounding >= 0) end = at + step / 2.0; continue; }
   if (sounding >= 0 && end <= at) stop(end);
   if (s.gate == rest) { stop(at); continue; }
   const int note = midiNote(i, 36, transpose); const bool legato = sounding >= 0;
   if (legato && sounding == note) { end = at + step / 2.0; } // slide into the same note: it goes on
   else { juce::MidiMessage on = juce::MidiMessage::noteOn(1, note, juce::uint8(s.accent ? 127 : 96)); on.setTimeStamp(at); if (legato) { seq.addEvent(on); stop(at + 1); } else seq.addEvent(on); sounding = note; }
   end = s.slide ? (i + 1) * step + 1.0 : at + step / 2.0; // slide: held into the next step (it ends where the next note takes over)
   if (s.slide) { const auto& next = steps[size_t((i + 1) % length)]; if (i + 1 >= length || next.gate == rest) end = (i + 1) * step; } }
  stop(juce::jmin(end, double(length * step)));
  juce::MidiMessage eot = juce::MidiMessage::endOfTrack(); eot.setTimeStamp(length * step); seq.addEvent(eot);
  seq.updateMatchedPairs(); file.addTrack(seq); return file;
 }
 // A pattern turned by one step (left: the second step becomes the first).
 void rotate(bool left) { if (length < 2) return; if (left) std::rotate(steps.begin(), steps.begin() + 1, steps.begin() + length); else std::rotate(steps.begin(), steps.begin() + length - 1, steps.begin() + length); }
};

} // namespace oxide
