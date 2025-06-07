#include "JuceHeader.h"
#include "WaveformDisplay.h"
#include "PluginProcessor.h" 


WaveformDisplay::WaveformDisplay(DjIaVstProcessor& processor) : audioProcessor(processor)
{
	setSize(400, 80);

	zoomFactor = 1.0;
	viewStartTime = 0.0;
	sampleRate = 48000.0;
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

void WaveformDisplay::calculateStretchRatio()
{
	if (originalBpm > 0.0f && sampleBpm > 0.0f)
	{
		stretchRatio = sampleBpm / originalBpm;
	}
	else
	{
		stretchRatio = 1.0f;
	}
}

void WaveformDisplay::setPlaybackPosition(double timeInSeconds, bool isPlaying)
{
	double adjustedPosition = timeInSeconds;
	calculateStretchRatio();
	if (stretchRatio > 0.0f && stretchRatio != 1.0f)
	{
		adjustedPosition = timeInSeconds / stretchRatio;
	}

	playbackPosition = adjustedPosition;
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
		g.drawText("Ctrl+Wheel: Zoom | Wheel: Scroll | Right-click: Lock/Unlock",
			bounds.reduced(5).removeFromBottom(15), juce::Justification::centred);
		return;
	}

	g.setColour(juce::Colours::lightblue);

	drawWaveform(g);
	drawLoopMarkers(g);
	drawBeatMarkers(g);
	drawPlaybackHead(g);

	if (zoomFactor > 1.0)
	{
		g.setColour(juce::Colours::yellow);
		g.setFont(10.0f);
		g.drawText("Zoom: " + juce::String(zoomFactor, 1) + "x", 5, getHeight() - 15, 60, 15, juce::Justification::left);
	}

	if (loopPointsLocked)
	{
		g.setColour(juce::Colours::red);
		g.setFont(10.0f);
		g.drawText("LOCKED", getWidth() - 60, getHeight() - 15, 55, 15, juce::Justification::right);
	}
}

void WaveformDisplay::mouseDown(const juce::MouseEvent& e)
{
	if (e.mods.isRightButtonDown())
	{
		loopPointsLocked = !loopPointsLocked;
		repaint();
		return;
	}

	if (loopPointsLocked)
		return;

	float startX = timeToX(loopStart);
	float endX = timeToX(loopEnd);

	float tolerance = 15.0f;

	if (std::abs(e.x - startX) < tolerance)
	{
		draggingStart = true;
	}
	else if (std::abs(e.x - endX) < tolerance)
	{
		draggingEnd = true;
	}
	else
	{
		double clickTime = xToTime(e.x);
		double distToStart = std::abs(clickTime - loopStart);
		double distToEnd = std::abs(clickTime - loopEnd);

		if (distToStart < distToEnd)
		{
			loopStart = clickTime;
			if (loopStart >= loopEnd)
			{
				loopStart = loopEnd;
			}
		}
		else
		{
			loopEnd = clickTime;
			if (loopEnd <= loopStart)
			{
				loopEnd = loopStart;
			}
		}

		if (onLoopPointsChanged)
		{
			onLoopPointsChanged(loopStart, loopEnd);
		}
		repaint();
	}
}

void WaveformDisplay::mouseDrag(const juce::MouseEvent& e)
{
	if (loopPointsLocked || trackBpm <= 0.0f)
		return;

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

double WaveformDisplay::getMinLoopDuration() const
{
	if (trackBpm <= 0.0f)
		return 1.0;

	double beatDuration = 60.0 / trackBpm;
	return beatDuration * 4.0;
}

void WaveformDisplay::mouseUp(const juce::MouseEvent& e)
{
	draggingStart = false;
	draggingEnd = false;
}

void WaveformDisplay::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
	if (e.mods.isCtrlDown())
	{
		if (getTotalDuration() <= 0.0)
		{
			return;
		}
		double mouseTime = xToTime(e.x);
		mouseTime = juce::jlimit(0.0, getTotalDuration(), mouseTime);
		double oldZoomFactor = zoomFactor;

		if (wheel.deltaY > 0)
		{
			zoomFactor = juce::jlimit(1.0, 10.0, zoomFactor * 1.2);
		}
		else
		{
			zoomFactor = juce::jlimit(1.0, 10.0, zoomFactor / 1.2);
		}
		double newViewDuration = getTotalDuration() / zoomFactor;
		if (newViewDuration <= 0.0)
		{
			zoomFactor = oldZoomFactor;
			return;
		}

		viewStartTime = mouseTime - (e.x / (double)getWidth()) * newViewDuration;
		viewStartTime = juce::jlimit(0.0, getTotalDuration() - newViewDuration, viewStartTime);

		generateThumbnail();
		repaint();
	}
	else if (zoomFactor > 1.0)
	{
		double viewDuration = getTotalDuration() / zoomFactor;
		double scrollAmount = wheel.deltaY * viewDuration * 0.1;

		viewStartTime = juce::jlimit(0.0, getTotalDuration() - viewDuration,
			viewStartTime - scrollAmount);

		generateThumbnail();
		repaint();
	}
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

	double viewDuration = getTotalDuration() / zoomFactor;
	double viewEndTime = juce::jlimit(viewStartTime, getTotalDuration(),
		viewStartTime + viewDuration);

	int startSample = (int)(viewStartTime * sampleRate);
	int endSample = (int)(viewEndTime * sampleRate);
	startSample = juce::jlimit(0, audioBuffer.getNumSamples() - 1, startSample);
	endSample = juce::jlimit(startSample + 1, audioBuffer.getNumSamples(), endSample);

	int viewSamples = endSample - startSample;

	if (viewSamples <= 0)
		return;

	int targetPoints = getWidth() * 4;
	int samplesPerPoint = std::max(1, viewSamples / targetPoints);
	targetPoints = juce::jlimit(10, 10000, getWidth() * 4);
	samplesPerPoint = juce::jlimit(1, viewSamples, viewSamples / targetPoints);

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
	double beatDuration = 60.0 / trackBpm;
	double barDuration = beatDuration * 4.0;

	int startBar = (int)(loopStart / barDuration) + 1;
	int endBar = (int)(loopEnd / barDuration);
	int totalBars = endBar - startBar + 1;

	g.setColour(juce::Colours::white);
	g.setFont(10.0f);

	g.drawText("Bar " + juce::String(startBar), startX + 2, 2, 50, 15,
		juce::Justification::left);

	g.drawText("Bar " + juce::String(endBar) + " (" + juce::String(totalBars) + " bars)",
		endX - 80, 2, 78, 15, juce::Justification::right);
}

void WaveformDisplay::drawPlaybackHead(juce::Graphics& g)
{
	if (isCurrentlyPlaying && playbackPosition >= 0.0)
	{
		double displayPosition = playbackPosition;
		if (stretchRatio > 0.0f && stretchRatio != 1.0f)
		{
			displayPosition = playbackPosition * stretchRatio;
		}
		float headX = timeToX(displayPosition);

		if (headX >= 0 && headX <= getWidth())
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
	if (viewDuration <= 0.0)
		return 0.0f;

	double relativeTime = time - viewStartTime;
	float result = juce::jmap(relativeTime, 0.0, viewDuration, 0.0, (double)getWidth());

	return juce::jlimit(0.0f, (float)getWidth(), result);
}

void WaveformDisplay::drawBeatMarkers(juce::Graphics& g)
{
	if (thumbnail.empty())
		return;

	float hostBpm = getHostBpm();
	if (hostBpm <= 0.0f)
		return;
	float totalDuration = getTotalDuration();
	float viewDuration = totalDuration / zoomFactor;
	float viewEndTime = getViewEndTime();

	float beatDuration = 60.0f / hostBpm * stretchRatio;
	float barDuration = beatDuration * 4.0f;

	g.setColour(juce::Colours::white.withAlpha(0.8f));
	for (float time = 0.0f; time <= viewEndTime; time += barDuration)
	{
		drawMeasures(time, g, barDuration);
	}

	if (zoomFactor > 2.0)
	{
		drawBeats(g, beatDuration, viewEndTime, barDuration);
	}
}

void WaveformDisplay::drawMeasures(float time, juce::Graphics& g, float barDuration)
{
	if (time >= viewStartTime)
	{
		float x = timeToX(time);
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

void WaveformDisplay::drawBeats(juce::Graphics& g, float beatDuration, float viewEndTime, float barDuration)
{
	g.setColour(juce::Colours::white.withAlpha(0.4f));
	for (float time = beatDuration; time <= viewEndTime; time += beatDuration)
	{
		if (fmod(time, barDuration) < 0.01f)
			continue;

		if (time >= viewStartTime)
		{
			float x = timeToX(time);
			if (x >= 0 && x <= getWidth())
			{
				g.drawLine(x, 0, x, getHeight(), 1.0f);
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

	double originalDuration = audioBuffer.getNumSamples() / sampleRate;

	return originalDuration / stretchRatio;
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
