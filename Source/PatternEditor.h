// ---- OXIDE PATTERN: the sixteen steps on the panel ---------------------------------------------------------------
// Each column is a step: its number (click: the pattern ends there), the note (drag up or down, click to select the
// step that WRITE fills from the keys), UP and DOWN (the octave), ACC, SLIDE, and the gate (click: NOTE, REST, TIE).
// The step playing lights up. Everything is drawn here and read from the processor, so the host and WRITE stay in step.
class PatternGrid : public Component {
public:
 explicit PatternGrid(Orbit& o) : orbit(o) { setWantsKeyboardFocus(false); }
 void refresh() {
  const int index = orbit.patternIndex(); const auto text = orbit.patternText(index);
  const int playing = orbit.playingStep(), selected = orbit.editStep.load();
  if (text != shownText || index != shownIndex || playing != shownPlaying || selected != shownSelected) {
   shownText = text; shownIndex = index; shownPlaying = playing; shownSelected = selected; pattern.fromString(text); repaint(); }
 }
 void paint(Graphics& g) override {
  using namespace orbitui;
  static const char* names[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B","C'"}; // the C above: C'
  for (int i = 0; i < oxide::Pattern::maxSteps; ++i) {
   const auto col = column(i).toFloat(); const auto& s = pattern.steps[size_t(i)]; const bool inside = i < pattern.length;
   const float alpha = inside ? 1.f : .3f;
   // header: the step number, lit while it plays; END after the last step
   auto head = col.withHeight(22.f);
   g.setColour((i == shownPlaying ? teal : Colour(0xff1b1d1f)).withMultipliedAlpha(alpha)); g.fillRoundedRectangle(head.reduced(1), 3);
   g.setColour(i == shownPlaying ? steelInk : dim.withMultipliedAlpha(alpha)); g.setFont(face(14.f, true)); g.drawText(String(i + 1), head, Justification::centred);
   if (i == pattern.length - 1) { g.setColour(teal); g.fillRect(col.getRight() - 3.f, col.getY(), 3.f, col.getHeight()); }
   // the note
   auto noteBox = noteArea(i).toFloat(); const bool on = s.gate == oxide::play;
   g.setColour(Colour(0xff0e0f10)); g.fillRoundedRectangle(noteBox, 4);
   g.setColour(i == shownSelected ? led : Colour(0xff34373a)); g.drawRoundedRectangle(noteBox, 4, i == shownSelected ? 2.f : 1.f);
   g.setColour((on ? ink : dim.withAlpha(.6f)).withMultipliedAlpha(alpha)); g.setFont(face(22.f, true));
   g.drawText(s.gate == oxide::rest ? String("-") : s.gate == oxide::tie ? String("~") : String(names[s.note]), noteBox, Justification::centred);
   // the switches
   const std::array<std::pair<const char*, bool>, 4> keys {{{"UP", s.octave > 0}, {"DOWN", s.octave < 0}, {"ACC", s.accent}, {"SLIDE", s.slide}}};
   for (int k = 0; k < 4; ++k) drawKey(g, keyArea(i, k).toFloat(), keys[size_t(k)].first, keys[size_t(k)].second, alpha);
   const char* gate = s.gate == oxide::rest ? "REST" : s.gate == oxide::tie ? "TIE" : "NOTE";
   drawKey(g, keyArea(i, 4).toFloat(), gate, s.gate == oxide::play, alpha);
  }
 }
 void mouseDown(const MouseEvent& e) override {
  dragStep = -1;
  for (int i = 0; i < oxide::Pattern::maxSteps; ++i) {
   if (column(i).withHeight(22).contains(e.getPosition())) { orbit.editPattern([i](oxide::Pattern& p) { p.length = i + 1; }); return; }
   if (noteArea(i).contains(e.getPosition())) { dragStep = i; dragStartY = e.y; dragNote = pattern.steps[size_t(i)].note; orbit.editStep.store(i);
    if (pattern.steps[size_t(i)].gate != oxide::play) orbit.editPattern([i](oxide::Pattern& p) { p.steps[size_t(i)].gate = oxide::play; });
    refresh(); return; }
   for (int k = 0; k < 5; ++k) if (keyArea(i, k).contains(e.getPosition())) {
    orbit.editPattern([i, k](oxide::Pattern& p) { auto& s = p.steps[size_t(i)];
     if (k == 0) s.octave = s.octave > 0 ? 0 : 1; else if (k == 1) s.octave = s.octave < 0 ? 0 : -1;
     else if (k == 2) s.accent = !s.accent; else if (k == 3) s.slide = !s.slide; else s.gate = (s.gate + 2) % 3; }); // NOTE > REST > TIE
    return; }
  }
 }
 void mouseDrag(const MouseEvent& e) override {
  if (dragStep < 0) return; const int note = jlimit(0, 12, dragNote + (dragStartY - e.y) / 8);
  if (note != pattern.steps[size_t(dragStep)].note) { const int i = dragStep; orbit.editPattern([i, note](oxide::Pattern& p) { p.steps[size_t(i)].note = note; }); refresh(); }
 }
private:
 Rectangle<int> column(int i) const { const float w = float(getWidth()) / float(oxide::Pattern::maxSteps); return Rectangle<int>(roundToInt(float(i) * w) + 2, 0, roundToInt(w) - 4, getHeight()); }
 Rectangle<int> noteArea(int i) const { return column(i).withTrimmedTop(26).withHeight(40); }
 Rectangle<int> keyArea(int i, int k) const { auto c = column(i).withTrimmedTop(70); const int h = (c.getHeight() - 4) / 5; return c.withTrimmedTop(k * (h + 1)).withHeight(h); }
 void drawKey(Graphics& g, Rectangle<float> r, const char* text, bool lit, float alpha) {
  using namespace orbitui;
  g.setColour((lit ? Colour(0xffe8c020) : Colour(0xff222427)).withMultipliedAlpha(alpha)); g.fillRoundedRectangle(r.reduced(1), 3);
  g.setColour((lit ? steelInk : dim).withMultipliedAlpha(alpha)); g.setFont(face(11.f, true)); g.drawText(text, r, Justification::centred);
 }
 Orbit& orbit; oxide::Pattern pattern; String shownText; int shownIndex = -1, shownPlaying = -2, shownSelected = -2, dragStep = -1, dragStartY = 0, dragNote = 0;
};

// The sixteen pattern memories, four by four: the PATTERN parameter (a host can automate it).
class PatternKeys : public Component {
public:
 explicit PatternKeys(RangedAudioParameter& p) : attachment(p, [this](float) { repaint(); }) { getProperties().set("parameterId", p.paramID); attachment.sendInitialUpdate(); setWantsKeyboardFocus(false); }
 void paint(Graphics& g) override {
  using namespace orbitui; const int chosen = roundToInt(attachment.getValue());
  for (int i = 0; i < 16; ++i) { const auto r = key(i).toFloat(); const bool on = i == chosen;
   g.setColour(on ? Colour(0xffe8c020) : Colour(0xff222427)); g.fillRoundedRectangle(r.reduced(1.5f), 3);
   g.setColour(on ? steelInk : ink); g.setFont(face(13.f, true)); g.drawText(String(i + 1), r, Justification::centred); }
 }
 void mouseDown(const MouseEvent& e) override { for (int i = 0; i < 16; ++i) if (key(i).contains(e.getPosition())) { attachment.setValueAsCompleteGesture(float(i)); return; } }
private:
 Rectangle<int> key(int i) const { const int w = getWidth() / 4, h = getHeight() / 4; return {(i % 4) * w, (i / 4) * h, w, h}; }
 struct Attachment : ParameterAttachment { Attachment(RangedAudioParameter& p, std::function<void(float)> f) : ParameterAttachment(p, std::move(f)), parameter(p) {}
  float getValue() const { return parameter.convertFrom0to1(parameter.getValue()); } RangedAudioParameter& parameter; } attachment;
};
