#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;
using namespace wb;

static constexpr int kSub = 32;

float WifebeaterProcessor::Lfo::tick (int shape, float rate, int n, double sr)
{
    const float prev = ph;
    ph += rate * (float) n / (float) sr;
    const bool wrapped = ph >= 1.f; ph -= std::floor (ph);
    switch (shape)
    {
        case 0: return std::sin (kTwoPi * ph);
        case 1: return 4.f * std::abs (ph - 0.5f) - 1.f;
        case 2: return 2.f * ph - 1.f;
        case 3: return ph < 0.5f ? 1.f : -1.f;
        case 4: if (wrapped || prev == ph) held = rng.bi(); return held;
        default:
            if (wrapped) target = rng.bi();
            smooth += (target - smooth) * std::min (1.f, rate * (float) n / (float) sr * 4.f);
            return smooth;
    }
}

WifebeaterProcessor::WifebeaterProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", AudioChannelSet::stereo(), false)
                          .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "WIFEBEATER", createLayout())
{
    for (int i = 0; i < p::kNumP; ++i)
    {
        auto* prm = apvts.getParameter (kIdStr[i]);
        jassert (prm != nullptr);
        info[(size_t) i].raw = apvts.getRawParameterValue (kIdStr[i]);
        info[(size_t) i].range = prm->getNormalisableRange();
        info[(size_t) i].isFloat = dynamic_cast<AudioParameterFloat*> (prm) != nullptr;
    }
    resetCurve();
    for (auto& s : scope) s.store (0.f);
    useDefaultSample();
}

bool WifebeaterProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    if (l.getMainOutputChannelSet() != AudioChannelSet::stereo()) return false;
    const auto in = l.getMainInputChannelSet();
    return in.isDisabled() || in == AudioChannelSet::stereo() || in == AudioChannelSet::mono();
}

void WifebeaterProcessor::prepareToPlay (double s, int block)
{
    sr = s;
    voice[0].prepare (s, 0, kSub); voice[1].prepare (s, 1, kSub);
    ampEnv.setSampleRate (s); modEnv.setSampleRate (s);
    inL.assign (kSub, 0.f); inR.assign (kSub, 0.f); env.assign (kSub, 0.f); outL.assign (kSub, 0.f); outR.assign (kSub, 0.f);
    juce::ignoreUnused (block);
    limGain = 1.f; follower = 0.f;
}

//==============================================================================
void WifebeaterProcessor::handleMidi (const MidiMessage& m)
{
    if (m.isNoteOn())
    {
        const int n = m.getNoteNumber();
        held.erase (std::remove (held.begin(), held.end(), n), held.end());
        held.push_back (n); lastNote = n; velocity = m.getFloatVelocity();
        ampEnv.noteOn(); modEnv.noteOn(); gate = true;
    }
    else if (m.isNoteOff())
    {
        held.erase (std::remove (held.begin(), held.end(), m.getNoteNumber()), held.end());
        if (held.empty()) { gate = false; ampEnv.noteOff(); modEnv.noteOff(); }
        else lastNote = held.back();
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    { held.clear(); gate = false; ampEnv.noteOff(); modEnv.noteOff(); }
    else if (m.isPitchWheel()) bend = (float) (m.getPitchWheelValue() - 8192) / 8192.f * 2.f;
    else if (m.isControllerOfType (1)) modWheel = (float) m.getControllerValue() / 127.f;
}

void WifebeaterProcessor::updateFrame()
{
    for (size_t i = 0; i < info.size(); ++i)
    {
        auto& pi = info[i];
        float v = pi.raw->load (std::memory_order_relaxed);
        if (pi.isFloat && pi.mod != 0.f)
            v = pi.range.convertFrom0to1 (jlimit (0.f, 1.f, pi.range.convertTo0to1 (v) + pi.mod));
        frame[i] = v;
    }
}

void WifebeaterProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    const int total = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels();
    const bool haveIn = numIn > 0;

    // snapshot the input before we overwrite the buffer
    AudioBuffer<float> inCopy;
    if (haveIn) inCopy.makeCopyOf (buffer, true);

    {
        const SpinLock::ScopedTryLockType tl (sampleLock);
        if (tl.isLocked()) audioSample = sample;
    }
    static const SampleData emptySample;
    const SampleData& sd = audioSample ? *audioSample : emptySample;

    auto midiIt = midi.cbegin();
    float* outPtr[2] = { buffer.getWritePointer (0), buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr };

    for (int pos = 0; pos < total; pos += kSub)
    {
        const int n = jmin (kSub, total - pos);
        while (midiIt != midi.cend() && (*midiIt).samplePosition < pos + n) { handleMidi ((*midiIt).getMessage()); ++midiIt; }

        // --- envelopes
        updateFrame();
        ampEnv.setParameters ({ frame[p::eA] > 0 ? frame[p::eA] : 0.002f, frame[p::eD], frame[p::eS], frame[p::eR] });
        const bool drone = frame[p::drone] > 0.5f;
        modEnv.setParameters ({ frame[p::meA], frame[p::meD], 0.f, 0.05f });
        for (int i = 0; i < n; ++i) env[(size_t) i] = drone ? 1.f : ampEnv.getNextSample();
        const float envNow = env[0];
        const float modEnvNow = modEnv.getNextSample();

        // --- mod sources (block rate)
        const float l1 = lfo1.tick (I (frame, p::l1Shape), frame[p::l1Rate], n, sr);
        const float l2 = lfo2.tick (I (frame, p::l2Shape), frame[p::l2Rate], n, sr);
        chaosAcc += frame[p::mcRate] * (float) n / (float) sr;
        while (chaosAcc >= 1.f) { chaosAcc -= 1.f; chaosX = 3.9f * chaosX * (1.f - chaosX); if (! (chaosX > 1e-4f && chaosX < 0.9999f)) chaosX = chaosRng.uni(); chaosVal = 2.f * chaosX - 1.f; }
        randAcc += frame[p::mrRate] * (float) n / (float) sr;
        while (randAcc >= 1.f) { randAcc -= 1.f; randVal = randRng.bi(); }
        const float keyRel = jlimit (-1.f, 1.f, ((float) lastNote - frame[p::root]) / 24.f);
        const float src[11] = { 0.f, l1, l2, envNow, modEnvNow, chaosVal, randVal, follower, velocity, modWheel, keyRel };

        for (auto& pi : info) pi.mod = 0.f;
        for (int s = 0; s < kNumModSlots; ++s)
        {
            const int b = p::m1Src + s * 3;
            const int si = I (frame, b), di = I (frame, b + 1);
            if (si <= 0 || di <= 0 || di > kNumModDest) continue;
            info[(size_t) kModDest[di - 1]].mod += src[jlimit (0, 10, si)] * frame[(size_t) b + 2];
        }
        updateFrame();

        // --- pitch
        const float noteFreq = 440.f * std::exp2 (((float) lastNote + bend - 69.f) / 12.f);
        const float rootFreq = 440.f * std::exp2 ((frame[p::root] - 69.f) / 12.f);
        voice[0].update (frame, noteFreq, rootFreq, sd);
        voice[1].update (frame, noteFreq, rootFreq, sd);

        const float* iL = nullptr; const float* iR = nullptr;
        if (haveIn)
        {
            iL = inCopy.getReadPointer (0) + pos;
            iR = (inCopy.getNumChannels() > 1 ? inCopy.getReadPointer (1) : inCopy.getReadPointer (0)) + pos;
        }
        voice[0].process (iL, env.data(), outL.data(), n, sd, curve.data());
        voice[1].process (iR, env.data(), outR.data(), n, sd, curve.data());

        // --- master: width, gain, safety
        const float gain = Decibels::decibelsToGain (frame[p::gain], -59.f);
        const float width = frame[p::width];
        const bool safe = frame[p::safe] > 0.5f;
        constexpr float ceilingLin = 0.944f; // -0.5 dBFS
        float pk = 0.f, fol = follower;
        for (int i = 0; i < n; ++i)
        {
            const float mid = 0.5f * (outL[(size_t) i] + outR[(size_t) i]);
            const float side = 0.5f * (outL[(size_t) i] - outR[(size_t) i]) * width;
            float l = (mid + side) * gain, r = (mid - side) * gain;
            if (safe)
            {
                l = dcL.tick (l); r = dcR.tick (r);
                const float peak = jmax (std::abs (l), std::abs (r));
                const float need = peak > ceilingLin ? ceilingLin / peak : 1.f;
                limGain = need < limGain ? need : limGain + (1.f - limGain) * 0.0008f;
                l = jlimit (-ceilingLin, ceilingLin, l * limGain);
                r = jlimit (-ceilingLin, ceilingLin, r * limGain);
            }
            else
            {
                l = jlimit (-16.f, 16.f, finiteOr (l)); r = jlimit (-16.f, 16.f, finiteOr (r));
            }
            if (outPtr[0]) outPtr[0][pos + i] = l;
            if (outPtr[1]) outPtr[1][pos + i] = r;
            const float m = 0.5f * (l + r);
            pk = jmax (pk, std::abs (l), std::abs (r));
            fol = jmax (std::abs (m), fol * 0.9995f);
            const int sp = scopePos.load (std::memory_order_relaxed);
            scope[(size_t) (sp % kScope)].store (m, std::memory_order_relaxed);
            scopePos.store (sp + 1, std::memory_order_relaxed);
        }
        follower = fol;
        peakMeter.store (jmax (pk, peakMeter.load() * 0.98f), std::memory_order_relaxed);
    }
    midi.clear();
}

//==============================================================================
void WifebeaterProcessor::resetCurve()
{
    for (int i = 0; i < kCurvePoints; ++i)
        curve[(size_t) i].store ((float) i / (float) (kCurvePoints - 1) * 2.f - 1.f);
}

void WifebeaterProcessor::curveToState()
{
    StringArray s;
    for (auto& c : curve) s.add (String (c.load(), 3));
    apvts.state.setProperty ("curve", s.joinIntoString (","), nullptr);
}

std::shared_ptr<const SampleData> WifebeaterProcessor::getSample() const
{
    const SpinLock::ScopedLockType l (sampleLock);
    return sample;
}

void WifebeaterProcessor::useDefaultSample()
{
    auto sd = std::make_shared<SampleData>();
    sd->sr = 44100.0; sd->name = "built-in grit";
    const int n = 44100 * 4; sd->data.resize ((size_t) n);
    Rng rng (2024); float lp = 0.f, ph = 0.f;
    for (int i = 0; i < n; ++i)
    {
        const float t = (float) i / 44100.f;
        const float w = rng.bi();
        lp += 0.05f * (w - lp);
        const float burst = std::pow (std::max (0.f, std::sin (kTwoPi * t * 1.5f)), 6.f);
        ph += (60.f + 2000.f * std::pow (std::fmod (t * 0.5f, 1.f), 3.f)) / 44100.f;
        float s = lp * 3.f * burst + std::sin (kTwoPi * ph) * 0.4f * (1.f - burst);
        if (rng.uni() < 0.0004f) s += rng.bi();
        sd->data[(size_t) i] = jlimit (-1.f, 1.f, s);
    }
    {
        const SpinLock::ScopedLockType l (sampleLock);
        if (sample) retired.push_back (sample);
        sample = sd;
    }
    samplePath = {};
    apvts.state.removeProperty ("samplePath", nullptr);
}

bool WifebeaterProcessor::loadSampleFile (const File& f)
{
    AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<AudioFormatReader> reader (fm.createReaderFor (f));
    if (reader == nullptr || reader->lengthInSamples < 16) return false;
    const int len = (int) std::min<int64> (reader->lengthInSamples, (int64) (reader->sampleRate * 60.0));
    AudioBuffer<float> tmp ((int) reader->numChannels, len);
    reader->read (&tmp, 0, len, 0, true, true);
    auto sd = std::make_shared<SampleData>();
    sd->sr = reader->sampleRate; sd->name = f.getFileName(); sd->data.resize ((size_t) len);
    for (int i = 0; i < len; ++i)
    {
        float s = 0.f; for (int c = 0; c < tmp.getNumChannels(); ++c) s += tmp.getSample (c, i);
        sd->data[(size_t) i] = s / (float) tmp.getNumChannels();
    }
    {
        const SpinLock::ScopedLockType l (sampleLock);
        if (sample) retired.push_back (sample);
        sample = sd;
        if (retired.size() > 4) retired.erase (retired.begin());
    }
    samplePath = f.getFullPathName();
    apvts.state.setProperty ("samplePath", samplePath, nullptr);
    return true;
}

void WifebeaterProcessor::getStateInformation (MemoryBlock& dest)
{
    curveToState();
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void WifebeaterProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (ValueTree::fromXml (*xml));
            const auto c = StringArray::fromTokens (apvts.state.getProperty ("curve").toString(), ",", "");
            if (c.size() == kCurvePoints)
                for (int i = 0; i < kCurvePoints; ++i) curve[(size_t) i].store (c[i].getFloatValue());
            const File f (apvts.state.getProperty ("samplePath").toString());
            if (f.existsAsFile()) loadSampleFile (f);
        }
}

AudioProcessorEditor* WifebeaterProcessor::createEditor() { return new WifebeaterEditor (*this); }

#ifndef WB_TEST_BUILD
AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new WifebeaterProcessor(); }
#endif
