#include "JuceHeader.h"
#include "WaveformDisplay.h"
#include "PluginProcessor.h" 
#include "TrackData.h"

WaveformDisplay::WaveformDisplay(DjIaVstProcessor& processor) : audioProcessor(processor)
{
	setSize(400, 80);

	zoomFactor = 1.0;
	viewStartTime = 0.0;
	sampleRate = 48000.0;

	horizontalScrollBar = std::make_unique<juce::ScrollBar>(false);
	horizontalScrollBar->setRangeLimits(0.0, 1.0);
	horizontalScrollBar->addListener(this);

}

WaveformDisplay::~WaveformDisplay()
{
}

void WaveformDisplay::setSampleBpm(float bpm)
{
	sampleBpm = bpm;
	calculateStretchRatio();
	juce::MessageManager::callAsync([this]() {
		repaint();
		});
}

void WaveformDisplay::setOriginalBpm(float bpm)
{
	originalBpm = bpm;
}

void WaveformDisplay::setAudioData(const juce::AudioBuffer<float>& audioBuffer, double sampleRate)
{
	this->audioBuffer = audioBuffer;
	this->sampleRate = sampleRate;

	zoomFactor = 1.0;
	viewStartTime = 0.0;

	generateThumbnail();
	repaint();
}

void WaveformDisplay::setLoopPoints(double startTime, double endTime)
{
	if (!loopPointsLocked)
	{
		loopStart = startTime;
		loopEnd = endTime;
		juce::MessageManager::callAsync([this]() {
			repaint();
			});
	}
}

void WaveformDisplay::lockLoopPoints(bool locked)
{
	loopPointsLocked = locked;
	juce::MessageManager::callAsync([this]() {
		repaint();
		});
}

void WaveformDisplay::calculateStretchRatio() const
{
	if (originalBpm > 0.0f && sampleBpm > 0.0f)
	{
		const_cast<WaveformDisplay*>(this)->stretchRatio = sampleBpm / originalBpm;
	}
	else
	{
		const_cast<WaveformDisplay*>(this)->stretchRatio = 1.0f;
	}
}


void WaveformDisplay::setPlaybackPosition(double timeInSeconds, bool isPlaying)
{
	playbackPosition = timeInSeconds;
	isCurrentlyPlaying = isPlaying;

	juce::MessageManager::callAsync([this]() {
		repaint();
		});
}

void WaveformDisplay::paint(juce::Graphics& g)
{
	auto bounds = getLocalBounds();

	g.setColour(juce::Colours::black);
	g.fillRect(bounds);

	if (thumbnail.empty())
	{
		g.setColour(juce::Colours::grey);
		g.setFont(12.0f);
		g.drawText("No audio data", bounds.reduced(5).removeFromTop(20), juce::Justification::centred);

		g.setColour(juce::Colours::lightgrey);
		g.setFont(10.0f);
		g.drawText("Ctrl+Wheel: Zoom | Wheel: Scroll | Right-click: Lock/Unlock | Ctrl+Click: Drag and Drop in DAW",
			bounds.reduced(5).removeFromBottom(15), juce::Justification::centred);
		return;
	}

	g.setColour(juce::Colours::lightblue);

	drawWaveform(g);
	drawLoopMarkers(g);
	drawBeatMarkers(g);
	drawPlaybackHead(g);
	drawStretchMarkers(g);

	drawVisibleBarLabels(g);

	if (zoomFactor > 1.0)
	{
		g.setColour(juce::Colours::yellow);
		g.setFont(10.0f);
		g.drawText("Zoom: " + juce::String(zoomFactor, 1) + "x", 5, getHeight() - 20, 60, 15, juce::Justification::left);
	}

	if (loopPointsLocked)
	{
		g.setColour(juce::Colours::red);
		g.setFont(10.0f);
		g.drawText("LOCKED", getWidth() - 60, getHeight() - 20, 55, 15, juce::Justification::right);
	}
}

void WaveformDisplay::drawStretchMarkers(juce::Graphics& g)
{
	for (int i = 0; i < stretchMarkers.size(); ++i)
	{
		const auto& marker = stretchMarkers[i];
		float markerX = timeToX(marker.currentTime);

		juce::Colour markerColour = marker.isSelected ?
			juce::Colours::yellow : juce::Colours::cyan;

		if (marker.isMultiSelected)
			markerColour = juce::Colours::magenta;
		else if (i == selectedMarkerId)
			markerColour = juce::Colours::orange;
		else if (marker.isSelected)
			markerColour = juce::Colours::yellow;
		else
			markerColour = juce::Colours::cyan;

		g.setColour(markerColour);
		g.drawLine(markerX, 0, markerX, getHeight(), 2.0f);

		juce::Path arrow;
		arrow.addTriangle(markerX - 6, 0, markerX + 6, 0, markerX, 12);
		g.fillPath(arrow);

		g.setColour(juce::Colours::white);
		g.setFont(9.0f);
		g.drawText(juce::String(marker.id), markerX - 10, 15, 20, 12,
			juce::Justification::centred);
	}
}

void WaveformDisplay::mouseDown(const juce::MouseEvent& e)
{
	if (e.mods.isRightButtonDown())
	{
		int markerIndex = getMarkerAtPosition(e.x);
		if (markerIndex >= 0)
		{
			showMarkerContextMenu(markerIndex, e.getPosition());
			return;
		}
		loopPointsLocked = !loopPointsLocked;
		repaint();
		return;
	}

	if (e.mods.isCtrlDown())
	{
		int markerIndex = getMarkerAtPosition(e.x);
		if (markerIndex >= 0)
		{
			stretchMarkers[markerIndex].isMultiSelected = !stretchMarkers[markerIndex].isMultiSelected;
			repaint();
			return;
		}
		return;
	}

	if (loopPointsLocked)
		return;

	clearMultiSelection();

	float startX = timeToX(loopStart);
	float endX = timeToX(loopEnd);
	float tolerance = 15.0f;

	if (std::abs(e.x - startX) < tolerance)
	{
		draggingStart = true;
		return;
	}
	else if (std::abs(e.x - endX) < tolerance)
	{
		draggingEnd = true;
		return;
	}

	selectedMarkerId = getMarkerAtPosition(e.x);
	if (selectedMarkerId >= 0)
	{
		isDraggingMarker = true;
		stretchMarkers[selectedMarkerId].isSelected = true;
		repaint();
		return;
	}

	createMarkerAtPosition(e.x);
}

void WaveformDisplay::clearMultiSelection()
{
	for (auto& marker : stretchMarkers)
	{
		marker.isMultiSelected = false;
	}
}

int WaveformDisplay::getMultiSelectedCount()
{
	int count = 0;
	for (const auto& marker : stretchMarkers)
	{
		if (marker.isMultiSelected) count++;
	}
	return count;
}

void WaveformDisplay::showMarkerContextMenu(int markerIndex, juce::Point<int> position)
{
	if (markerIndex < 0 || markerIndex >= stretchMarkers.size()) return;

	float markerX = timeToX(stretchMarkers[markerIndex].currentTime);
	auto screenPos = localPointToGlobal(juce::Point<int>((int)markerX, 20));

	juce::PopupMenu menu;

	int multiSelectedCount = getMultiSelectedCount();
	bool hasMultiSelection = multiSelectedCount > 0;

	if (hasMultiSelection)
	{
		menu.addItem(1, "Delete " + juce::String(multiSelectedCount) + " Selected Markers");
		menu.addItem(2, "Clear Selection");
		menu.addSeparator();
		menu.addItem(3, "Snap Selected to Grid");
	}
	else
	{
		menu.addItem(1, "Delete Marker");
		menu.addSeparator();
	}

	menu.addItem(10, "Snap to Grid: " + juce::String(snapToGrid ? "ON" : "OFF"));

	juce::PopupMenu snapMenu;
	snapMenu.addItem(20, "1/4", true, snapResolution == WHOLE_BEAT);
	snapMenu.addItem(21, "1/8", true, snapResolution == HALF_BEAT);
	snapMenu.addItem(22, "1/16", true, snapResolution == QUARTER_BEAT);
	snapMenu.addItem(23, "1/32", true, snapResolution == EIGHTH_BEAT);

	menu.addSubMenu("Snap Resolution", snapMenu);

	menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this)
		.withTargetScreenArea(juce::Rectangle<int>(screenPos.x - 10, screenPos.y, 20, 10)),
		[this, markerIndex, hasMultiSelection](int result)
		{
			handleMenuResult(result, markerIndex, hasMultiSelection);
		});
}

void WaveformDisplay::deleteMarker(int markerIndex)
{
	if (markerIndex >= 0 && markerIndex < stretchMarkers.size())
	{
		stretchMarkers.erase(stretchMarkers.begin() + markerIndex);
		selectedMarkerId = -1;
		repaint();
		if (onMarkersChanged && !isUpdatingMarkers)
			onMarkersChanged();
	}
}

void WaveformDisplay::handleMenuResult(int result, int markerIndex, bool hasMultiSelection)
{
	switch (result)
	{
	case 1:
		if (hasMultiSelection)
		{
			deleteSelectedMarkers();
		}
		else
		{
			deleteMarker(markerIndex);
		}
		break;

	case 2:
		clearMultiSelection();
		repaint();
		break;

	case 3:
		snapSelectedMarkersToGrid();
		break;

	case 10:
		snapToGrid = !snapToGrid;
		if (onMarkersChanged) onMarkersChanged();
		repaint();
		break;

	case 20:
		snapResolution = WHOLE_BEAT;
		if (onMarkersChanged) onMarkersChanged();
		repaint();
		break;

	case 21:
		snapResolution = HALF_BEAT;
		if (onMarkersChanged) onMarkersChanged();
		repaint();
		break;

	case 22:
		snapResolution = QUARTER_BEAT;
		if (onMarkersChanged) onMarkersChanged();
		repaint();
		break;

	case 23:
		snapResolution = EIGHTH_BEAT;
		if (onMarkersChanged) onMarkersChanged();
		repaint();
		break;
	}
}

void WaveformDisplay::deleteSelectedMarkers()
{
	for (int i = stretchMarkers.size() - 1; i >= 0; --i)
	{
		if (stretchMarkers[i].isMultiSelected)
		{
			stretchMarkers.erase(stretchMarkers.begin() + i);
		}
	}

	selectedMarkerId = -1;
	repaint();

	if (onMarkersChanged)
		onMarkersChanged();

}

void WaveformDisplay::snapSelectedMarkersToGrid()
{
	for (auto& marker : stretchMarkers)
	{
		if (marker.isMultiSelected)
		{
			marker.currentTime = snapTimeToGrid(marker.currentTime);
		}
	}

	repaint();

	if (onMarkersChanged)
		onMarkersChanged();

}

int WaveformDisplay::getMarkerAtPosition(float x)
{
	float tolerance = 15.0f;

	for (int i = 0; i < stretchMarkers.size(); ++i)
	{
		float markerX = timeToX(stretchMarkers[i].currentTime);
		if (std::abs(x - markerX) < tolerance)
		{
			return i;
		}
	}
	return -1;
}

void WaveformDisplay::createMarkerAtPosition(float x)
{
	double time = xToTime(x);

	StretchMarker newMarker;
	newMarker.originalTime = time;
	newMarker.currentTime = time;
	newMarker.isSelected = true;
	newMarker.id = nextMarkerId++;

	stretchMarkers.push_back(newMarker);
	selectedMarkerId = stretchMarkers.size() - 1;

	repaint();

	if (onMarkersChanged)
		onMarkersChanged();
}

void WaveformDisplay::mouseDrag(const juce::MouseEvent& e)
{
	if (!draggingStart && !draggingEnd && currentAudioFile.exists() && e.mods.isCtrlDown())
	{
		auto distanceFromStart = e.getDistanceFromDragStart();
		if (distanceFromStart > 10 && !isDraggingAudio)
		{
			isDraggingAudio = true;
			juce::StringArray files;
			files.add(currentAudioFile.getFullPathName());
			DBG("Starting external drag with: " << currentAudioFile.getFullPathName());
			bool success = performExternalDragDropOfFiles(files, false);
			DBG("Drag result: " << (success ? "SUCCESS" : "FAILED"));
			return;
		}
	}

	if (isDraggingMarker && selectedMarkerId >= 0 && selectedMarkerId < stretchMarkers.size())
	{
		double rawTime = xToTime(e.x);

		if (snapToGrid)
		{
			double snappedTime = snapTimeToGrid(rawTime);
			float expectedX = timeToX(snappedTime);
			DBG("Raw time: " << rawTime);
			DBG("Snapped time: " << snappedTime);
			DBG("Mouse X: " << e.x);
			DBG("Expected X for snapped time: " << expectedX);

			stretchMarkers[selectedMarkerId].currentTime = snappedTime;
		}
		stretchMarkers[selectedMarkerId].isSelected = true;
		calculateStretchRatios();
		repaint();
		if (onMarkersChanged && !isUpdatingMarkers)
			onMarkersChanged();
		return;
	}

	if (loopPointsLocked || trackBpm <= 0.0f)
		return;

	if (!e.mods.isCtrlDown())
	{
		if (draggingStart)
		{
			double newStart = xToTime(e.x);
			loopStart = juce::jlimit(getViewStartTime(), loopEnd, newStart);
			repaint();
			if (onLoopPointsChanged)
			{
				onLoopPointsChanged(loopStart, loopEnd);
			}
		}
		else if (draggingEnd)
		{
			double newEnd = xToTime(e.x);
			loopEnd = juce::jlimit(loopStart, getViewEndTime(), newEnd);
			repaint();
			if (onLoopPointsChanged)
			{
				onLoopPointsChanged(loopStart, loopEnd);
			}
		}
	}
}

double WaveformDisplay::snapTimeToGrid(double time)
{
	float hostBpm = getHostBpm();
	if (hostBpm <= 0.0f) return time;

	int numerator = audioProcessor.getTimeSignatureNumerator();
	double beatDuration = (60.0 / hostBpm) * stretchRatio;
	double snapInterval;

	switch (snapResolution)
	{
	case WHOLE_BEAT:    snapInterval = beatDuration; break;
	case HALF_BEAT:     snapInterval = beatDuration * 0.5; break;
	case QUARTER_BEAT:  snapInterval = beatDuration * 0.25; break;
	case EIGHTH_BEAT:   snapInterval = beatDuration * 0.125; break;
	default:            snapInterval = beatDuration; break;
	}

	double snapped = round(time / snapInterval) * snapInterval;

	DBG("Snap: " << time << " -> " << snapped << " (to nearest " << snapInterval << "s grid)");
	return snapped;
}

void WaveformDisplay::calculateStretchRatios()
{
	for (const auto& marker : stretchMarkers)
	{
		double ratio = marker.currentTime / marker.originalTime;
		DBG("Marker " << marker.id << " ratio: " << ratio);
	}
}

double WaveformDisplay::getMinLoopDuration() const
{
	if (trackBpm <= 0.0f)
		return 1.0;

	int numerator = audioProcessor.getTimeSignatureNumerator();

	double beatDuration = 60.0 / trackBpm;
	return beatDuration * numerator;
}

void WaveformDisplay::setAudioFile(const juce::File& file)
{
	currentAudioFile = file;
}


void WaveformDisplay::mouseUp(const juce::MouseEvent& e)
{
	bool wasDraggingMarker = isDraggingMarker;
	draggingStart = false;
	draggingEnd = false;
	isDraggingAudio = false;
	isDraggingMarker = false;
	selectedMarkerId = -1;
	if (wasDraggingMarker && onMarkersChanged)
	{
		onMarkersChanged();
	}
}

void WaveformDisplay::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
	if (e.mods.isCtrlDown())
	{
		if (getTotalDuration() <= 0.0)
			return;

		double totalDuration = getTotalDuration();
		double currentViewDuration = totalDuration / zoomFactor;
		double mouseRatio = (double)e.x / (double)getWidth();
		double mouseTime = viewStartTime + (mouseRatio * currentViewDuration);
		double oldZoomFactor = zoomFactor;

		if (wheel.deltaY > 0)
		{
			zoomFactor = juce::jlimit(1.0, 10.0, zoomFactor * 1.2);
		}
		else
		{
			zoomFactor = juce::jlimit(1.0, 10.0, zoomFactor / 1.2);
		}

		if (zoomFactor == oldZoomFactor)
			return;

		double newViewDuration = totalDuration / zoomFactor;
		viewStartTime = mouseTime - (mouseRatio * newViewDuration);

		setViewStartTime(viewStartTime);

		updateScrollBarVisibility();
		generateThumbnail();
		repaint();
	}
	else if (zoomFactor > 1.0)
	{
		double viewDuration = getTotalDuration() / zoomFactor;
		double scrollAmount = wheel.deltaY * viewDuration * 0.1;

		setViewStartTime(viewStartTime - scrollAmount);

		updateScrollBar();
		generateThumbnail();
		repaint();
	}
}

void WaveformDisplay::updateScrollBarVisibility()
{
	bool shouldShow = (zoomFactor > 1.0);

	if (shouldShow && !scrollBarVisible)
	{
		addAndMakeVisible(*horizontalScrollBar);
		horizontalScrollBar->setBounds(0, getHeight() - 8, getWidth(), 12);
		scrollBarVisible = true;
		updateScrollBar();
	}
	else if (!shouldShow && scrollBarVisible)
	{
		removeChildComponent(horizontalScrollBar.get());
		scrollBarVisible = false;
	}
	else if (shouldShow)
	{
		horizontalScrollBar->setBounds(0, getHeight() - 8, getWidth(), 12);
		updateScrollBar();
	}
}

void WaveformDisplay::updateScrollBar()
{
	if (!scrollBarVisible) return;

	double totalDuration = getTotalDuration();

	if (totalDuration <= 0.0)
	{
		horizontalScrollBar->setCurrentRange(0.0, 1.0);
		return;
	}

	double viewProportionOfTotal = 1.0 / zoomFactor;
	double currentRangeStart = viewStartTime / totalDuration;

	horizontalScrollBar->setCurrentRange(currentRangeStart, viewProportionOfTotal);
}

void WaveformDisplay::scrollBarMoved(juce::ScrollBar* scrollBarThatHasMoved, double newRangeStart)
{
	if (scrollBarThatHasMoved == horizontalScrollBar.get())
	{
		double totalDuration = getTotalDuration();
		double newViewStartTime = newRangeStart * totalDuration;

		setViewStartTime(newViewStartTime);
		generateThumbnail();
		repaint();
	}
}

void WaveformDisplay::setViewStartTime(double newViewStartTime)
{
	double totalDuration = getTotalDuration();
	if (totalDuration <= 0.0)
	{
		viewStartTime = 0.0;
		return;
	}

	double viewDuration = totalDuration / zoomFactor;
	double maxViewStartTime = totalDuration - viewDuration;

	if (maxViewStartTime < 0.0)
	{
		maxViewStartTime = 0.0;
	}

	viewStartTime = juce::jlimit(0.0, maxViewStartTime, newViewStartTime);
}

float WaveformDisplay::getHostBpm() const
{
	return audioProcessor.getHostBpm();
}

void WaveformDisplay::generateThumbnail()
{
	thumbnail.clear();

	if (audioBuffer.getNumSamples() == 0)
		return;

	double totalDuration = getTotalDuration();
	double viewDuration = totalDuration / zoomFactor;
	double viewEndTime = juce::jlimit(viewStartTime, totalDuration, viewStartTime + viewDuration);

	int startSample = (int)(viewStartTime * sampleRate);
	int endSample = (int)(viewEndTime * sampleRate);
	startSample = juce::jlimit(0, audioBuffer.getNumSamples() - 1, startSample);
	endSample = juce::jlimit(startSample + 1, audioBuffer.getNumSamples(), endSample);

	int viewSamples = endSample - startSample;

	if (viewSamples <= 0)
		return;

	int targetPoints = getWidth() * 2;
	targetPoints = juce::jlimit(getWidth(), getWidth() * 10, targetPoints);

	int samplesPerPoint = juce::jmax(1, viewSamples / targetPoints);

	for (int point = 0; point < targetPoints; ++point)
	{
		int retFlag;
		feedThumbnail(startSample, point, samplesPerPoint, retFlag);
		if (retFlag == 2)
			break;
	}
}

void WaveformDisplay::feedThumbnail(int startSample, int point, int samplesPerPoint, int& retFlag)
{
	retFlag = 1;
	int sampleStart = startSample + (point * samplesPerPoint);
	int sampleEnd = std::min(sampleStart + samplesPerPoint, audioBuffer.getNumSamples());
	if (sampleStart >= audioBuffer.getNumSamples())
	{
		{
			retFlag = 2;
			return;
		};
	}
	float rmsSum = 0.0f;
	float peak = 0.0f;
	int count = 0;

	for (int sample = sampleStart; sample < sampleEnd; ++sample)
	{

		if (sample >= audioBuffer.getNumSamples())
		{
			break;
		}
		for (int ch = 0; ch < audioBuffer.getNumChannels(); ++ch)
		{
			float val = audioBuffer.getSample(ch, sample);
			rmsSum += val * val;
			peak = std::max(peak, std::abs(val));
			count++;
		}
	}

	float rms = count > 0 ? std::sqrt(rmsSum / count) : 0.0f;
	float finalValue = (rms * 0.7f) + (peak * 0.3f);

	thumbnail.push_back(finalValue);
}

void WaveformDisplay::drawWaveform(juce::Graphics& g)
{
	if (thumbnail.empty())
		return;

	juce::Colour waveformColor;
	setColorDependingTimeStretchRatio(waveformColor);

	g.setColour(waveformColor);

	juce::Path waveformPath;
	bool pathStarted = false;

	int thumbnailSize = thumbnail.size();
	float pixelsPerPoint = (float)getWidth() / thumbnailSize;

	for (int i = 0; i < thumbnailSize; ++i)
	{
		generateTopHalfPath(i, pixelsPerPoint, pathStarted, waveformPath, thumbnailSize);
	}

	g.strokePath(waveformPath, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved));

	juce::Path bottomPath;
	pathStarted = false;

	for (int i = 0; i < thumbnailSize; ++i)
	{
		generateBottomHalfPath(i, pixelsPerPoint, pathStarted, bottomPath, thumbnailSize);
	}

	g.strokePath(bottomPath, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved));

	g.setColour(juce::Colours::lightblue.withAlpha(0.3f));
	g.drawLine(0, getHeight() * 0.5f, getWidth(), getHeight() * 0.5f, 0.5f);
}

void WaveformDisplay::setColorDependingTimeStretchRatio(juce::Colour& waveformColor) const
{
	float deviation = std::abs(stretchRatio - 1.0f);

	if (deviation < 0.005f)
	{
		waveformColor = juce::Colour(0xff90EE90);
	}
	else if (deviation < 0.08f)
	{
		float normalizedDev = juce::jlimit(0.0f, 1.0f, (deviation - 0.005f) / 0.075f);

		if (stretchRatio > 1.0f)
		{
			if (normalizedDev < 0.5f)
			{
				float factor = normalizedDev / 0.5f;
				juce::Colour baseGreen(0xff98D982);
				juce::Colour beige(0xffD4AF8C);
				waveformColor = baseGreen.interpolatedWith(beige, factor);
			}
			else
			{
				float factor = (normalizedDev - 0.5f) / 0.5f;
				juce::Colour beige(0xffD4AF8C);
				juce::Colour orangePale(0xffCC8866);
				waveformColor = beige.interpolatedWith(orangePale, factor);
			}
		}
		else
		{
			if (normalizedDev < 0.5f)
			{
				float factor = normalizedDev / 0.5f;
				juce::Colour baseGreen(0xff98D982);
				juce::Colour blueGrey(0xff7B9CB0);
				waveformColor = baseGreen.interpolatedWith(blueGrey, factor);
			}
			else
			{
				float factor = (normalizedDev - 0.5f) / 0.5f;
				juce::Colour blueGrey(0xff7B9CB0);
				juce::Colour bluePale(0xff6B8CAE);
				waveformColor = blueGrey.interpolatedWith(bluePale, factor);
			}
		}
	}
	else
	{
		waveformColor = stretchRatio > 1.0f ?
			juce::Colour(0xffAA6644) :
			juce::Colour(0xff556B8D);
	}
}

void WaveformDisplay::generateBottomHalfPath(int i, float pixelsPerPoint, bool& pathStarted, juce::Path& bottomPath, int thumbnailSize)
{
	float x = i * pixelsPerPoint;
	float amplitude = thumbnail[i];

	float centerY = getHeight() * 0.5f;
	float waveHeight = amplitude * centerY * 0.8f;
	float bottomY = centerY + waveHeight;

	if (!pathStarted)
	{
		bottomPath.startNewSubPath(x, centerY);
		pathStarted = true;
	}

	if (i > 0 && i < thumbnailSize - 1)
	{
		float prevX = (i - 1) * pixelsPerPoint;
		float nextX = (i + 1) * pixelsPerPoint;
		float controlX = (prevX + nextX) * 0.5f;
		bottomPath.quadraticTo(controlX, bottomY, x, bottomY);
	}
	else
	{
		bottomPath.lineTo(x, bottomY);
	}
}

void WaveformDisplay::generateTopHalfPath(int i, float pixelsPerPoint, bool& pathStarted, juce::Path& waveformPath, int thumbnailSize)
{
	float x = i * pixelsPerPoint;
	float amplitude = thumbnail[i];

	float centerY = getHeight() * 0.5f;
	float waveHeight = amplitude * centerY * 0.8f;

	float topY = centerY - waveHeight;
	float bottomY = centerY + waveHeight;

	if (!pathStarted)
	{
		waveformPath.startNewSubPath(x, centerY);
		pathStarted = true;
	}
	if (i > 0 && i < thumbnailSize - 1)
	{
		float prevX = (i - 1) * pixelsPerPoint;
		float nextX = (i + 1) * pixelsPerPoint;

		float controlX = (prevX + nextX) * 0.5f;
		waveformPath.quadraticTo(controlX, topY, x, topY);
	}
	else
	{
		waveformPath.lineTo(x, topY);
	}
}

void WaveformDisplay::drawLoopMarkers(juce::Graphics& g)
{
	float startX = timeToX(loopStart);
	float endX = timeToX(loopEnd);

	juce::Colour loopColour = loopPointsLocked ? juce::Colours::orange : juce::Colours::purple;
	g.setColour(loopColour.withAlpha(0.3f));
	g.fillRect(startX, 0.0f, endX - startX, (float)getHeight());

	float lineWidth = loopPointsLocked ? 3.0f : 2.0f;
	g.setColour(loopColour);
	g.drawLine(startX, 0, startX, getHeight(), lineWidth);
	g.drawLine(endX, 0, endX, getHeight(), lineWidth);

	if (trackBpm > 0.0f)
	{
		drawLoopBarLabels(g, startX, endX);
	}
	else
	{
		drawLoopTimeLabels(g, startX, endX);
	}
}

void WaveformDisplay::drawLoopTimeLabels(juce::Graphics& g, float startX, float endX)
{
	g.setColour(juce::Colours::white);
	g.setFont(10.0f);
	g.drawText(juce::String(loopStart, 2) + "s", startX + 2, 2, 50, 15,
		juce::Justification::left);
	g.drawText(juce::String(loopEnd, 2) + "s", endX - 50, 2, 48, 15,
		juce::Justification::right);
}

void WaveformDisplay::drawLoopBarLabels(juce::Graphics& g, float startX, float endX) const
{
	g.setColour(juce::Colours::white);
	g.setFont(10.0f);
	g.drawText(juce::String(loopStart, 2) + "s", startX + 5, getHeight() - 30, 50, 15,
		juce::Justification::left);
	g.drawText(juce::String(loopEnd, 2) + "s", endX - 55, getHeight() - 30, 48, 15,
		juce::Justification::right);
}


void WaveformDisplay::drawVisibleBarLabels(juce::Graphics& g)
{
	if (trackBpm <= 0.0f)
		return;

	int numerator = audioProcessor.getTimeSignatureNumerator();

	float effectiveBpm = trackBpm;
	if (stretchRatio > 0.0f && stretchRatio != 1.0f)
	{
		effectiveBpm = trackBpm * stretchRatio;
	}

	double beatDuration = 60.0 / effectiveBpm;
	double barDuration = beatDuration * numerator;

	double viewStart = getViewStartTime();
	double viewEnd = getViewEndTime();

	int leftBar = (int)(viewStart / barDuration) + 1;
	int rightBar = (int)(viewEnd / barDuration) + 1;

	if (fmod(viewEnd, barDuration) < 0.01)
	{
		rightBar--;
	}

	g.setColour(juce::Colours::lightgrey);
	g.setFont(12.0f);
	g.drawText("Bar " + juce::String(leftBar),
		5, 2, 60, 15, juce::Justification::left);
	g.drawText("Bar " + juce::String(rightBar),
		getWidth() - 65, 2, 60, 15, juce::Justification::right);

	int visibleBars = rightBar - leftBar + 1;
	if (visibleBars > 1)
	{
		g.setFont(10.0f);
		g.drawText("(" + juce::String(visibleBars) + " bars visible)",
			getWidth() / 2 - 40, 2, 80, 15, juce::Justification::centred);
	}
}

void WaveformDisplay::drawPlaybackHead(juce::Graphics& g)
{
	if (isCurrentlyPlaying && playbackPosition >= 0.0)
	{
		float headX = timeToX(playbackPosition);

		double viewStart = getViewStartTime();
		double viewEnd = getViewEndTime();

		if (playbackPosition >= viewStart && playbackPosition <= viewEnd && headX >= 0 && headX <= getWidth())
		{
			g.setColour(juce::Colours::red);
			g.drawLine(headX, 0, headX, getHeight(), 4.0f);

			juce::Path triangle;
			triangle.addTriangle(headX - 8, 0, headX + 8, 0, headX, 16);
			g.setColour(juce::Colours::yellow);
			g.fillPath(triangle);

			triangle.clear();
			triangle.addTriangle(headX - 8, getHeight(), headX + 8, getHeight(), headX, getHeight() - 16);
			g.fillPath(triangle);

			g.setColour(juce::Colours::white);
			g.setFont(14.0f);
			g.drawText(juce::String(playbackPosition, 2) + "s", headX - 40, getHeight() / 2 - 10, 80, 20,
				juce::Justification::centred);
		}
	}
}

float WaveformDisplay::timeToX(double time)
{
	double totalDuration = getTotalDuration();
	if (totalDuration <= 0.0)
		return 0.0f;

	double viewDuration = totalDuration / zoomFactor;
	double relativeTime = time - viewStartTime;

	return juce::jmap(relativeTime, 0.0, viewDuration, 0.0, (double)getWidth());
}

void WaveformDisplay::drawBeatMarkers(juce::Graphics& g)
{
	if (thumbnail.empty()) return;

	float hostBpm = getHostBpm();
	if (hostBpm <= 0.0f) return;

	int numerator = audioProcessor.getTimeSignatureNumerator();
	int denominator = audioProcessor.getTimeSignatureDenominator();

	double totalDuration = getTotalDuration();
	double viewDuration = totalDuration / zoomFactor;
	double viewEndTime = juce::jlimit(viewStartTime, totalDuration, viewStartTime + viewDuration);

	float beatDuration = 60.0f / hostBpm * stretchRatio;
	float barDuration = beatDuration * numerator;

	g.setColour(juce::Colours::white.withAlpha(0.9f));
	double firstBarTime = floor(viewStartTime / barDuration) * barDuration;
	for (double time = firstBarTime; time <= viewEndTime; time += barDuration)
	{
		drawMeasures(time, g, barDuration, viewDuration);
	}

	g.setColour(juce::Colours::white.withAlpha(0.6f));
	drawBeats(g, beatDuration, viewEndTime, barDuration, viewDuration);

	g.setColour(juce::Colours::white.withAlpha(0.3f));
	drawSubdivisions(g, beatDuration * 0.5f, viewEndTime, barDuration, viewDuration);

	g.setColour(juce::Colours::white.withAlpha(0.2f));
	drawSubdivisions(g, beatDuration * 0.25f, viewEndTime, barDuration, viewDuration);

}

void WaveformDisplay::drawSubdivisions(juce::Graphics& g, float subdivisionDuration, double viewEndTime, float barDuration, double viewDuration)
{
	double firstSubdivisionTime = floor(viewStartTime / subdivisionDuration) * subdivisionDuration;

	for (double time = firstSubdivisionTime; time <= viewEndTime; time += subdivisionDuration)
	{
		bool isOnBeat = (fmod(time, subdivisionDuration * 2.0) < 0.01);
		bool isOnBar = (fmod(time, barDuration) < 0.01);

		if (!isOnBeat && !isOnBar && time >= viewStartTime)
		{
			double relativeTime = time - viewStartTime;
			float x = (relativeTime / viewDuration) * getWidth();

			if (x >= 0 && x <= getWidth())
			{
				g.drawLine(x, getHeight() * 0.2f, x, getHeight() * 0.8f, 0.5f);
			}
		}
	}
}

void WaveformDisplay::drawMeasures(float time, juce::Graphics& g, float barDuration, double viewDuration)
{
	if (time >= viewStartTime)
	{
		double relativeTime = time - viewStartTime;
		float x = (relativeTime / viewDuration) * getWidth();
		if (x >= 0 && x <= getWidth())
		{
			g.drawLine(x, 0, x, getHeight(), 2.0f);
			int measureNumber = (int)(time / barDuration) + 1;
			g.setFont(10.0f);
			g.drawText(juce::String(measureNumber), x + 2, 2, 30, 15,
				juce::Justification::left);
		}
	}
}

void WaveformDisplay::drawBeats(juce::Graphics& g, float beatDuration, float viewEndTime, float barDuration, double viewDuration)
{
	g.setColour(juce::Colours::white.withAlpha(0.4f));
	double firstBeatTime = floor(viewStartTime / beatDuration) * beatDuration;
	for (double time = firstBeatTime; time <= viewEndTime; time += beatDuration)
	{
		if (fmod(time, barDuration) > 0.01)
		{
			if (time >= viewStartTime)
			{
				double relativeTime = time - viewStartTime;
				float x = (relativeTime / viewDuration) * getWidth();
				if (x >= 0 && x <= getWidth())
				{
					g.drawLine(x, 0, x, getHeight(), 1.0f);
				}
			}
		}
	}
}

double WaveformDisplay::xToTime(float x)
{
	double totalDuration = getTotalDuration();
	if (totalDuration <= 0.0)
	{
		return 0.0;
	}

	double viewDuration = totalDuration / zoomFactor;
	if (viewDuration <= 0.0)
	{
		return 0.0;
	}

	double relativeTime = juce::jmap((double)x, 0.0, (double)getWidth(), 0.0, viewDuration);
	double result = viewStartTime + relativeTime;

	return juce::jlimit(0.0, totalDuration, result);
}

double WaveformDisplay::getTotalDuration() const
{
	if (audioBuffer.getNumSamples() == 0 || sampleRate <= 0)
		return 0.0;

	return audioBuffer.getNumSamples() / sampleRate;
}

double WaveformDisplay::getViewStartTime() const
{
	return viewStartTime;
}

double WaveformDisplay::getViewEndTime() const
{
	return juce::jlimit(viewStartTime, getTotalDuration(),
		viewStartTime + (getTotalDuration() / zoomFactor));
}

void WaveformDisplay::loadMarkersFromTrack(TrackData* track)
{
	if (!track) return;

	isUpdatingMarkers = true;
	int currentSelectedId = (selectedMarkerId >= 0 && selectedMarkerId < stretchMarkers.size())
		? stretchMarkers[selectedMarkerId].id : -1;

	stretchMarkers.clear();
	selectedMarkerId = -1;
	for (const auto& saved : track->stretchMarkers)
	{
		StretchMarker marker;
		marker.originalTime = saved.originalTime;
		marker.currentTime = saved.currentTime;
		marker.id = saved.id;
		marker.isMultiSelected = saved.isMultiSelected;
		marker.isSelected = (saved.id == currentSelectedId);
		stretchMarkers.push_back(marker);

		if (saved.id == currentSelectedId)
		{
			selectedMarkerId = stretchMarkers.size() - 1;
		}
	}

	snapToGrid = track->snapToGrid;
	snapResolution = (SnapResolution)track->snapResolution;
	nextMarkerId = track->nextMarkerId;
	isUpdatingMarkers = false;
	repaint();
}

void WaveformDisplay::saveMarkersToTrack(TrackData* track)
{
	if (!track) return;

	track->stretchMarkers.clear();
	for (const auto& marker : stretchMarkers)
	{
		SavedStretchMarker saved;
		saved.originalTime = marker.originalTime;
		saved.currentTime = marker.currentTime;
		saved.isMultiSelected = marker.isMultiSelected;
		saved.id = marker.id;
		track->stretchMarkers.push_back(saved);
	}

	track->snapToGrid = snapToGrid;
	track->snapResolution = snapResolution;
	track->nextMarkerId = nextMarkerId;
}
