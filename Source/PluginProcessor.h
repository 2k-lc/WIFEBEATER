#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "Engine.h"

class WifebeaterProcessor : public juce::AudioProcessor
{
public:
    WifebeaterProcessor();
    ~WifebeaterProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    using juce::AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "WIFEBEATER"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // sample handling (message thread)
    bool loadSampleFile (const juce::File& f);
    void useDefaultSample();
    std::shared_ptr<const wb::SampleData> getSample() const;

    // drawn distortion curve (message thread writes, audio thread reads)
    std::array<std::atomic<float>, wb::kCurvePoints> curve;
    void resetCurve();
    void curveToState();

    juce::AudioProcessorValueTreeState apvts;

    // UI feedback
    static constexpr int kScope = 2048;
    std::array<std::atomic<float>, kScope> scope;
    std::atomic<int> scopePos { 0 };
    std::atomic<float> peakMeter { 0.f };

private:
    struct PInfo { std::atomic<float>* raw = nullptr; juce::NormalisableRange<float> range; bool isFloat = false; float mod = 0.f; };
    std::array<PInfo, wb::p::kNumP> info;
    wb::Frame frame {};

    struct Lfo { float ph = 0, held = 0, target = 0, smooth = 0; wb::Rng rng { 1 };
                 float tick (int shape, float rate, int n, double sr); };
    Lfo lfo1, lfo2; wb::Rng chaosRng { 9 }, randRng { 10 };
    float chaosX = 0.4f, chaosAcc = 0.f, chaosVal = 0.f, randAcc = 0.f, randVal = 0.f, follower = 0.f;
    juce::ADSR ampEnv, modEnv;

    // midi
    std::vector<int> held; int lastNote = 36; float velocity = 1.f, modWheel = 0.f, bend = 0.f; bool gate = false;

    wb::Voice voice[2];
    std::vector<float> inL, inR, env, outL, outR;
    double sr = 44100;

    mutable juce::SpinLock sampleLock;
    std::shared_ptr<const wb::SampleData> sample;
    std::vector<std::shared_ptr<const wb::SampleData>> retired;
    std::shared_ptr<const wb::SampleData> audioSample; // audio-thread copy
    juce::String samplePath;

    // safety
    float limGain = 1.f; wb::DcBlock dcL, dcR;

    friend class WifebeaterEditor;
    void handleMidi (const juce::MidiMessage&);
    void updateFrame();
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WifebeaterProcessor)
};
