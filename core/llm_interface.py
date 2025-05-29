import json
import re
import time
import gc
from llama_cpp import Llama


class DJAILL:

    def __init__(self, model_path, config=None):
        self.model_path = model_path
        self.session_state = config or {}
        self.conversation_history = [
            {"role": "system", "content": self.get_system_prompt()}
        ]

    def destroy_model(self):
        if hasattr(self, "model"):
            try:
                del self.model
                gc.collect()
                print("🧹 Model destroyed")
            except Exception as e:
                print(f"⚠️ Error destroying model: {e}")

    def init_model(self):
        print(f"⚡ Initializing LLM model from {self.model_path}...")
        self.model = Llama(
            model_path=self.model_path,
            n_ctx=4096,
            n_gpu_layers=-1,
            n_threads=4,
            verbose=False,
        )
        print("✅ New LLM model initialized")

    def _ensure_alternating_roles(self):
        if len(self.conversation_history) < 2:
            return

        system_msg = None
        messages = self.conversation_history[:]

        if messages and messages[0].get("role") == "system":
            system_msg = messages.pop(0)

        cleaned_messages = []
        last_role = None

        for msg in messages:
            current_role = msg.get("role")

            if current_role != last_role:
                cleaned_messages.append(msg)
                last_role = current_role
            else:
                if cleaned_messages:
                    cleaned_messages[-1]["content"] += f"\n\n{msg['content']}"

        if system_msg:
            self.conversation_history = [system_msg] + cleaned_messages
        else:
            self.conversation_history = cleaned_messages

    def _add_message_safely(self, role, content):
        if not self.conversation_history:
            self.conversation_history.append({"role": role, "content": content})
            return

        last_msg = self.conversation_history[-1]
        if last_msg.get("role") == role:
            last_msg["content"] += f"\n\n{content}"
        else:
            self.conversation_history.append({"role": role, "content": content})

    def get_next_decision(self):
        current_time = time.time()
        if self.session_state.get("last_action_time", 0) > 0:
            elapsed = current_time - self.session_state["last_action_time"]
            self.session_state["session_duration"] = (
                self.session_state.get("session_duration", 0) + elapsed
            )
        self.session_state["last_action_time"] = current_time
        user_prompt = self._build_prompt()
        self._add_message_safely("user", user_prompt)

        if len(self.conversation_history) > 19:

            system_prompt = self.conversation_history[0]
            recent_pairs = self.conversation_history[-16:]

            self.conversation_history = [system_prompt] + recent_pairs

        print(
            f"\n🧠 AI-DJ generation with {len(self.conversation_history)} history messages..."
        )
        response = self.model.create_chat_completion(self.conversation_history)

        print("✅ Generation complete!")

        try:
            response_text = response["choices"][0]["message"]["content"]
            self._add_message_safely("assistant", response_text)

            json_match = re.search(r"({.*})", response_text, re.DOTALL)
            if json_match:
                json_str = json_match.group(1)
                decision = json.loads(json_str)
            else:
                decision = {
                    "action_type": "generate_sample",
                    "parameters": {
                        "sample_details": {
                            "musicgen_prompt": "techno kick drum, driving beat",
                            "key": self.session_state.get("current_key", "C minor"),
                        }
                    },
                    "reasoning": "Fallback: No valid JSON response",
                }

        except (json.JSONDecodeError, KeyError) as e:
            print(f"Error parsing response: {e}")
            print(f"Raw response: {response_text}")

            decision = {
                "action_type": "generate_sample",
                "parameters": {
                    "sample_details": {
                        "musicgen_prompt": "electronic music sample",
                        "key": self.session_state.get("current_key", "C minor"),
                    }
                },
                "reasoning": f"Parsing error: {str(e)}",
            }

        if "history" not in self.session_state:
            self.session_state["history"] = []
        self.session_state["history"].append(decision)

        return decision

    def _build_prompt(self):
        user_prompt = self.session_state.get("user_prompt", "")
        current_tempo = self.session_state.get("current_tempo", 126)
        current_key = self.session_state.get("current_key", "C minor")

        return f"""⚠️ NEW USER PROMPT ⚠️
Keywords: {user_prompt}

Context:
- Tempo: {current_tempo} BPM
- Key: {current_key}

IMPORTANT: This new prompt has PRIORITY. If it's different from your previous generation, ABANDON the previous style completely and focus on this new prompt."""

    def get_system_prompt(self) -> str:
        return """You are a smart music sample generator. The user provides you with keywords, you generate coherent JSON.

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
1. 🔥 IF the user requests a specific style/genre → IGNORE the history and generate exactly what they ask for
2. 📝 IF it's a vague or similar request → You can consider the history for variety
3. 🎯 ALWAYS respect keywords User's exact

TECHNICAL RULES:
- Create a consistent and accurate MusicGen prompt
- For the key: use the one provided or adapt if necessary
- Respond ONLY in JSON

EXAMPLES:
User: "deep techno rhythm kick hardcore" → musicgen_prompt: "deep techno kick drum, hardcore rhythm, driving 4/4 beat, industrial"
User: "ambient space" → musicgen_prompt: "ambient atmospheric space soundscape, ethereal pads"
User: "jazzy piano" → musicgen_prompt: "jazz piano, smooth chords, melodic improvisation"""

    def reset_conversation(self):
        self.conversation_history = [
            {"role": "system", "content": self.get_system_prompt()}
        ]
        print("🔄 Conversation history reset")
