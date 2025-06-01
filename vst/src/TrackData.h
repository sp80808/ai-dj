struct TrackData
{
	juce::String trackId;
	juce::String trackName;
	int slotIndex = -1;
	std::atomic<bool> isPlaying{false};
	std::atomic<bool> isArmed{false};
	std::atomic<bool> isArmedToStop{false};
	std::atomic<bool> isCurrentlyPlaying{false};
	float fineOffset = 0.0f;
	std::atomic<double> cachedPlaybackRatio{1.0};
	juce::AudioSampleBuffer stagingBuffer;
	std::atomic<bool> hasStagingData{false};
	std::atomic<bool> swapRequested{false};
	std::function<void(bool)> onPlayStateChanged;
	std::atomic<int> stagingNumSamples{0};
	std::atomic<double> stagingSampleRate{48000.0};
	float stagingOriginalBpm = 126.0f;
	double loopStart = 0.0;
	double loopEnd = 4.0;
	float originalBpm = 126.0f;
	int timeStretchMode = 4;
	double timeStretchRatio = 1.0;
	double bpmOffset = 0.0;
	int midiNote = 60;
	juce::AudioSampleBuffer audioBuffer;
	double sampleRate = 48000.0;
	int numSamples = 0;
	std::atomic<bool> isEnabled{true};
	std::atomic<bool> isSolo{false};
	std::atomic<bool> isMuted{false};
	std::atomic<float> volume{0.8f};
	std::atomic<float> pan{0.0f};
	juce::String prompt;
	juce::String style;
	juce::String stems;
	float bpm = 126.0f;
	std::atomic<double> readPosition{0.0};
	bool showWaveform = false;

	TrackData() : trackId(juce::Uuid().toString())
	{
		volume = 0.8f;
		pan = 0.0f;
		readPosition = 0.0;
		bpmOffset = 0.0;
		onPlayStateChanged = nullptr;
	}

	void reset()
	{
		audioBuffer.setSize(0, 0);
		numSamples = 0;
		readPosition = 0.0;
		isEnabled = true;
		isMuted = false;
		isSolo = false;
		volume = 0.8f;
		pan = 0.0f;
		bpmOffset = 0.0;
	}

	void setPlaying(bool playing)
	{
		bool wasPlaying = isPlaying.load();
		isPlaying = playing;
		if (wasPlaying != playing && onPlayStateChanged)
		{
			juce::MessageManager::callAsync([this, playing]()
											{
					if (onPlayStateChanged) {
						onPlayStateChanged(playing);
					} });
		}
	}
};