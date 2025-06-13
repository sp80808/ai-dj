#pragma once
#include "JuceHeader.h"

class DjIaVstProcessor;

struct StretchMarker
{
	double originalTime;
	double currentTime;
	bool isSelected;
	int id;
	bool isMultiSelected = false;
};
struct TrackData;
struct SavedStretchMarker;
class WaveformDisplay : public juce::Component, public juce::ScrollBar::Listener, public juce::DragAndDropContainer
{
public:
	WaveformDisplay(DjIaVstProcessor& processor);
	~WaveformDisplay();

	std::function<void(double, double)> onLoopPointsChanged;
	std::function<void()> onMarkersChanged;

	bool isDraggingMarker = false;
	bool WaveformDisplay::hasMultiSelection() const
	{
		for (const auto& marker : stretchMarkers)
		{
			if (marker.isMultiSelected)
				return true;
		}
		return false;
	}

	void setOriginalBpm(float bpm);
	void setSampleBpm(float bpm);
	void lockLoopPoints(bool locked);
	void setPlaybackPosition(double timeInSeconds, bool isPlaying);
	void setAudioData(const juce::AudioBuffer<float>& audioBuffer, double sampleRate);
	void setLoopPoints(double startTime, double endTime);
	void setAudioFile(const juce::File& file);
	void loadMarkersFromTrack(TrackData* track);
	void saveMarkersToTrack(TrackData* track);

private:
	juce::AudioBuffer<float> audioBuffer;
	juce::File currentAudioFile;
	juce::Point<int> dragStartPosition;

	std::vector<StretchMarker> stretchMarkers;
	int nextMarkerId = 0;
	int selectedMarkerId = -1;

	bool isUpdatingMarkers = false;
	bool snapToGrid = true;
	enum SnapResolution { WHOLE_BEAT, HALF_BEAT, QUARTER_BEAT, EIGHTH_BEAT };
	SnapResolution snapResolution = QUARTER_BEAT;

	std::unique_ptr<juce::ScrollBar> horizontalScrollBar;

	DjIaVstProcessor& audioProcessor;

	std::vector<float> thumbnail;

	double loopStart = 0.0;
	double loopEnd = 4.0;
	double sampleRate = 48000.0;
	double zoomFactor = 1.0;
	double viewStartTime = 0.0;
	double playbackPosition = 0.0;

	bool scrollBarVisible = false;
	bool loopPointsLocked = false;
	bool draggingStart = false;
	bool draggingEnd = false;
	bool isDraggingAudio = false;
	bool isCurrentlyPlaying = false;

	float trackBpm = 126.0f;
	float sampleBpm = 126.0f;
	float stretchRatio = 1.0f;
	float originalBpm = 126.0f;
	float timeStretchRatio = 1.0f;

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
	void drawMeasures(float time, juce::Graphics& g, float barDuration, double viewDuration);
	void drawBeats(juce::Graphics& g, float beatDuration, float viewEndTime, float barDuration, double viewDuration);
	void calculateStretchRatio() const;
	void updateScrollBarVisibility();
	void updateScrollBar();
	void drawVisibleBarLabels(juce::Graphics& g);
	void setViewStartTime(double newViewStartTime);
	void createMarkerAtPosition(float x);
	void showMarkerContextMenu(int markerIndex, juce::Point<int> position);
	void deleteMarker(int markerIndex);
	void drawStretchMarkers(juce::Graphics& g);
	void calculateStretchRatios();
	void clearMultiSelection();
	void handleMenuResult(int result, int markerIndex, bool hasMultiSelection);
	void deleteSelectedMarkers();
	void snapSelectedMarkersToGrid();

	void paint(juce::Graphics& g) override;
	void mouseDown(const juce::MouseEvent& e) override;
	void mouseDrag(const juce::MouseEvent& e) override;
	void mouseUp(const juce::MouseEvent& e) override;
	void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
	void scrollBarMoved(juce::ScrollBar* scrollBarThatHasMoved, double newRangeStart) override;

	double xToTime(float x);
	double getTotalDuration() const;
	double getViewStartTime() const;
	double getViewEndTime() const;
	double getMinLoopDuration() const;

	double snapTimeToGrid(double time);

	int getMarkerAtPosition(float x);
	int getMultiSelectedCount();
};