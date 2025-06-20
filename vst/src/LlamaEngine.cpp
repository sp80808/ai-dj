#include "LlamaEngine.h"

bool LlamaEngine::initialize(const juce::String& modelPath) {
	try {
		juce::File modelFile(modelPath);
		if (!modelFile.exists()) {
			return false;
		}

		llama_model_params model_params = llama_model_default_params();
		model = llama_load_model_from_file(modelPath.toUTF8(), model_params);

		if (!model) {
			return false;
		}

		llama_context_params ctx_params = llama_context_default_params();
		ctx_params.n_ctx = 0;
		ctx_params.no_perf = true;
		ctx = llama_new_context_with_model(model, ctx_params);

		if (!ctx) {
			llama_free_model(model);
			return false;
		}

		llama_sampler_chain_params sampler_params = llama_sampler_chain_default_params();
		sampler_params.no_perf = true;
		sampler = llama_sampler_chain_init(sampler_params);
		llama_sampler_chain_add(sampler, llama_sampler_init_min_p(0.05f, 1));
		llama_sampler_chain_add(sampler, llama_sampler_init_temp(0.7f));
		llama_sampler_chain_add(sampler, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));

		return true;

	}
	catch (const std::exception& e) {
		return false;
	}
}

juce::String LlamaEngine::generateResponse(const juce::String& userId) {
	auto& conv = conversations[userId];
	std::vector<llama_chat_message> chat_msgs;

	for (const auto& msg : conv) {
		chat_msgs.push_back({
			msg.role.toRawUTF8(),
			msg.content.toRawUTF8()
			});
	}

	std::vector<char> formatted_buffer(8192);
	int32_t new_len = llama_chat_apply_template(
		nullptr,
		chat_msgs.data(),
		chat_msgs.size(),
		true,
		formatted_buffer.data(),
		formatted_buffer.size()
	);

	if (new_len < 0) {
		return "{}";
	}

	std::string formatted(formatted_buffer.data(), new_len);

	const llama_vocab* vocab = llama_model_get_vocab(model);
	std::vector<llama_token> tokens = common_tokenize(vocab, formatted, true, true);

	llama_batch batch = llama_batch_get_one(tokens.data(), tokens.size());

	if (llama_decode(ctx, batch) < 0) {
		return "{}";
	}

	juce::String response = "";
	const int max_tokens = 200;

	for (int i = 0; i < max_tokens; ++i) {
		llama_token token = llama_sampler_sample(sampler, ctx, -1);

		if (llama_vocab_is_eog(vocab, token)) break;

		char buf[128];
		int n = llama_token_to_piece(vocab, token, buf, sizeof(buf), 0, true);
		if (n < 0) {
			break;
		}

		response += juce::String::fromUTF8(buf, n);

		batch = llama_batch_get_one(&token, 1);

		if (llama_decode(ctx, batch) < 0) {
			break;
		}
	}

	return response;
}

nlohmann::json LlamaEngine::getNextDecision(const juce::String& userPrompt,
	const juce::String& userId,
	float bpm,
	const juce::String& key) {

	std::lock_guard<std::mutex> lock(conversationsMutex);

	if (conversations.find(userId) == conversations.end()) {
		initUserConversation(userId);
	}

	juce::String promptText = buildPrompt(userPrompt, bpm, key);

	conversations[userId].push_back({
		"user",
		promptText,
		juce::Time::getCurrentTime().toMilliseconds() / 1000.0
		});

	juce::String response = generateResponse(userId);

	auto decision = parseDecisionResponse(response, key, userPrompt, bpm);

	conversations[userId].push_back({
		"assistant",
		nlohmann::json(decision).dump(),
		juce::Time::getCurrentTime().toMilliseconds() / 1000.0
		});

	cleanupConversation(userId);

	return decision;
}

void LlamaEngine::initUserConversation(const juce::String& userId) {
	conversations[userId] = {
		{"system", getSystemPrompt(), 0.0}
	};
}

juce::String LlamaEngine::buildPrompt(const juce::String& userPrompt,
	float bpm,
	const juce::String& key) {
	return juce::String::formatted(
		R"(NEW USER PROMPT
Keywords: %s

Context:
- Tempo: %.0f BPM
- Key: %s

IMPORTANT: This new prompt has PRIORITY. If it's different from your previous generation, ABANDON the previous style completely and focus on this new prompt.)",
userPrompt.toRawUTF8(),
bpm,
key.toRawUTF8()
);
}

nlohmann::json LlamaEngine::parseDecisionResponse(const juce::String& response,
	const juce::String& defaultKey, const juce::String& userPrompt,
	float bpm) {
	try {
		std::regex jsonRegex(R"(\{.*\})", std::regex_constants::ECMAScript);
		std::smatch match;
		std::string responseStr = response.toStdString();

		if (std::regex_search(responseStr, match, jsonRegex)) {
			std::string jsonStr = match[0].str();
			return nlohmann::json::parse(jsonStr);
		}
	}
	catch (const std::exception& e) {
		DBG("JSON parse error: " << e.what());
	}

	return nlohmann::json{
		{"action_type", "generate_sample"},
		{"parameters", {
			{"sample_details", {
				{"musicgen_prompt", (userPrompt + " " + juce::String(bpm) + "bpm " + defaultKey).toStdString()},
				{"key", defaultKey.toStdString()}
			}}
		}},
		{"reasoning", "Fallback: JSON parsing failed"}
	};
}

juce::String LlamaEngine::getSystemPrompt() {
	return R"(You are a smart music sample generator. The user provides you with keywords, you generate coherent JSON.

MANDATORY FORMAT:
{
    "action_type": "generate_sample",
    "parameters": {
        "sample_details": {
            "musicgen_prompt": "[prompt optimized for MusicGen based on keywords]",
            "key": "[appropriate key or keep the provided one]"
        }
    },
    "reasoning": "Short explanation of your choices"
}

PRIORITY RULES:
1. IF the user requests a specific style/genre → IGNORE the history and generate exactly what they ask for
2. IF it's a vague or similar request → You can consider the history for variety
3. ALWAYS respect keywords User's exact

TECHNICAL RULES:
- Create a consistent and accurate MusicGen prompt
- For the key: use the one provided or adapt if necessary
- Respond ONLY in JSON

EXAMPLES:
User: "deep techno rhythm kick hardcore" → musicgen_prompt: "deep techno kick drum, hardcore rhythm, driving 4/4 beat, industrial"
User: "ambient space" → musicgen_prompt: "ambient atmospheric space soundscape, ethereal pads"
User: "jazzy piano" → musicgen_prompt: "jazz piano, smooth chords, melodic improvisation")";
}

void LlamaEngine::cleanupConversation(const juce::String& userId) {
	auto& conv = conversations[userId];
	if (conv.size() > 9) {
		conv.erase(conv.begin() + 1, conv.begin() + 3);
	}
}
