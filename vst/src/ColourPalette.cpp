#include "ColourPalette.h"

constexpr auto COLOR_SUCCESS = 0xff5cb85c;
constexpr auto COLOR_WARNING = 0xfff0ad4e;
constexpr auto COLOR_DANGER = 0xffd9534f;
constexpr auto COLOR_DANGER_LIGHT = 0xfff07a7a;
constexpr auto COLOR_DANGER_DARK = 0xff9d3838;
constexpr auto COLOR_PRIMARY = 0xff5bc0de;
constexpr auto COLOR_SECONDARY = 0xff999988;
constexpr auto COLOR_BG_DEEP = 0xff181818;
constexpr auto COLOR_BG_DARK = 0xff252525;
constexpr auto COLOR_BG_MID = 0xff353535;
constexpr auto COLOR_BG_LIGHT = 0xff454545;
constexpr auto COLOR_TEXT_PRIMARY = 0xfff5f5f5;
constexpr auto COLOR_TEXT_SECONDARY = 0xffb0b0b0;
constexpr auto COLOR_TEXT_ACCENT = 0xff0099ff;
constexpr auto COLOR_INACTIVE = 0xff4a4a4a;
constexpr auto COLOR_SELECTED = 0xff00ff80;
constexpr auto COLOR_PLAY_ARMED = 0xffcc8844;
constexpr auto COLOR_SOLO_ACTIVE = 0xffffc107;
constexpr auto COLOR_SOLO_TEXT = 0xff1a1a1a;
constexpr auto COLOR_STOP_ACTIVE = 0xffbf6030;
constexpr auto COLOR_TRACK1 = 0xff5a7a9a;
constexpr auto COLOR_TRACK2 = 0xff9a7a5a;
constexpr auto COLOR_TRACK3 = 0xff6a8a6a;
constexpr auto COLOR_TRACK4 = 0xff8a5a6a;
constexpr auto COLOR_TRACK5 = 0xff7a6a8a;
constexpr auto COLOR_TRACK6 = 0xff5a8a8a;
constexpr auto COLOR_TRACK7 = 0xff7a8a6a;
constexpr auto COLOR_TRACK8 = 0xff8a6a7a;
constexpr auto COLOR_SEQUENCER_ACCENT = 0xff6a7a6a;
constexpr auto COLOR_SEQUENCER_BEAT = 0xff6a6a7a;
constexpr auto COLOR_SEQUENCER_SUBBEAT = 0xff5a5a5a;

const juce::Colour ColourPalette::track1(COLOR_TRACK1);
const juce::Colour ColourPalette::track2(COLOR_TRACK2);
const juce::Colour ColourPalette::track3(COLOR_TRACK3);
const juce::Colour ColourPalette::track4(COLOR_TRACK4);
const juce::Colour ColourPalette::track5(COLOR_TRACK5);
const juce::Colour ColourPalette::track6(COLOR_TRACK6);
const juce::Colour ColourPalette::track7(COLOR_TRACK7);
const juce::Colour ColourPalette::track8(COLOR_TRACK8);

const juce::Colour ColourPalette::buttonPrimary(COLOR_PRIMARY);
const juce::Colour ColourPalette::buttonSecondary(COLOR_SECONDARY);
const juce::Colour ColourPalette::buttonDanger(COLOR_DANGER);
const juce::Colour ColourPalette::buttonSuccess(COLOR_SUCCESS);
const juce::Colour ColourPalette::buttonWarning(COLOR_WARNING);
const juce::Colour ColourPalette::buttonDangerLight(COLOR_DANGER_LIGHT);
const juce::Colour ColourPalette::buttonDangerDark(COLOR_DANGER_DARK);

const juce::Colour ColourPalette::backgroundDark(COLOR_BG_DARK);
const juce::Colour ColourPalette::backgroundMid(COLOR_BG_MID);
const juce::Colour ColourPalette::backgroundLight(COLOR_BG_LIGHT);
const juce::Colour ColourPalette::backgroundDeep(COLOR_BG_DEEP);

const juce::Colour ColourPalette::textPrimary(COLOR_TEXT_PRIMARY);
const juce::Colour ColourPalette::textSecondary(COLOR_TEXT_SECONDARY);
const juce::Colour ColourPalette::textDanger(COLOR_DANGER_LIGHT);
const juce::Colour ColourPalette::textSuccess(COLOR_SUCCESS);
const juce::Colour ColourPalette::textWarning(COLOR_WARNING);
const juce::Colour ColourPalette::textAccent(COLOR_TEXT_ACCENT);

const juce::Colour ColourPalette::sliderThumb(COLOR_SUCCESS);
const juce::Colour ColourPalette::sliderTrack(COLOR_INACTIVE);

const juce::Colour ColourPalette::vuPeak(COLOR_TEXT_PRIMARY);
const juce::Colour ColourPalette::vuClipping(COLOR_DANGER_LIGHT);
const juce::Colour ColourPalette::vuGreen(COLOR_SUCCESS);
const juce::Colour ColourPalette::vuOrange(COLOR_WARNING);
const juce::Colour ColourPalette::vuRed(COLOR_DANGER);

const juce::Colour ColourPalette::playActive(COLOR_SUCCESS);
const juce::Colour ColourPalette::playArmed(COLOR_PLAY_ARMED);
const juce::Colour ColourPalette::muteActive(COLOR_DANGER);
const juce::Colour ColourPalette::soloActive(COLOR_SOLO_ACTIVE);
const juce::Colour ColourPalette::soloText(COLOR_SOLO_TEXT);
const juce::Colour ColourPalette::stopActive(COLOR_STOP_ACTIVE);
const juce::Colour ColourPalette::buttonInactive(COLOR_INACTIVE);
const juce::Colour ColourPalette::trackSelected(COLOR_SELECTED);

const juce::Colour ColourPalette::sequencerAccent(COLOR_SEQUENCER_ACCENT);
const juce::Colour ColourPalette::sequencerBeat(COLOR_SEQUENCER_BEAT);
const juce::Colour ColourPalette::sequencerSubBeat(COLOR_SEQUENCER_SUBBEAT);

juce::Colour ColourPalette::getTrackColour(int trackIndex)
{
	static const std::vector<juce::Colour> trackColours = {
		track1, track2, track3, track4, track5, track6, track7, track8
	};
	return trackColours[trackIndex % trackColours.size()];
}

juce::Colour ColourPalette::withAlpha(const juce::Colour& colour, float alpha)
{
	return colour.withAlpha(alpha);
}

juce::Colour ColourPalette::darken(const juce::Colour& colour, float amount)
{
	return colour.darker(amount);
}

juce::Colour ColourPalette::lighten(const juce::Colour& colour, float amount)
{
	return colour.brighter(amount);
}