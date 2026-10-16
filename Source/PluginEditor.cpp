#include "PluginEditor.h"

using namespace juce;
using namespace wbui;

//==============================================================================
WBLookAndFeel::WBLookAndFeel()
{
    setColour (ResizableWindow::backgroundColourId, bg);
    setColour (Slider::textBoxTextColourId, fg);
    setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (Label::textColourId, fg);
    setColour (ComboBox::backgroundColourId, bg);
    setColour (ComboBox::textColourId, fg);
    setColour (ComboBox::outlineColourId, fg);
    setColour (ComboBox::arrowColourId, fg);
    setColour (PopupMenu::backgroundColourId, bg);
    setColour (PopupMenu::textColourId, fg);
    setColour (PopupMenu::highlightedBackgroundColourId, accent);
    setColour (PopupMenu::highlightedTextColourId, bg);
    setColour (TextButton::buttonColourId, bevel);
    setColour (TextButton::textColourOffId, fg);
    setColour (TextButton::textColourOnId, fg);
}

Font WBLookAndFeel::mono (float h, bool bold)
{
    // Times New Roman where it exists (Windows/macOS); the host's default serif otherwise.
    static const bool haveTimes = Font::findAllTypefaceNames().contains ("Times New Roman");
    return Font (FontOptions (haveTimes ? String ("Times New Roman") : Font::getDefaultSerifFontName(), h,
                              bold ? Font::bold : Font::plain));
}

void WBLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, Slider&)
{
    const auto r = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (8.f);
    const float d = jmin (r.getWidth(), r.getHeight());
    const auto c = r.getCentre();
    const auto circle = Rectangle<float> (d, d).withCentre (c);
    const float ang = a0 + pos * (a1 - a0);
    g.setColour (fg); g.drawEllipse (circle, 1.f);
    // tick marks at min / max, like a printed panel
    for (float a : { a0, a1 })
        g.drawLine (c.x + std::sin (a) * d * 0.5f, c.y - std::cos (a) * d * 0.5f,
                    c.x + std::sin (a) * d * 0.68f, c.y - std::cos (a) * d * 0.68f, 1.f);
    g.drawLine (c.x, c.y, c.x + std::sin (ang) * d * 0.5f, c.y - std::cos (ang) * d * 0.5f, 2.f);
}

void WBLookAndFeel::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float pos, float, float, Slider::SliderStyle, Slider&)
{
    const float cy = (float) y + (float) h * 0.5f;
    g.setColour (fg); g.fillRect ((float) x, cy - 0.5f, (float) w, 1.f);
    g.setColour (bevel); g.fillRect (pos - 5.f, cy - 9.f, 10.f, 18.f);
    g.setColour (fg); g.drawRect (pos - 5.f, cy - 9.f, 10.f, 18.f, 1.f);
}

void WBLookAndFeel::drawToggleButton (Graphics& g, ToggleButton& b, bool, bool)
{
    const bool on = b.getToggleState();
    const auto box = Rectangle<float> (14.f, 14.f).withCentre ({ (float) b.getWidth() * 0.5f - 14.f, (float) b.getHeight() * 0.5f });
    g.setColour (bg); g.fillRect (box);
    g.setColour (fg); g.drawRect (box, 1.f);
    if (on) { g.drawLine (box.getX() + 2, box.getY() + 2, box.getRight() - 2, box.getBottom() - 2, 2.f);
              g.drawLine (box.getX() + 2, box.getBottom() - 2, box.getRight() - 2, box.getY() + 2, 2.f); }
    g.setFont (mono (15.f));
    g.drawText (b.getButtonText().isEmpty() ? (on ? "on" : "off") : b.getButtonText(),
                Rectangle<float> (box.getRight() + 5.f, 0.f, (float) b.getWidth() - box.getRight() - 5.f, (float) b.getHeight()).toNearestInt(),
                Justification::centredLeft);
}

void WBLookAndFeel::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox&)
{
    g.setColour (bg); g.fillRect (0, 0, w, h);
    g.setColour (fg); g.drawRect (0, 0, w, h, 1);
    g.setColour (bevel); g.fillRect (w - 18, 1, 17, h - 2);
    g.setColour (fg); g.drawRect (w - 18, 1, 17, h - 2, 1);
    Path t; t.addTriangle ((float) w - 14.f, (float) h * 0.4f, (float) w - 5.f, (float) h * 0.4f, (float) w - 9.5f, (float) h * 0.66f);
    g.fillPath (t);
}

void WBLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool, bool down)
{
    if (b.getComponentID() == "link") return;
    auto r = b.getLocalBounds().reduced (1);
    const bool in = down || b.getToggleState();
    g.setColour (bevel); g.fillRect (r);
    // Win95-style bevel
    g.setColour (in ? dim : Colours::white); g.drawLine ((float) r.getX(), (float) r.getY(), (float) r.getRight(), (float) r.getY(), 2.f);
    g.drawLine ((float) r.getX(), (float) r.getY(), (float) r.getX(), (float) r.getBottom(), 2.f);
    g.setColour (in ? Colours::white : dim); g.drawLine ((float) r.getX(), (float) r.getBottom(), (float) r.getRight(), (float) r.getBottom(), 2.f);
    g.drawLine ((float) r.getRight(), (float) r.getY(), (float) r.getRight(), (float) r.getBottom(), 2.f);
    g.setColour (fg); g.drawRect (r, 1);
}

void WBLookAndFeel::drawButtonText (Graphics& g, TextButton& b, bool, bool down)
{
    const bool link = b.getComponentID() == "link";
    const bool active = b.getToggleState();
    g.setFont (mono (link ? 20.f : 16.f, link && active));
    g.setColour (link ? (active ? visited : accent) : fg);
    auto r = b.getLocalBounds();
    if (! link && down) r.translate (1, 1);
    g.drawText (b.getButtonText(), r, Justification::centred);
    if (link)
    {
        const float tw = g.getCurrentFont().getStringWidthFloat (b.getButtonText());
        const float y = (float) r.getCentreY() + 10.f;
        g.fillRect ((float) r.getCentreX() - tw * 0.5f, y, tw, 1.f);
    }
}

//==============================================================================
namespace
{
// One labelled control bound to a parameter. Type is picked from the parameter.
class ParamCtl : public Component
{
public:
    ParamCtl (AudioProcessorValueTreeState& s, const String& id, const String& text = {})
    {
        auto* prm = s.getParameter (id);
        jassert (prm != nullptr);
        label.setText (text.isEmpty() ? prm->getName (32) : text, dontSendNotification);
        label.setJustificationType (Justification::centred);
        addAndMakeVisible (label);
        if (dynamic_cast<AudioParameterChoice*> (prm))
        {
            kind = 1; combo = std::make_unique<ComboBox>();
            combo->addItemList (dynamic_cast<AudioParameterChoice*> (prm)->choices, 1);
            addAndMakeVisible (*combo);
            cAtt = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment> (s, id, *combo);
        }
        else if (dynamic_cast<AudioParameterBool*> (prm))
        {
            kind = 2; toggle = std::make_unique<ToggleButton>();
            addAndMakeVisible (*toggle);
            bAtt = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (s, id, *toggle);
        }
        else
        {
            slider = std::make_unique<Slider> (Slider::RotaryHorizontalVerticalDrag, Slider::TextBoxBelow);
            slider->setTextBoxStyle (Slider::TextBoxBelow, false, 78, 19);
            slider->setColour (Slider::textBoxTextColourId, fg);
            slider->setColour (Slider::textBoxOutlineColourId, dim);
            slider->setColour (Slider::textBoxBackgroundColourId, bg);
            slider->setRotaryParameters (MathConstants<float>::pi * 1.2f, MathConstants<float>::pi * 2.8f, true);
            slider->setDoubleClickReturnValue (true, (double) prm->convertFrom0to1 (prm->getDefaultValue()));
            addAndMakeVisible (*slider);
            sAtt = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (s, id, *slider);
        }
    }
    void resized() override
    {
        auto r = getLocalBounds();
        label.setBounds (r.removeFromTop (17));
        if (slider) slider->setBounds (r);
        else if (combo) combo->setBounds (r.withSizeKeepingCentre (r.getWidth() - 4, 26));
        else if (toggle) toggle->setBounds (r.withSizeKeepingCentre (r.getWidth() - 10, 26));
    }
    int kind = 0; // 0 knob 1 combo 2 toggle
    Label label;
    std::unique_ptr<Slider> slider; std::unique_ptr<ComboBox> combo; std::unique_ptr<ToggleButton> toggle;
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> sAtt;
    std::unique_ptr<AudioProcessorValueTreeState::ComboBoxAttachment> cAtt;
    std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment> bAtt;
};

constexpr int kCtlW = 86, kCtlH = 92;

class Section : public Component
{
public:
    Section (String t) : title (std::move (t)) {}
    void add (AudioProcessorValueTreeState& s, const String& id, const String& text = {})
    {
        ctls.push_back (std::make_unique<ParamCtl> (s, id, text));
        addAndMakeVisible (*ctls.back());
    }
    int cols = 4;
    Point<int> preferred() const
    {
        const int n = (int) ctls.size(); const int c = jmax (1, jmin (cols, n)); const int rows = (n + c - 1) / c;
        return { c * kCtlW + 12, rows * kCtlH + 34 };
    }
    void paint (Graphics& g) override
    {
        g.setColour (fg); g.drawRect (getLocalBounds(), 1);
        g.setFont (WBLookAndFeel::mono (17.f, true)); g.drawText (title.toLowerCase(), 8, 1, getWidth() - 10, 20, Justification::centredLeft);
        g.fillRect (6, 22, getWidth() - 12, 1);
    }
    void resized() override
    {
        const int n = (int) ctls.size(); const int c = jmax (1, jmin (cols, n));
        for (int i = 0; i < n; ++i)
            ctls[(size_t) i]->setBounds (6 + (i % c) * kCtlW, 28 + (i / c) * kCtlH, kCtlW, kCtlH);
    }
    String title;
    std::vector<std::unique_ptr<ParamCtl>> ctls;
};

//==============================================================================
class ScopeView : public Component, private Timer
{
public:
    explicit ScopeView (WifebeaterProcessor& p) : proc (p) { startTimerHz (30); }
    void timerCallback() override { repaint(); }
    void paint (Graphics& g) override
    {
        g.setColour (bg); g.fillAll();
        g.setColour (fg); g.drawRect (getLocalBounds(), 1);
        const int n = jmin (1024, getWidth() - 40);
        const int end = proc.scopePos.load();
        const float cy = (float) getHeight() * 0.5f, hh = (float) getHeight() * 0.45f;
        Path path;
        for (int i = 0; i < n; ++i)
        {
            const int idx = ((end - n + i) % WifebeaterProcessor::kScope + WifebeaterProcessor::kScope) % WifebeaterProcessor::kScope;
            const float v = jlimit (-1.2f, 1.2f, proc.scope[(size_t) idx].load());
            const float x = 4.f + (float) i / (float) n * (float) (getWidth() - 40);
            if (i == 0) path.startNewSubPath (x, cy - v * hh); else path.lineTo (x, cy - v * hh);
        }
        g.setColour (fg); g.strokePath (path, PathStrokeType (1.f));
        // peak meter
        const float pk = proc.peakMeter.load();
        const float fill = jlimit (0.f, 1.f, pk) * (float) (getHeight() - 8);
        g.setColour (bevel); g.fillRect ((float) getWidth() - 28.f, 4.f, 20.f, (float) getHeight() - 8.f);
        g.setColour (pk > 0.93f ? alert : fg);
        g.fillRect ((float) getWidth() - 28.f, (float) getHeight() - 4.f - fill, 20.f, fill);
    }
    WifebeaterProcessor& proc;
};

class SampleView : public Component, private Timer
{
public:
    explicit SampleView (WifebeaterProcessor& p) : proc (p) { startTimerHz (12); }
    void timerCallback() override
    {
        auto s = proc.getSample();
        if (s != cached || getWidth() != cachedW) rebuild (s);
        repaint();
    }
    void rebuild (std::shared_ptr<const wb::SampleData> s)
    {
        cached = s; cachedW = getWidth(); peaks.clear();
        if (! s || s->data.empty() || getWidth() < 4) return;
        const int w = getWidth() - 4; const size_t n = s->data.size();
        for (int x = 0; x < w; ++x)
        {
            const size_t a = (size_t) x * n / (size_t) w, b = jmax (a + 1, (size_t) (x + 1) * n / (size_t) w);
            float mn = 0, mx = 0;
            for (size_t i = a; i < b && i < n; i += std::max<size_t> (1, (b - a) / 64)) { mn = jmin (mn, s->data[i]); mx = jmax (mx, s->data[i]); }
            peaks.push_back ({ mn, mx });
        }
    }
    void paint (Graphics& g) override
    {
        g.setColour (bg); g.fillAll(); g.setColour (fg); g.drawRect (getLocalBounds(), 1);
        const float cy = (float) getHeight() * 0.5f, hh = (float) getHeight() * 0.45f;
        const float start = proc.apvts.getRawParameterValue ("sStart")->load();
        const float len = proc.apvts.getRawParameterValue ("sLen")->load();
        const float w = (float) getWidth() - 4.f;
        g.setColour (bevel); g.fillRect (2.f + start * w, 2.f, jmin (len, 1.f - start) * w, (float) getHeight() - 4.f);
        g.setColour (fg);
        for (size_t x = 0; x < peaks.size(); ++x)
            g.drawVerticalLine (2 + (int) x, cy - peaks[x].second * hh, cy - peaks[x].first * hh);
        g.setColour (alert); g.drawVerticalLine ((int) (2.f + start * w), 2.f, (float) getHeight() - 2.f);
        g.setColour (fg); g.setFont (WBLookAndFeel::mono (13.f));
        g.drawText (cached ? cached->name : String ("(no sample)"), 8, 4, getWidth() - 16, 14, Justification::topRight);
    }
    WifebeaterProcessor& proc; std::shared_ptr<const wb::SampleData> cached; int cachedW = 0;
    std::vector<std::pair<float, float>> peaks;
};

class CurveEditor : public Component
{
public:
    explicit CurveEditor (WifebeaterProcessor& p) : proc (p) {}
    void paint (Graphics& g) override
    {
        g.setColour (bg); g.fillAll(); g.setColour (fg); g.drawRect (getLocalBounds(), 1);
        g.setFont (WBLookAndFeel::mono (17.f, true)); g.drawText ("drawn curve", 8, 1, getWidth() - 10, 20, Justification::centredLeft);
        g.setFont (WBLookAndFeel::mono (13.f)); g.drawText ("(drag to draw)", 120, 3, getWidth() - 126, 16, Justification::centredRight);
        g.fillRect (6, 22, getWidth() - 12, 1);
        auto r = area();
        g.setColour (bevel); g.drawLine (r.getX(), r.getCentreY(), r.getRight(), r.getCentreY()); g.drawLine (r.getCentreX(), r.getY(), r.getCentreX(), r.getBottom());
        Path path;
        for (int i = 0; i < wb::kCurvePoints; ++i)
        {
            const float x = r.getX() + r.getWidth() * (float) i / (float) (wb::kCurvePoints - 1);
            const float y = r.getBottom() - (proc.curve[(size_t) i].load() * 0.5f + 0.5f) * r.getHeight();
            if (i == 0) path.startNewSubPath (x, y); else path.lineTo (x, y);
        }
        g.setColour (fg); g.strokePath (path, PathStrokeType (2.f));
    }
    Rectangle<float> area() const { return getLocalBounds().toFloat().reduced (8.f).withTrimmedTop (18.f); }
    void set (Point<float> pt)
    {
        auto r = area();
        const int i = jlimit (0, wb::kCurvePoints - 1, (int) std::round ((pt.x - r.getX()) / r.getWidth() * (float) (wb::kCurvePoints - 1)));
        const float v = jlimit (-1.f, 1.f, ((r.getBottom() - pt.y) / r.getHeight()) * 2.f - 1.f);
        if (last >= 0 && last != i)
            for (int k = jmin (last, i); k <= jmax (last, i); ++k)
            {
                const float t = (float) (k - last) / (float) (i - last);
                proc.curve[(size_t) k].store (lastV + t * (v - lastV));
            }
        else proc.curve[(size_t) i].store (v);
        last = i; lastV = v; repaint();
    }
    void mouseDown (const MouseEvent& e) override { last = -1; set (e.position); }
    void mouseDrag (const MouseEvent& e) override { set (e.position); }
    void mouseUp (const MouseEvent&) override { proc.curveToState(); }
    WifebeaterProcessor& proc; int last = -1; float lastV = 0;
};

struct Page : Component
{
    std::vector<std::unique_ptr<Section>> secs;
    std::vector<Component*> extra;      // laid out by subclass-specific code
    int topReserved = 0;
    Section& add (const String& t, int cols = 4) { secs.push_back (std::make_unique<Section> (t)); secs.back()->cols = cols; addAndMakeVisible (*secs.back()); return *secs.back(); }
    void resized() override
    {
        int x = 0, y = topReserved, rowH = 0; const int W = getWidth();
        for (auto& s : secs)
        {
            auto sz = s->preferred();
            if (x > 0 && x + sz.x > W) { x = 0; y += rowH + 8; rowH = 0; }
            s->setBounds (x, y, sz.x, sz.y); x += sz.x + 8; rowH = jmax (rowH, sz.y);
        }
        flowBottom = y + rowH; flowX = x; flowY = y; flowRowH = rowH;
    }
    int flowBottom = 0, flowX = 0, flowY = 0, flowRowH = 0;
};
}

//==============================================================================
struct WifebeaterEditor::Impl
{
    Impl (WifebeaterProcessor& p, WifebeaterEditor& ed)
        : proc (p), editor (ed), scopeView (p), sampleView (p), curveEd (p)
    {
        auto& s = p.apvts;
        // ---- header
        for (auto id : { "gain", "safe", "drone" }) { header.push_back (std::make_unique<ParamCtl> (s, id)); ed.addAndMakeVisible (*header.back()); }
        header[1]->label.setText ("Safe limiter", dontSendNotification);
        header[2]->label.setText ("Drone", dontSendNotification);

        // ---- pages
        auto* src = pages.emplace_back (std::make_unique<Page>()).get();
        { auto& a = src->add ("NOISE", 3); a.add (s, "nLevel", "Level"); a.add (s, "nType", "Type"); a.add (s, "nColor", "Color");
          auto& c = src->add ("CHAOS", 4); c.add (s, "cLevel", "Level"); c.add (s, "cType", "Type"); c.add (s, "cRate", "Rate"); c.add (s, "cAmt", "Amount");
          auto& o = src->add ("OSCILLATOR", 4); o.add (s, "oLevel", "Level"); o.add (s, "oWave", "Wave"); o.add (s, "oTune", "Tune"); o.add (s, "oDetune", "Detune");
          o.add (s, "oRatio", "B Ratio"); o.add (s, "oFM", "FM"); o.add (s, "oRing", "Ring"); o.add (s, "oSync", "Sync");
          auto& r = src->add ("RESONATOR", 4); r.add (s, "rLevel", "Level"); r.add (s, "rDecay", "Decay"); r.add (s, "rDamp", "Damp"); r.add (s, "rExcite", "Excite");
          auto& e = src->add ("AMP ENVELOPE", 4); e.add (s, "eA", "A"); e.add (s, "eD", "D"); e.add (s, "eS", "S"); e.add (s, "eR", "R");
          auto& m = src->add ("PITCH / INPUT", 3); m.add (s, "root", "Root"); m.add (s, "inLevel", "Audio In"); m.add (s, "width", "Width"); }

        auto* smp = pages.emplace_back (std::make_unique<Page>()).get();
        { smp->topReserved = 112;
          auto& a = smp->add ("SAMPLE", 4); a.add (s, "sLevel", "Level"); a.add (s, "sMode", "Mode"); a.add (s, "sKey", "Keytrack"); a.add (s, "sRev", "Reverse");
          a.add (s, "sStart", "Start"); a.add (s, "sLen", "Length"); a.add (s, "sPitch", "Pitch");
          auto& g = smp->add ("GRANULAR", 5); g.add (s, "gSize", "Size"); g.add (s, "gDens", "Density"); g.add (s, "gScatPos", "Pos Scat"); g.add (s, "gScatPitch", "Pitch Scat"); g.add (s, "gRev", "Reverse");
          auto& x = smp->add ("GLITCH", 5); x.add (s, "xRate", "Clock"); x.add (s, "xSlice", "Slice"); x.add (s, "xChance", "Repeat"); x.add (s, "xRev", "Reverse"); x.add (s, "xStop", "Tape Stop");
          auto& w = smp->add ("WAVETABLE", 4); w.add (s, "wPos", "Position"); w.add (s, "wWarp", "Warp"); w.add (s, "wFM", "FM"); w.add (s, "wRing", "Ring");
          smp->addAndMakeVisible (sampleView); smp->addAndMakeVisible (loadBtn); smp->addAndMakeVisible (defaultBtn);
          loadBtn.setButtonText ("Load sample..."); defaultBtn.setButtonText ("Use built-in sample");
          loadBtn.onClick = [this] {
              chooser = std::make_unique<FileChooser> ("Load a sample", File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
              chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [this] (const FileChooser& fc) {
                  if (fc.getResult().existsAsFile()) proc.loadSampleFile (fc.getResult()); }); };
          defaultBtn.onClick = [this] { proc.useDefaultSample(); }; }

        auto* spc = pages.emplace_back (std::make_unique<Page>()).get();
        { auto& a = spc->add ("SPECTRAL MANGLER", 5); a.add (s, "spMode", "Mode"); a.add (s, "spAmt", "Mix"); a.add (s, "spX", "X"); a.add (s, "spY", "Y"); a.add (s, "spHold", "Hold"); }

        auto* dst = pages.emplace_back (std::make_unique<Page>()).get();
        { for (int i = 1; i <= 3; ++i) {
              const String n = "d" + String (i);
              auto& d = dst->add ("DISTORTION " + String (i), 3); d.add (s, n + "On", "Active"); d.add (s, n + "Type", "Type"); d.add (s, n + "Mix", "Mix");
              d.add (s, n + "Drive", "Drive"); d.add (s, n + "X", "X"); d.add (s, n + "Tone", "Tone"); }
          auto& f = dst->add ("FEEDBACK", 3); f.add (s, "fbAmt", "Amount"); f.add (s, "fbTime", "Time"); f.add (s, "fbTone", "Tone");
          auto& fl = dst->add ("FILTER", 3); fl.add (s, "fType", "Type"); fl.add (s, "fCut", "Cutoff"); fl.add (s, "fRes", "Reso");
          dst->addAndMakeVisible (curveEd); resetCurveBtn.setButtonText ("Reset"); randCurveBtn.setButtonText ("Random");
          dst->addAndMakeVisible (resetCurveBtn); dst->addAndMakeVisible (randCurveBtn);
          resetCurveBtn.onClick = [this] { proc.resetCurve(); proc.curveToState(); curveEd.repaint(); };
          randCurveBtn.onClick = [this] { Random r; for (auto& c : proc.curve) c.store (r.nextFloat() * 2.f - 1.f); proc.curveToState(); curveEd.repaint(); }; }

        auto* mod = pages.emplace_back (std::make_unique<Page>()).get();
        { auto& m = mod->add ("MODULATORS", 8); m.add (s, "l1Rate", "LFO1 Rate"); m.add (s, "l1Shape", "LFO1 Shape"); m.add (s, "l2Rate", "LFO2 Rate"); m.add (s, "l2Shape", "LFO2 Shape");
          m.add (s, "meA", "Env A"); m.add (s, "meD", "Env D"); m.add (s, "mcRate", "Chaos Rate"); m.add (s, "mrRate", "Rand Rate");
          for (int i = 1; i <= wb::kNumModSlots; ++i)
              slotCtl.push_back ({ std::make_unique<ParamCtl> (s, "m" + String (i) + "Src", "Source"), std::make_unique<ParamCtl> (s, "m" + String (i) + "Dst", "Destination"),
                                   std::make_unique<ParamCtl> (s, "m" + String (i) + "Amt", "Amount") });
          for (auto& t : slotCtl) for (auto* c : { t[0].get(), t[1].get(), t[2].get() }) {
              mod->addAndMakeVisible (*c);
              if (c->slider) { c->slider->setSliderStyle (Slider::LinearHorizontal); c->slider->setTextBoxStyle (Slider::TextBoxRight, false, 60, 20); } } }

        for (auto& pg : pages) ed.addAndMakeVisible (*pg);
        const char* names[] = { "sources", "sampler", "spectral", "distortion", "modulation" };
        for (int i = 0; i < 5; ++i)
        {
            auto b = std::make_unique<TextButton> (names[i]); b->setComponentID ("link"); b->setClickingTogglesState (true); b->setRadioGroupId (77);
            b->onClick = [this, i] { editor.showPage (i); }; ed.addAndMakeVisible (*b); tabs.push_back (std::move (b));
        }
        mangleBtn.setButtonText ("Mangle!"); ed.addAndMakeVisible (mangleBtn);
        mangleAmt.setSliderStyle (Slider::LinearHorizontal); mangleAmt.setRange (0.0, 1.0, 0.01); mangleAmt.setValue (0.5);
        mangleAmt.setTextBoxStyle (Slider::NoTextBox, false, 0, 0); ed.addAndMakeVisible (mangleAmt);
        mangleBtn.onClick = [this] { editor.mangle ((float) mangleAmt.getValue()); };
        ed.addAndMakeVisible (scopeView);
    }

    WifebeaterProcessor& proc; WifebeaterEditor& editor;
    std::vector<std::unique_ptr<ParamCtl>> header;
    std::vector<std::unique_ptr<Page>> pages;
    std::vector<std::unique_ptr<TextButton>> tabs;
    TextButton mangleBtn, loadBtn, defaultBtn, resetCurveBtn, randCurveBtn;
    Slider mangleAmt;
    ScopeView scopeView; SampleView sampleView; CurveEditor curveEd;
    std::vector<std::array<std::unique_ptr<ParamCtl>, 3>> slotCtl;
    std::unique_ptr<FileChooser> chooser;
};

//==============================================================================
WifebeaterEditor::WifebeaterEditor (WifebeaterProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&laf);
    impl = std::make_unique<Impl> (p, *this);
    setSize (1060, 740);
    impl->tabs[0]->setToggleState (true, dontSendNotification);
    showPage (0);
    startTimerHz (15);
}

WifebeaterEditor::~WifebeaterEditor() { setLookAndFeel (nullptr); }

void WifebeaterEditor::showPage (int i)
{
    page = i;
    for (size_t k = 0; k < impl->pages.size(); ++k) impl->pages[k]->setVisible ((int) k == i);
    resized();
}

void WifebeaterEditor::timerCallback() {}

void WifebeaterEditor::filesDropped (const StringArray& files, int, int)
{
    if (files.size() > 0 && proc.loadSampleFile (File (files[0]))) { impl->tabs[1]->setToggleState (true, dontSendNotification); showPage (1); }
}

void WifebeaterEditor::paint (Graphics& g)
{
    g.fillAll (bg);
    g.setColour (fg);
    g.setFont (laf.mono (54.f, true));
    g.drawText ("WIFEBEATER", 12, 0, 520, 56, Justification::centredLeft);
    g.setFont (laf.mono (16.f).italicised());
    g.drawText ("harsh noise instrument v0.1 - best viewed in Netscape Navigator 4.0 at 800x600", 14, 52, 700, 20, Justification::centredLeft);
    // <hr>
    g.setColour (dim); g.fillRect (10, 100, getWidth() - 20, 2);
    g.setColour (Colours::white); g.fillRect (10, 102, getWidth() - 20, 1);
}

void WifebeaterEditor::resized()
{
    auto r = getLocalBounds().reduced (10);
    auto head = r.removeFromTop (88);
    auto hc = head.removeFromRight (3 * kCtlW);
    for (size_t i = 0; i < impl->header.size(); ++i) impl->header[i]->setBounds (hc.removeFromLeft (kCtlW).withHeight (88));
    r.removeFromTop (6);
    auto bar = r.removeFromTop (34);
    for (auto& t : impl->tabs) t->setBounds (bar.removeFromLeft (118));
    impl->mangleBtn.setBounds (bar.removeFromRight (100).reduced (2, 3));
    impl->mangleAmt.setBounds (bar.removeFromRight (140));
    auto foot = r.removeFromBottom (92);
    impl->scopeView.setBounds (foot);
    r.removeFromTop (6); r.removeFromBottom (6);
    for (auto& pg : impl->pages) pg->setBounds (r);

    // page-specific extras
    auto* smp = impl->pages[1].get();
    impl->sampleView.setBounds (0, 0, smp->getWidth() - 220, 104);
    impl->sampleView.timerCallback();
    impl->loadBtn.setBounds (smp->getWidth() - 208, 0, 200, 48);
    impl->defaultBtn.setBounds (smp->getWidth() - 208, 56, 200, 48);

    auto* dst = impl->pages[3].get();
    const int cy = dst->flowBottom + 8;
    impl->curveEd.setBounds (0, cy, jmin (360, dst->getWidth() / 2), jmax (120, dst->getHeight() - cy - 36));
    impl->resetCurveBtn.setBounds (impl->curveEd.getRight() + 8, cy, 100, 30);
    impl->randCurveBtn.setBounds (impl->curveEd.getRight() + 8, cy + 36, 100, 30);

    auto* mod = impl->pages[4].get();
    int y = mod->secs[0]->getBottom() + 10;
    const int rowH = jmax (30, (mod->getHeight() - y) / wb::kNumModSlots);
    for (auto& t : impl->slotCtl)
    {
        for (auto& c : t) c->label.setVisible (false);
        t[0]->setBounds (0, y, 190, rowH - 4);
        t[1]->setBounds (200, y, 300, rowH - 4);
        t[2]->setBounds (510, y, 360, rowH - 4);
        y += rowH;
    }
    for (auto* c : { impl->slotCtl[0][0].get(), impl->slotCtl[0][1].get(), impl->slotCtl[0][2].get() }) juce::ignoreUnused (c);
}

void WifebeaterEditor::mangle (float amount)
{
    static const StringArray keep { "gain", "safe", "root", "inLevel", "width", "drone", "sKey" };
    Random rnd;
    for (auto* prm : proc.getParameters())
    {
        auto* rp = dynamic_cast<RangedAudioParameter*> (prm);
        if (rp == nullptr || keep.contains (rp->paramID)) continue;
        // keep mod routing amounts gentle unless mangling hard
        const float cur = rp->getValue();
        float target = rnd.nextFloat();
        if (dynamic_cast<AudioParameterChoice*> (rp) == nullptr && dynamic_cast<AudioParameterBool*> (rp) == nullptr)
            target = rnd.nextFloat();
        float nv;
        if (dynamic_cast<AudioParameterChoice*> (rp) != nullptr || dynamic_cast<AudioParameterBool*> (rp) != nullptr)
            nv = rnd.nextFloat() < amount * 0.5f ? target : cur;
        else
            nv = jlimit (0.f, 1.f, cur + (target - cur) * amount);
        rp->beginChangeGesture(); rp->setValueNotifyingHost (nv); rp->endChangeGesture();
    }
}
