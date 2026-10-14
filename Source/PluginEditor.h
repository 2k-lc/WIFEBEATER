#pragma once
#include "PluginProcessor.h"

namespace wbui
{
const juce::Colour bg { 0xffffffff }, fg { 0xff000000 }, dim { 0xff808080 }, accent { 0xff0000ee }, panel { 0xffffffff },
                   visited { 0xff551a8b }, alert { 0xffff0000 }, bevel { 0xffc0c0c0 };

struct WBLookAndFeel : juce::LookAndFeel_V4
{
    WBLookAndFeel();
    static juce::Font mono (float h, bool bold = false); // name kept; this is the serif face
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool, bool) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool, int, int, int, int, juce::ComboBox&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override { return mono (15.f); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return mono (16.f); }
    juce::Font getLabelFont (juce::Label&) override { return mono (14.f); }
    juce::Font getPopupMenuFont() override { return mono (15.f); }
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
