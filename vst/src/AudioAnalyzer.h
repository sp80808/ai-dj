#pragma once
#include "JuceHeader.h"
#include "SoundTouch.h"
#include "BPMDetect.h"

class AudioAnalyzer
{
public:
	static float detectBPM(const juce::AudioBuffer<float>& buffer, double sampleRate)
	{
		if (buffer.getNumSamples() == 0)
			return 0.0f;

		try
		{
			soundtouch::BPMDetect bpmDetect(1, (int)sampleRate);

			std::vector<float> monoData;
			monoData.reserve(buffer.getNumSamples());

			bool retFlag;
			float retVal = normalizeAudio(buffer, monoData, retFlag);
			if (retFlag) return retVal;

			chunkAnalysis(monoData, bpmDetect);

			float detectedBPM = bpmDetect.getBpm();
			return returnDetectedBPMorFallback(detectedBPM, buffer, sampleRate);
		}
		catch (const std::exception& /*e*/)
		{
			return 0.0f;
		}
	}

	static float returnDetectedBPMorFallback(float detectedBPM, const juce::AudioSampleBuffer& buffer, double sampleRate)
	{
		if (detectedBPM >= 30.0f && detectedBPM <= 300.0f)
		{
			return detectedBPM;
		}
		else
		{
			float fallbackBPM = detectBPMByOnsets(buffer, sampleRate);
			if (fallbackBPM >= 30.0f && fallbackBPM <= 300.0f)
			{
				return fallbackBPM;
			}

			return 0.0f;
		}
	}

	static void chunkAnalysis(std::vector<float>& monoData, soundtouch::BPMDetect& bpmDetect)
	{
		const int chunkSize = 4096;

		for (size_t i = 0; i < monoData.size(); i += chunkSize)
		{
			size_t remaining = std::min((size_t)chunkSize, monoData.size() - i);
			bpmDetect.inputSamples(&monoData[i], (int)remaining);
		}
	}

	static float normalizeAudio(const juce::AudioSampleBuffer& buffer, std::vector<float>& monoData, bool& retFlag)
	{
		retFlag = true;
		float maxLevel = 0.0f;
		for (int i = 0; i < buffer.getNumSamples(); ++i)
		{
			float mono = buffer.getSample(0, i);
			if (buffer.getNumChannels() > 1)
			{
				mono = (buffer.getSample(0, i) + buffer.getSample(1, i)) * 0.5f;
			}

			maxLevel = std::max(maxLevel, std::abs(mono));
			monoData.push_back(mono);
		}
		if (maxLevel < 0.001f)
		{
			return 0.0f;
		}
		float normalizeGain = 0.5f / maxLevel;
		for (auto& sample : monoData)
		{
			sample *= normalizeGain;
		}
		retFlag = false;
		return {};
	}

	static float detectBPMByOnsets(const juce::AudioBuffer<float>& buffer, double sampleRate)
	{
		if (buffer.getNumSamples() < sampleRate)
			return 0.0f;

		try
		{
			const int hopSize = 512;
			const int windowSize = 1024;
			std::vector<float> onsetStrength;

			for (int i = 0; i < buffer.getNumSamples() - windowSize; i += hopSize)
			{
				float energy = 0.0f;
				for (int j = 0; j < windowSize; ++j)
				{
					float sample = buffer.getSample(0, i + j);
					if (buffer.getNumChannels() > 1)
					{
						sample = (sample + buffer.getSample(1, i + j)) * 0.5f;
					}
					energy += sample * sample;
				}
				onsetStrength.push_back(std::sqrt(energy / windowSize));
			}
			std::vector<int> onsets;
			float threshold = 0.1f;

			for (int i = 1; i < onsetStrength.size() - 1; ++i)
			{
				if (onsetStrength[i] > threshold &&
					onsetStrength[i] > onsetStrength[i - 1] &&
					onsetStrength[i] > onsetStrength[i + 1])
				{
					onsets.push_back(i);
				}
			}

			if (onsets.size() < 4)
			{
				return 0.0f;
			}
			std::vector<float> intervals;
			for (int i = 1; i < onsets.size(); ++i)
			{
				float intervalSeconds = (onsets[i] - onsets[i - 1]) * hopSize / (float)sampleRate;
				if (intervalSeconds > 0.2f && intervalSeconds < 2.0f)
				{
					intervals.push_back(60.0f / intervalSeconds);
				}
			}

			if (intervals.empty())
			{
				return 0.0f;
			}

			std::sort(intervals.begin(), intervals.end());
			float medianBPM = intervals[intervals.size() / 2];

			if (medianBPM >= 30.0f && medianBPM <= 300.0f)
			{
				return medianBPM;
			}

			return 0.0f;
		}
		catch (const std::exception& /*e*/)
		{
			return 0.0f;
		}
	}

	static void timeStretchBuffer(juce::AudioBuffer<float>& buffer,
		double ratio, double sampleRate)
	{
		if (ratio == 1.0 || buffer.getNumSamples() == 0)
			return;

		try
		{
			soundtouch::SoundTouch soundTouch;
			soundTouch.setSampleRate((int)sampleRate);
			soundTouch.setChannels(buffer.getNumChannels());
			soundTouch.setTempoChange((ratio - 1.0) * 100.0);

			if (buffer.getNumChannels() == 1)
			{
				soundTouch.putSamples(buffer.getReadPointer(0), buffer.getNumSamples());
			}
			else
			{
				std::vector<float> interleavedInput;
				interleavedInput.reserve(buffer.getNumSamples() * 2);

				for (int i = 0; i < buffer.getNumSamples(); ++i)
				{
					interleavedInput.push_back(buffer.getSample(0, i));
					interleavedInput.push_back(buffer.getSample(1, i));
				}

				soundTouch.putSamples(interleavedInput.data(), buffer.getNumSamples());
			}

			soundTouch.flush();

			int outputSamples = soundTouch.numSamples();
			if (outputSamples > 0)
			{
				buffer.setSize(buffer.getNumChannels(), outputSamples, false, false, true);

				if (buffer.getNumChannels() == 1)
				{
					soundTouch.receiveSamples(buffer.getWritePointer(0), outputSamples);
				}
				else
				{
					std::vector<float> interleavedOutput(outputSamples * 2);
					soundTouch.receiveSamples(interleavedOutput.data(), outputSamples);

					for (int i = 0; i < outputSamples; ++i)
					{
						buffer.setSample(0, i, interleavedOutput[i * 2]);
						buffer.setSample(1, i, interleavedOutput[i * 2 + 1]);
					}
				}
			}
		}
		catch (const std::exception& e)
		{
			std::cout << "Error: " << e.what() << std::endl;
		}
	}
};