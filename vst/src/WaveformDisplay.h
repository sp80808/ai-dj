#pragma once
#include "JuceHeader.h"

class DjIaVstProcessor;

class WaveformDisplay : public juce::Component
{
public:
	WaveformDisplay(DjIaVstProcessor& processor);
	~WaveformDisplay();

	std::function<void(double, double)> onLoopPointsChanged;

	void setOriginalBpm(float bpm);
	void setSampleBpm(float bpm);
	void lockLoopPoints(bool locked);
	void setPlaybackPosition(double timeInSeconds, bool isPlaying);
	void setAudioData(const juce::AudioBuffer<float>& audioBuffer, double sampleRate);
	void setLoopPoints(double startTime, double endTime);

private:
	juce::AudioBuffer<float> audioBuffer;
	DjIaVstProcessor& audioProcessor;
	double sampleRate = 48000.0;
	std::vector<float> thumbnail;
	double loopStart = 0.0;
	double loopEnd = 4.0;
	bool loopPointsLocked = false;
	float trackBpm = 126.0f;
	float sampleBpm = 126.0f;
	float stretchRatio = 1.0f;
	bool draggingStart = false;
	bool draggingEnd = false;

	float originalBpm = 126.0f;
	float timeStretchRatio = 1.0f;

	double zoomFactor = 1.0;
	double viewStartTime = 0.0;

	double playbackPosition = 0.0;
	bool isCurrentlyPlaying = false;

	float getHostBpm() const;
	float timeToX(double time);

	void generateThumbnail();
	void feedThumbnail(int startSample, int point, int samplesPerPoint, int& retFlag);
	void drawWaveform(juce::Graphics& g);
	void setColorDependingTimeStretchRatio(juce::Colour& waveformColor) const;
	void generateBottomHalfPath(int i, float pixelsPerPoint, bool& pathStarted, juce::Path& bottomPath, int thumbnailSize);
	void generateTopHalfPath(int i, float pixelsPerPoint, bool& pathStarted, juce::Path& waveformPath, int thumbnailSize);
	void drawLoopMarkers(juce::Graphics& g);
	void drawLoopTimeLabels(juce::Graphics& g, float startX, float endX);
	void drawLoopBarLabels(juce::Graphics& g, float startX, float endX) const;
	void drawPlaybackHead(juce::Graphics& g);
	void drawBeatMarkers(juce::Graphics& g);
	void drawMeasures(float time, juce::Graphics& g, float barDuration);
	void drawBeats(juce::Graphics& g, float beatDuration, float viewEndTime, float barDuration);
	void calculateStretchRatio();

	void paint(juce::Graphics& g) override;
	void mouseDown(const juce::MouseEvent& e) override;
	void mouseDrag(const juce::MouseEvent& e) override;
	void mouseUp(const juce::MouseEvent& e) override;
	void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

	double xToTime(float x);
	double getTotalDuration() const;
	double getViewStartTime() const;
	double getViewEndTime() const;
	double getMinLoopDuration() const;
};