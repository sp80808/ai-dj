#include "StableAudioEngine.h"

#ifdef _WIN32
#include <direct.h>
#define GETCWD _getcwd
#define CHDIR _chdir
#else
#include <unistd.h>
#define GETCWD getcwd
#define CHDIR chdir
#endif

bool StableAudioEngine::initialize(const juce::String& modelsDir) {
	try {
		DBG("Initializing Stable Audio Engine (subprocess mode)...");
		modelsDirectory = modelsDir;

		auto baseDir = juce::File(modelsDir);
		audiogenExecutable = baseDir.getChildFile("audiogen.exe");

		if (!audiogenExecutable.exists()) {
			audiogenExecutable = baseDir.getChildFile("audiogen");
		}

		if (!checkRequiredFiles()) {
			return false;
		}

		isInitialized = true;
		DBG("Stable Audio Engine ready! Using executable: " << audiogenExecutable.getFullPathName());
		return true;

	}
	catch (const std::exception& e) {
		DBG("Exception during initialization: " << e.what());
		return false;
	}
}

bool StableAudioEngine::checkRequiredFiles() {
	auto baseDir = juce::File(modelsDirectory);

	juce::StringArray missingFiles;

	if (!audiogenExecutable.exists())
		missingFiles.add("audiogen executable");
	if (!baseDir.getChildFile("conditioners_float32.tflite").exists())
		missingFiles.add("conditioners_float32.tflite");
	if (!baseDir.getChildFile("dit_model.tflite").exists())
		missingFiles.add("dit_model.tflite");
	if (!baseDir.getChildFile("autoencoder_model.tflite").exists())
		missingFiles.add("autoencoder_model.tflite");
	if (!baseDir.getChildFile("spiece.model").exists())
		missingFiles.add("spiece.model");

	if (!missingFiles.isEmpty()) {
		DBG("Missing required files: " << missingFiles.joinIntoString(", "));
		return false;
	}

	return true;
}

StableAudioEngine::GenerationResult StableAudioEngine::generateSample(const GenerationParams& params) {
	GenerationResult result;

	if (!isInitialized) {
		result.errorMessage = "Engine not initialized";
		return result;
	}

	try {
		DBG("Generating audio: '" << params.prompt << "' (" << params.duration << "s)");

		auto startTime = juce::Time::getMillisecondCounterHiRes();
		cleanupTempFiles();

		auto sanitizedPrompt = sanitizePrompt(params.prompt);
		auto seed = (params.seed == -1) ? generateRandomSeed() : params.seed;
		auto duration = juce::jlimit(0.1f, 10.0f, params.duration);

		juce::StringArray command;
		command.add(audiogenExecutable.getFullPathName());
		command.add(modelsDirectory);
		command.add(sanitizedPrompt);
		command.add(juce::String(params.numThreads));
		command.add(juce::String(seed));

		DBG("Executing command: " << command.joinIntoString(" "));

		char originalDir[1024];
		GETCWD(originalDir, sizeof(originalDir));

		auto workingDir = juce::File(modelsDirectory);
		DBG("Changing working directory to: " << workingDir.getFullPathName());

		if (CHDIR(workingDir.getFullPathName().toRawUTF8()) != 0) {
			result.errorMessage = "Failed to change working directory";
			return result;
		}

		juce::ChildProcess childProcess;

		if (!childProcess.start(command)) {
			CHDIR(originalDir);
			result.errorMessage = "Failed to start audiogen process";
			return result;
		}

		if (!childProcess.waitForProcessToFinish(60000)) {
			childProcess.kill();
			CHDIR(originalDir);
			result.errorMessage = "Process timeout (60s)";
			return result;
		}

		CHDIR(originalDir);

		auto exitCode = childProcess.getExitCode();
		if (exitCode != 0) {
			result.errorMessage = "Process failed with exit code: " + juce::String(exitCode);
			return result;
		}

		auto outputFile = juce::File(modelsDirectory).getChildFile("output.wav");
		if (!outputFile.exists()) {
			result.errorMessage = "Output file not found: " + outputFile.getFullPathName();
			return result;
		}

		auto audioData = loadWavFile(outputFile);
		if (audioData.empty()) {
			result.errorMessage = "Failed to load generated audio file";
			return result;
		}

		auto endTime = juce::Time::getMillisecondCounterHiRes();

		result.leftChannel.clear();
		result.rightChannel.clear();
		result.audioData.clear();

		result.leftChannel.reserve(audioData.size() / 2);
		result.rightChannel.reserve(audioData.size() / 2);
		result.audioData.reserve(audioData.size() / 2);

		for (size_t i = 0; i < audioData.size(); i += 2) {
			float left = audioData[i];
			float right = (i + 1 < audioData.size()) ? audioData[i + 1] : 0.0f;

			result.leftChannel.push_back(left);
			result.rightChannel.push_back(right);
			result.audioData.push_back((left + right) * 0.5f);
		}

		result.actualDuration = static_cast<float>(result.audioData.size()) / params.sampleRate;
		result.success = true;
		result.performanceInfo = "Generated in " + juce::String(endTime - startTime, 0) + "ms";

		DBG("Generation successful: " << result.audioData.size() << " samples in "
			<< (endTime - startTime) << "ms");
		outputFile.deleteFile();

		return result;

	}
	catch (const std::exception& e) {
		result.errorMessage = juce::String("Generation failed: ") + e.what();
		DBG("Generation exception: " << e.what());
		return result;
	}
}

std::vector<float> StableAudioEngine::generateAudio(const juce::String& prompt, float duration) {
	GenerationParams params(prompt, duration);
	auto result = generateSample(params);
	return result.isValid() ? result.audioData : std::vector<float>();
}

std::vector<float> StableAudioEngine::loadWavFile(const juce::File& wavFile) {
	std::vector<float> audioData;

	try {
		juce::AudioFormatManager formatManager;
		formatManager.registerBasicFormats();

		auto* reader = formatManager.createReaderFor(wavFile);
		if (reader == nullptr) {
			DBG("Failed to create audio reader for: " << wavFile.getFullPathName());
			return audioData;
		}

		std::unique_ptr<juce::AudioFormatReader> readerPtr(reader);

		auto numSamples = static_cast<int>(reader->lengthInSamples);
		auto numChannels = static_cast<int>(reader->numChannels);

		juce::AudioBuffer<float> buffer(numChannels, numSamples);
		reader->read(&buffer, 0, numSamples, 0, true, true);

		audioData.reserve(numSamples * numChannels);

		for (int sample = 0; sample < numSamples; ++sample) {
			for (int channel = 0; channel < numChannels; ++channel) {
				audioData.push_back(buffer.getSample(channel, sample));
			}
		}

		DBG("Loaded WAV: " << numSamples << " samples, " << numChannels << " channels");

	}
	catch (const std::exception& e) {
		DBG("Exception loading WAV file: " << e.what());
		audioData.clear();
	}

	return audioData;
}

juce::String StableAudioEngine::sanitizePrompt(const juce::String& prompt) {
	auto sanitized = prompt.replace("\"", "\\\"")
		.replace("'", "\\'")
		.replace("|", "")
		.replace("&", "and")
		.replace(";", "");
	if (sanitized.length() > 200) {
		sanitized = sanitized.substring(0, 200);
	}

	return sanitized;
}

int StableAudioEngine::generateRandomSeed() {
	return random.nextInt(1000000);
}

void StableAudioEngine::cleanupTempFiles() {
	auto outputFile = juce::File(modelsDirectory).getChildFile("output.wav");
	if (outputFile.exists()) {
		outputFile.deleteFile();
	}
}