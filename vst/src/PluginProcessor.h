#pragma once
#include "JuceHeader.h"
#include "TrackManager.h"
#include "DjIaClient.h"
#include "MidiLearnManager.h"
#include "SimpleEQ.h"
#include <memory>
#include <unordered_map>
#include <vector>
#include <atomic>

class DjIaVstEditor;
class TrackComponent;

class DjIaVstProcessor : public juce::AudioProcessor,
						 public juce::AudioProcessorValueTreeState::Listener,
						 public juce::Timer
{
public:
	void timerCallback() override;
	std::function<void()> onUIUpdateNeeded;

	DjIaVstProcessor();
	~DjIaVstProcessor() override;

	void initDummySynth();
	void initTracks();
	void loadParameters();
	void cleanProcessor();
	void parameterChanged(const juce::String &parameterID, float newValue) override;
	void prepareToPlay(double sampleRate, int samplesPerBlock) override;
	void releaseResources() override;
	void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;
	void checkIfUIUpdateNeeded(juce::MidiBuffer &midiMessages);
	void applyMasterEffects(juce::AudioSampleBuffer &mainOutput);
	void copyTracksToIndividualOutputs(juce::AudioSampleBuffer &buffer);
	void clearOutputBuffers(juce::AudioSampleBuffer &buffer);
	void resizeIndividualsBuffers(juce::AudioSampleBuffer &buffer);
	void getDawInformations(juce::AudioPlayHead *playHead, bool &hostIsPlaying, double &hostBpm, double &hostPpqPosition);
	bool isBusesLayoutSupported(const BusesLayout &layouts) const override;
	bool getDrumsEnabled() const { return drumsEnabled; }
	void setDrumsEnabled(bool enabled) { drumsEnabled = enabled; }
	bool getBassEnabled() const { return bassEnabled; }
	void setBassEnabled(bool enabled) { bassEnabled = enabled; }
	bool getOtherEnabled() const { return otherEnabled; }
	void setOtherEnabled(bool enabled) { otherEnabled = enabled; }
	bool getVocalsEnabled() const { return vocalsEnabled; }
	void setVocalsEnabled(bool enabled) { vocalsEnabled = enabled; }
	bool getGuitarEnabled() const { return guitarEnabled; }
	void setGuitarEnabled(bool enabled) { guitarEnabled = enabled; }
	bool getPianoEnabled() const { return pianoEnabled; }
	void setPianoEnabled(bool enabled) { pianoEnabled = enabled; }
	double getLastBpm() const { return lastBpm; }
	void setLastBpm(double bpm) { lastBpm = bpm; }
	double getLastDuration() const { return lastDuration; }
	void setLastDuration(double duration) { lastDuration = duration; }
	int getLastKeyIndex() const { return lastKeyIndex; }
	void setLastKeyIndex(int index) { lastKeyIndex = index; }
	juce::AudioProcessorEditor *createEditor() override;
	bool hasEditor() const override { return true; }
	const juce::String getName() const override { return "DJ-IA VST"; }
	bool acceptsMidi() const override { return true; }
	bool producesMidi() const override { return false; }
	bool isMidiEffect() const override { return false; }
	double getTailLengthSeconds() const override { return 0.0; }
	int getNumPrograms() override { return 1; }
	int getCurrentProgram() override { return 0; }
	void setCurrentProgram(int) override {}
	const juce::String getProgramName(int) override { return {}; }
	void changeProgramName(int, const juce::String &) override {}
	void getStateInformation(juce::MemoryBlock &destData) override;
	void setStateInformation(const void *data, int sizeInBytes) override;
	void updateUI();
	void addCustomPromptsToIndexedPrompts(juce::ValueTree &promptsState, juce::Array<std::pair<int, juce::String>> &indexedPrompts);
	void loadCustomPromptsByCountProperty(juce::ValueTree &promptsState);
	void setMasterVolume(float volume) { masterVolume = volume; }
	void setMasterPan(float pan) { masterPan = pan; }
	void setMasterEQ(float high, float mid, float low)
	{
		masterHighEQ = high;
		masterMidEQ = mid;
		masterLowEQ = low;
	}
	float getMasterVolume() const { return masterVolume.load(); }
	float getMasterPan() const { return masterPan.load(); }
	juce::String createNewTrack(const juce::String &name = "Track");
	void deleteTrack(const juce::String &trackId);
	void selectTrack(const juce::String &trackId);
	juce::String getSelectedTrackId() const { return selectedTrackId; }
	void reorderTracks(const juce::String &fromTrackId, const juce::String &toTrackId);
	std::vector<juce::String> getAllTrackIds() const { return trackManager.getAllTrackIds(); }
	TrackData *getCurrentTrack() { return trackManager.getTrack(selectedTrackId); }
	TrackData *getTrack(const juce::String &trackId) { return trackManager.getTrack(trackId); }
	void generateLoop(const DjIaClient::LoopRequest &request, const juce::String &targetTrackId = "");
	void startNotePlaybackForTrack(const juce::String &trackId, int noteNumber, double hostBpm = 126.0);
	void setApiKey(const juce::String &key);
	void setServerUrl(const juce::String &url);
	double getHostBpm() const;
	juce::String getServerUrl() const { return serverUrl; }
	juce::String getApiKey() const { return apiKey; }
	juce::String getLastPrompt() const { return lastPrompt; }
	juce::String getLastKey() const { return lastKey; }
	int getLastPresetIndex() const { return lastPresetIndex; }
	bool getHostBpmEnabled() const { return hostBpmEnabled; }
	bool getServerSidePreTreatment() const { return serverSidePreTreatment; }
	void setLastPrompt(const juce::String &prompt) { lastPrompt = prompt; }
	void setLastPresetIndex(int index) { lastPresetIndex = index; }
	void setHostBpmEnabled(bool enabled) { hostBpmEnabled = enabled; }
	void updateAllWaveformsAfterLoad();
	void setAutoLoadEnabled(bool enabled);
	void setServerSidePreTreatment(bool enabled);
	bool getAutoLoadEnabled() const { return autoLoadEnabled.load(); }
	void loadPendingSample();
	bool hasSampleWaiting() const { return hasUnloadedSample.load(); }
	void setMidiIndicatorCallback(std::function<void(const juce::String &)> callback)
	{
		midiIndicatorCallback = callback;
	}
	juce::AudioProcessorValueTreeState &getParameters() { return parameters; }
	void addCustomPrompt(const juce::String &prompt);
	juce::StringArray getCustomPrompts() const;
	void clearCustomPrompts();
	bool getIsGenerating() const { return isGenerating; }
	void setIsGenerating(bool generating) { isGenerating = generating; }
	juce::String getGeneratingTrackId() const { return generatingTrackId; }
	void setGeneratingTrackId(const juce::String &trackId) { generatingTrackId = trackId; }
	bool isStateReady() const { return stateLoaded; }
	MidiLearnManager &getMidiLearnManager() { return midiLearnManager; }
	juce::ValueTree pendingMidiMappings;
	juce::AudioProcessorValueTreeState &getParameterTreeState() { return parameters; }
	std::atomic<bool> needsUIUpdate{false};
	void handleSampleParams(int slot, TrackData *track);

private:
	std::atomic<double> cachedHostBpm{126.0};
	std::atomic<bool> stateLoaded{false};
	static juce::AudioProcessor::BusesProperties createBusLayout();
	static const int MAX_TRACKS = 8;
	double lastDuration = 6.0;
	void updateMasterEQ();
	void loadAudioDataAsync(const juce::String &trackId, const juce::MemoryBlock &audioData);
	void processAudioBPMAndSync(TrackData *track);
	void loadAudioToStagingBuffer(std::unique_ptr<juce::AudioFormatReader> &reader, TrackData *track);
	void checkAndSwapStagingBuffers();
	void performAtomicSwap(TrackData *track, const juce::String &trackId);
	void updateWaveformDisplay(const juce::String &trackId);
	void performTrackDeletion(const juce::String &trackId);
	void reassignTrackOutputsAndMidi();
	bool drumsEnabled = false;
	bool bassEnabled = false;
	bool otherEnabled = false;
	bool vocalsEnabled = false;
	bool guitarEnabled = false;
	bool pianoEnabled = false;
	int lastKeyIndex = 1;
	bool isGenerating = false;
	juce::String generatingTrackId = "";
	std::unordered_map<int, juce::String> playingTracks;
	juce::StringArray customPrompts;
	TrackManager trackManager;
	MidiLearnManager midiLearnManager;
	juce::String selectedTrackId;
	std::vector<juce::AudioBuffer<float>> individualOutputBuffers;
	std::atomic<bool> isNotePlaying{false};
	std::atomic<int> currentNoteNumber{-1};
	juce::String lastPrompt = "";
	juce::String lastKey = "C minor";
	double lastBpm = 126.0;
	int lastPresetIndex = -1;
	bool hostBpmEnabled = false;
	double hostSampleRate = 0.0;
	double audioSampleRate = 0.0;
	juce::String serverUrl = "http://localhost:8000";
	juce::String apiKey;
	DjIaClient apiClient;
	juce::CriticalSection apiLock;
	juce::MemoryBlock pendingAudioData;
	juce::String pendingTrackId;
	std::atomic<bool> hasPendingAudioData{false};
	std::atomic<bool> autoLoadEnabled{true};
	std::atomic<bool> hasUnloadedSample{false};
	std::atomic<bool> serverSidePreTreatment{true};
	juce::Synthesiser synth;
	std::function<void(const juce::String &)> midiIndicatorCallback;
	juce::AudioProcessorValueTreeState parameters;
	std::atomic<float> *generateParam = nullptr;
	std::atomic<float> *playParam = nullptr;
	std::atomic<float> *autoLoadParam = nullptr;
	SimpleEQ masterEQ;
	std::atomic<float> masterVolume{0.8f};
	std::atomic<float> masterPan{0.0f};
	std::atomic<float> masterHighEQ{0.0f};
	std::atomic<float> masterMidEQ{0.0f};
	std::atomic<float> masterLowEQ{0.0f};
	std::atomic<bool> waitingForMidiToLoad{false};
	juce::String trackIdWaitingForLoad;
	std::atomic<bool> correctMidiNoteReceived{false};
	void processIncomingAudio();
	void clearPendingAudio();
	void processMidiMessages(juce::MidiBuffer &midiMessages, bool hostIsPlaying, double hostBpm);
	void playTrack(const juce::MidiMessage &message, double hostBpm);
	void updateTimeStretchRatios(double hostBpm);
	TrackComponent *findTrackComponentByName(const juce::String &trackName, DjIaVstEditor *editor);
	juce::Button *findGenerateButtonInTrack(TrackComponent *trackComponent);
	juce::Slider *findBpmOffsetSliderInTrack(TrackComponent *trackComponent);
	void stopNotePlaybackForTrack(int noteNumber);
	std::atomic<float> *masterVolumeParam = nullptr;
	std::atomic<float> *masterPanParam = nullptr;
	std::atomic<float> *masterHighParam = nullptr;
	std::atomic<float> *masterMidParam = nullptr;
	std::atomic<float> *masterLowParam = nullptr;
	std::atomic<float> *slotVolumeParams[8] = {nullptr};
	std::atomic<float> *slotPanParams[8] = {nullptr};
	std::atomic<float> *slotMuteParams[8] = {nullptr};
	std::atomic<float> *slotSoloParams[8] = {nullptr};
	std::atomic<float> *slotPlayParams[8] = {nullptr};
	std::atomic<float> *slotStopParams[8] = {nullptr};
	std::atomic<float> *slotGenerateParams[8] = {nullptr};
	std::atomic<float> *slotPitchParams[8] = {nullptr};
	std::atomic<float> *slotFineParams[8] = {nullptr};
	std::atomic<float> *slotBpmOffsetParams[8] = {nullptr};

	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DjIaVstProcessor)
};