#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// Single source of truth for parameter order. Each X(name) is both the APVTS
// parameter ID string and an index into the per-block parameter Frame.
#define WB_IDS(X) \
  X(gain) X(safe) X(drone) X(root) X(inLevel) X(width) \
  X(eA) X(eD) X(eS) X(eR) \
  X(nLevel) X(nType) X(nColor) \
  X(cLevel) X(cType) X(cRate) X(cAmt) \
  X(oLevel) X(oWave) X(oTune) X(oDetune) X(oRatio) X(oFM) X(oSync) X(oRing) \
  X(rLevel) X(rDecay) X(rDamp) X(rExcite) \
  X(sLevel) X(sMode) X(sStart) X(sLen) X(sPitch) X(sKey) X(sRev) \
  X(gSize) X(gDens) X(gScatPos) X(gScatPitch) X(gRev) \
  X(xRate) X(xSlice) X(xChance) X(xRev) X(xStop) \
  X(wPos) X(wWarp) X(wFM) X(wRing) \
  X(spMode) X(spAmt) X(spX) X(spY) X(spHold) \
  X(d1On) X(d1Type) X(d1Drive) X(d1X) X(d1Tone) X(d1Mix) \
  X(d2On) X(d2Type) X(d2Drive) X(d2X) X(d2Tone) X(d2Mix) \
  X(d3On) X(d3Type) X(d3Drive) X(d3X) X(d3Tone) X(d3Mix) \
  X(fbAmt) X(fbTime) X(fbTone) \
  X(fType) X(fCut) X(fRes) \
  X(l1Rate) X(l1Shape) X(l2Rate) X(l2Shape) X(meA) X(meD) X(mcRate) X(mrRate) \
  X(m1Src) X(m1Dst) X(m1Amt) X(m2Src) X(m2Dst) X(m2Amt) \
  X(m3Src) X(m3Dst) X(m3Amt) X(m4Src) X(m4Dst) X(m4Amt) \
  X(m5Src) X(m5Dst) X(m5Amt) X(m6Src) X(m6Dst) X(m6Amt) \
  X(m7Src) X(m7Dst) X(m7Amt) X(m8Src) X(m8Dst) X(m8Amt)

#define WB_ENUM(n) n,
#define WB_STR(n) #n,

namespace wb
{
namespace p { enum : int { WB_IDS(WB_ENUM) kNumP }; }
inline const char* const kIdStr[] = { WB_IDS(WB_STR) };

constexpr int kNumModSlots = 8;
constexpr int kCurvePoints = 64;

// Parameters the mod matrix may target (float params only).
inline constexpr int kModDest[] = {
  p::nLevel, p::nColor, p::cLevel, p::cRate, p::cAmt,
  p::oLevel, p::oTune, p::oDetune, p::oRatio, p::oFM, p::oRing,
  p::rLevel, p::rDecay, p::rDamp,
  p::sLevel, p::sStart, p::sLen, p::sPitch,
  p::gSize, p::gDens, p::gScatPos, p::gScatPitch,
  p::xRate, p::xSlice, p::xChance,
  p::wPos, p::wWarp, p::wFM,
  p::spAmt, p::spX, p::spY,
  p::d1Drive, p::d1X, p::d1Tone, p::d1Mix,
  p::d2Drive, p::d2X, p::d2Tone, p::d2Mix,
  p::d3Drive, p::d3X, p::d3Tone, p::d3Mix,
  p::fbAmt, p::fbTime, p::fbTone,
  p::fCut, p::fRes, p::gain, p::width };
constexpr int kNumModDest = (int) (sizeof (kModDest) / sizeof (kModDest[0]));

inline const juce::StringArray kModSrcNames { "None", "LFO 1", "LFO 2", "Amp Env", "Mod Env",
  "Chaos", "Random", "Follower", "Velocity", "Mod Wheel", "Key" };

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
}
