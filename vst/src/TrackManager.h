#pragma once
#include "JuceHeader.h"
#include "TrackData.h"

class TrackManager
{
public:
	TrackManager() = default;

	juce::String createTrack(const juce::String &name = "Track")
	{
		juce::ScopedLock lock(tracksLock);

		auto track = std::make_unique<TrackData>();
		track->trackName = name + " " + juce::String(tracks.size() + 1);
		track->bpmOffset = 0.0;
		track->midiNote = 60 + tracks.size();
		juce::String trackId = track->trackId;
		std::string stdId = trackId.toStdString();
		track->slotIndex = findFreeSlot();
		if (track->slotIndex != -1)
		{
			usedSlots[track->slotIndex] = true;
		}
		tracks[stdId] = std::move(track);
		trackOrder.push_back(stdId);

		return trackId;
	}

	void removeTrack(const juce::String &trackId)
	{
		juce::ScopedLock lock(tracksLock);
		std::string stdId = trackId.toStdString();
		if (auto *track = getTrack(trackId))
		{
			if (track->slotIndex != -1)
			{
				usedSlots[track->slotIndex] = false;
			}
		}
		tracks.erase(stdId);
		trackOrder.erase(std::remove(trackOrder.begin(), trackOrder.end(), stdId), trackOrder.end());
	}

	void reorderTracks(const juce::String &fromTrackId, const juce::String &toTrackId)
	{
		juce::ScopedLock lock(tracksLock);

		std::string fromStdId = fromTrackId.toStdString();
		std::string toStdId = toTrackId.toStdString();

		auto fromIt = std::find(trackOrder.begin(), trackOrder.end(), fromStdId);
		auto toIt = std::find(trackOrder.begin(), trackOrder.end(), toStdId);

		if (fromIt == trackOrder.end() || toIt == trackOrder.end())
			return;

		std::string movedId = *fromIt;
		trackOrder.erase(fromIt);

		toIt = std::find(trackOrder.begin(), trackOrder.end(), toStdId);
		trackOrder.insert(toIt, movedId);
	}

	TrackData *getTrack(const juce::String &trackId)
	{
		juce::ScopedLock lock(tracksLock);
		auto it = tracks.find(trackId.toStdString());
		return (it != tracks.end()) ? it->second.get() : nullptr;
	}

	std::vector<juce::String> getAllTrackIds() const
	{
		juce::ScopedLock lock(tracksLock);
		std::vector<juce::String> ids;
		for (const auto &stdId : trackOrder)
		{
			if (tracks.count(stdId))
			{
				ids.push_back(juce::String(stdId));
			}
		}
		return ids;
	}
	void renderAllTracks(juce::AudioBuffer<float> &outputBuffer,
						 std::vector<juce::AudioBuffer<float>> &individualOutputs,
						 double hostSampleRate)
	{
		const int numSamples = outputBuffer.getNumSamples();
		bool anyTrackSolo = false;

		{
			juce::ScopedLock lock(tracksLock);
			for (const auto &pair : tracks)
			{
				if (pair.second->isSolo.load())
				{
					anyTrackSolo = true;
					break;
				}
			}
		}

		outputBuffer.clear();
		for (auto &buffer : individualOutputs)
		{
			buffer.clear();
		}

		juce::ScopedLock lock(tracksLock);

		int trackIndex = 0;
		for (const auto &pair : tracks)
		{
			if (trackIndex >= individualOutputs.size())
				break;

			auto *track = pair.second.get();

			if (track->isEnabled.load() && track->numSamples > 0)
			{
				juce::AudioBuffer<float> tempMixBuffer(outputBuffer.getNumChannels(), numSamples);
				juce::AudioBuffer<float> tempIndividualBuffer(2, numSamples);
				tempMixBuffer.clear();
				tempIndividualBuffer.clear();

				renderSingleTrack(*track, tempMixBuffer, tempIndividualBuffer,
								  numSamples, hostSampleRate);

				bool shouldHearTrack = !track->isMuted.load() &&
									   (!anyTrackSolo || track->isSolo.load());

				if (shouldHearTrack)
				{
					for (int ch = 0; ch < outputBuffer.getNumChannels(); ++ch)
					{
						outputBuffer.addFrom(ch, 0, tempMixBuffer, ch, 0, numSamples);
					}
				}

				for (int ch = 0; ch < std::min(2, individualOutputs[trackIndex].getNumChannels()); ++ch)
				{
					individualOutputs[trackIndex].copyFrom(ch, 0, tempIndividualBuffer, ch, 0, numSamples);

					if (!shouldHearTrack)
					{
						individualOutputs[trackIndex].applyGain(ch, 0, numSamples, 0.0f);
					}
				}
			}

			trackIndex++;
		}
	}

	juce::ValueTree saveState() const
	{
		juce::ValueTree state("TrackManager");

		juce::ScopedLock lock(tracksLock);
		for (const auto &pair : tracks)
		{
			auto trackState = juce::ValueTree("Track");
			auto *track = pair.second.get();

			trackState.setProperty("id", track->trackId, nullptr);
			trackState.setProperty("name", track->trackName, nullptr);
			trackState.setProperty("slotIndex", track->slotIndex, nullptr);
			trackState.setProperty("prompt", track->prompt, nullptr);
			trackState.setProperty("style", track->style, nullptr);
			trackState.setProperty("stems", track->stems, nullptr);
			trackState.setProperty("bpm", track->bpm, nullptr);
			trackState.setProperty("originalBpm", track->originalBpm, nullptr);
			trackState.setProperty("timeStretchMode", track->timeStretchMode, nullptr);
			trackState.setProperty("bpmOffset", track->bpmOffset, nullptr);
			trackState.setProperty("midiNote", track->midiNote, nullptr);
			trackState.setProperty("loopStart", track->loopStart, nullptr);
			trackState.setProperty("loopEnd", track->loopEnd, nullptr);
			trackState.setProperty("volume", track->volume.load(), nullptr);
			trackState.setProperty("pan", track->pan.load(), nullptr);
			trackState.setProperty("muted", track->isMuted.load(), nullptr);
			trackState.setProperty("solo", track->isSolo.load(), nullptr);
			trackState.setProperty("enabled", track->isEnabled.load(), nullptr);
			trackState.setProperty("fineOffset", track->fineOffset, nullptr);
			trackState.setProperty("timeStretchRatio", track->timeStretchRatio, nullptr);
			trackState.setProperty("stagingOriginalBpm", track->stagingOriginalBpm, nullptr);
			trackState.setProperty("showWaveform", track->showWaveform, nullptr);
			trackState.setProperty("isPlaying", track->isPlaying.load(), nullptr);
			trackState.setProperty("isArmed", track->isArmed.load(), nullptr);
			trackState.setProperty("isArmedToStop", track->isArmedToStop.load(), nullptr);

			if (track->numSamples > 0)
			{
				juce::MemoryBlock audioData;
				juce::MemoryOutputStream stream(audioData, false);

				stream.writeDouble(track->sampleRate);
				stream.writeInt(track->audioBuffer.getNumChannels());
				stream.writeInt(track->numSamples);

				for (int ch = 0; ch < track->audioBuffer.getNumChannels(); ++ch)
				{
					stream.write(track->audioBuffer.getReadPointer(ch),
								 track->numSamples * sizeof(float));
				}

				trackState.setProperty("audioData", audioData.toBase64Encoding(), nullptr);
			}

			state.appendChild(trackState, nullptr);
		}

		return state;
	}

	void loadState(const juce::ValueTree &state)
	{
		juce::ScopedLock lock(tracksLock);
		tracks.clear();
		trackOrder.clear();
		usedSlots.fill(false);
		for (int i = 0; i < state.getNumChildren(); ++i)
		{
			auto trackState = state.getChild(i);
			if (!trackState.hasType("Track"))
			{
				continue;
			}

			auto track = std::make_unique<TrackData>();

			track->trackId = trackState.getProperty("id", juce::Uuid().toString());
			track->trackName = trackState.getProperty("name", "Track");
			track->prompt = trackState.getProperty("prompt", "");
			track->slotIndex = trackState.getProperty("slotIndex", -1);
			track->style = trackState.getProperty("style", "");
			track->stems = trackState.getProperty("stems", "");
			track->bpm = trackState.getProperty("bpm", 126.0f);
			track->originalBpm = trackState.getProperty("originalBpm", 126.0f);
			track->timeStretchMode = trackState.getProperty("timeStretchMode", 4);
			track->bpmOffset = trackState.getProperty("bpmOffset", 0.0);
			track->midiNote = trackState.getProperty("midiNote", 60);
			track->loopStart = trackState.getProperty("loopStart", 0.0);
			track->loopEnd = trackState.getProperty("loopEnd", 4.0);
			track->volume = trackState.getProperty("volume", 0.8f);
			track->pan = trackState.getProperty("pan", 0.0f);
			track->isEnabled = trackState.getProperty("enabled", true);
			track->fineOffset = trackState.getProperty("fineOffset", 0.0f);
			track->timeStretchRatio = trackState.getProperty("timeStretchRatio", 1.0);
			track->stagingOriginalBpm = trackState.getProperty("stagingOriginalBpm", 126.0f);
			track->showWaveform = trackState.getProperty("showWaveform", false);
			track->isMuted = trackState.getProperty("muted", false);
			track->isSolo = trackState.getProperty("solo", false);
			track->isPlaying = trackState.getProperty("isplaying", false);
			track->isArmed = trackState.getProperty("isArmed", false);
			track->isArmedToStop = trackState.getProperty("isArmedToStop", false);

			juce::String audioDataBase64 = trackState.getProperty("audioData", "");
			if (audioDataBase64.isNotEmpty())
			{
				juce::MemoryBlock audioData;
				if (audioData.fromBase64Encoding(audioDataBase64))
				{
					juce::MemoryInputStream stream(audioData, false);
					track->sampleRate = stream.readDouble();
					int numChannels = stream.readInt();
					track->numSamples = stream.readInt();

					if (track->numSamples > 0 && numChannels > 0)
					{
						track->audioBuffer.setSize(numChannels, track->numSamples);

						for (int ch = 0; ch < numChannels; ++ch)
						{
							stream.read(track->audioBuffer.getWritePointer(ch),
										track->numSamples * sizeof(float));
						}
					}
				}
			}
			if (track->slotIndex >= 0 && track->slotIndex < 8)
			{
				usedSlots[track->slotIndex] = true;
			}
			std::string stdId = track->trackId.toStdString();
			tracks[stdId] = std::move(track);
			trackOrder.push_back(stdId);
		}
	}

private:
	mutable juce::CriticalSection tracksLock;
	std::unordered_map<std::string, std::unique_ptr<TrackData>> tracks;
	std::vector<std::string> trackOrder;
	std::array<bool, 8> usedSlots{false};
	int findFreeSlot()
	{
		for (int i = 0; i < 8; ++i)
		{
			if (!usedSlots[i])
				return i;
		}
		return -1;
	}
	void renderSingleTrack(TrackData &track,
						   juce::AudioBuffer<float> &mixOutput,
						   juce::AudioBuffer<float> &individualOutput,
						   int numSamples, double hostSampleRate)
	{
		if (track.numSamples == 0 || !track.isPlaying.load())
			return;

		const float volume = juce::jlimit(0.0f, 1.0f, track.volume.load());
		const float pan = juce::jlimit(-1.0f, 1.0f, track.pan.load());
		double currentPosition = track.readPosition.load();
		double playbackRatio = 1.0;

		switch (track.timeStretchMode)
		{
		case 1:
			playbackRatio = 1.0;
			break;

		case 2:
			if (track.originalBpm > 0.0f)
			{
				float totalBpmAdjust = track.bpmOffset + track.fineOffset;
				float adjustedBpm = track.originalBpm + totalBpmAdjust;
				playbackRatio = adjustedBpm / track.originalBpm;
			}
			break;

		case 3:
			playbackRatio = 1.0;
			break;

		case 4:
			if (track.originalBpm > 0.0f)
			{
				float totalManualAdjust = track.bpmOffset + track.fineOffset;
				playbackRatio = (track.originalBpm + totalManualAdjust) / track.originalBpm;
			}
			break;
		}

		double startSample = track.loopStart * track.sampleRate;
		double endSample = track.loopEnd * track.sampleRate;

		startSample = juce::jlimit(0.0, (double)track.numSamples - 1, startSample);
		endSample = juce::jlimit(startSample + 1, (double)track.numSamples, endSample);

		double sectionLength = endSample - startSample;

		if (sectionLength < 100)
		{
			startSample = 0.0;
			endSample = track.numSamples;
			sectionLength = track.numSamples;
		}

		for (int i = 0; i < numSamples; ++i)
		{
			double absolutePosition = startSample + currentPosition;
			float leftGain = 1.0f;
			float rightGain = 1.0f;

			if (pan < 0.0f)
			{
				rightGain = 1.0f + pan;
			}
			else if (pan > 0.0f)
			{
				leftGain = 1.0f - pan;
			}

			if (absolutePosition >= endSample)
			{
				track.isPlaying = false;
				break;
			}

			if (absolutePosition >= track.numSamples)
			{
				track.isPlaying = false;
				break;
			}

			for (int ch = 0; ch < std::min(2, track.audioBuffer.getNumChannels()); ++ch)
			{
				int sampleIndex = static_cast<int>(absolutePosition);
				if (sampleIndex >= track.audioBuffer.getNumSamples())
				{
					track.isPlaying = false;
					break;
				}

				float sample = track.audioBuffer.getSample(ch, sampleIndex);
				sample *= volume;
				if (ch == 0)
				{
					sample *= leftGain;
				}
				else
				{
					sample *= rightGain;
				}

				mixOutput.addSample(ch, i, sample);
				individualOutput.setSample(ch, i, sample);
				mixOutput.addSample(ch, i, sample);
				individualOutput.setSample(ch, i, sample);
			}
			currentPosition += playbackRatio;
		}
		track.readPosition = currentPosition;
	}

	float interpolateLinear(const float *buffer, double position, int bufferSize)
	{
		int index = static_cast<int>(position);
		if (index >= bufferSize - 1)
			return buffer[bufferSize - 1];

		float fraction = static_cast<float>(position - index);
		return buffer[index] + fraction * (buffer[index + 1] - buffer[index]);
	}
};