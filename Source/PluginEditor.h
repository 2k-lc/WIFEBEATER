#pragma once
#include "PluginProcessor.h"

namespace wbui
{
const juce::Colour bg { 0xff0a0a0a }, fg { 0xfff0f0f0 }, dim { 0xff3a3a3a }, accent { 0xffff2200 }, panel { 0xff141414 };

struct WBLookAndFeel : juce::LookAndFeel_V4
{
    WBLookAndFeel();
    juce::Font mono (float h, bool bold = true) const;
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool, bool) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool, int, int, int, int, juce::ComboBox&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return mono (12.f); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return mono (13.f); }
    juce::Font getLabelFont (juce::Label&) override { return mono (11.f); }
    juce::Font getPopupMenuFont() override { return mono (13.f); }
};
}

class WifebeaterEditor : public juce::AudioProcessorEditor, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    explicit WifebeaterEditor (WifebeaterProcessor&);
    ~WifebeaterEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void filesDropped (const juce::StringArray&, int, int) override;

    void showPage (int);

private:
    void timerCallback() override;
    void mangle (float amount);

    struct Impl;
    std::unique_ptr<Impl> impl;
    WifebeaterProcessor& proc;
    wbui::WBLookAndFeel laf;
    int page = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WifebeaterEditor)
};
