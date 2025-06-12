import json
import re
import time
import gc
from datetime import datetime, timedelta
from apscheduler.schedulers.background import BackgroundScheduler
from llama_cpp import Llama
import torch
from typing import Dict, Any


class DJAILL:
    def __init__(self, model_path, config=None, memory_provider=None):
        self.model_path = model_path
        self.session_state = config or {}
        self.memory_provider = memory_provider

        if self.memory_provider is None:
            from distributed.shared.memory_providers import LocalMemoryProvider

            self.memory_provider = LocalMemoryProvider()

        if hasattr(self.memory_provider, "conversations"):
            self.scheduler = BackgroundScheduler()
            self.scheduler.add_job(
                func=self.scheduled_cleanup, trigger="interval", minutes=60
            )

    def destroy_model(self):
        if hasattr(self, "model"):
            try:
                del self.model
                if torch.cuda.is_available():
                    torch.cuda.empty_cache()
                gc.collect()
                print("🧹 Model destroyed")
            except Exception as e:
                print(f"❌ Error destroying model: {e}")

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

    def scheduled_cleanup(self):
        """Only works in local mode with LocalMemoryProvider"""
        if not hasattr(self.memory_provider, "conversations"):
            return

        conversations = getattr(self.memory_provider, "conversations", {})
        if not conversations:
            print("🧹 Cleanup: No conversations to clean")
            return

        conversations_lock = getattr(self.memory_provider, "conversations_lock", None)
        if not conversations_lock:
            return

        with conversations_lock:
            cutoff_time = datetime.now() - timedelta(seconds=3600)
            initial_count = len(conversations)

            conversations = {
                user_id: conv
                for user_id, conv in conversations.items()
                if not (
                    isinstance(conv, dict)
                    and conv.get("last_message_timestamp", datetime.now()) < cutoff_time
                )
            }

            final_count = len(conversations)
            cleaned_count = initial_count - final_count

            if cleaned_count > 0:
                print(
                    f"🧹 Cleanup: Removed {cleaned_count} old conversation(s), {final_count} remaining"
                )
            else:
                print(f"🧹 Cleanup: No old conversations found, {final_count} active")

    def init_user_conversation_history(self, user_id):
        conversation = self.memory_provider.get_or_create_conversation(user_id)

        if not conversation.get("conversation_history"):
            conversation["conversation_history"] = [
                {"role": "system", "content": self.get_system_prompt()}
            ]
            conversation["last_message_timestamp"] = datetime.now()
            self.memory_provider.update_conversation(user_id, conversation)

        return conversation

    def get_next_decision(self):
        current_time = time.time()
        if self.session_state.get("last_action_time", 0) > 0:
            elapsed = current_time - self.session_state["last_action_time"]
            self.session_state["session_duration"] = (
                self.session_state.get("session_duration", 0) + elapsed
            )
        self.session_state["last_action_time"] = current_time

        user_prompt = self._build_prompt()
        user_id = self.session_state.get("user_id", "default")

        conversation = self.init_user_conversation_history(user_id)

        conversation["conversation_history"].append(
            {"role": "user", "content": user_prompt}
        )

        print(
            f"🧠 AI-DJ generation with {len(conversation['conversation_history'])} history messages..."
        )

        response = self.model.create_chat_completion(
            conversation["conversation_history"]
        )
        print("✅ Generation complete!")

        try:
            response_text = response["choices"][0]["message"]["content"]
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
            print(f"❌ Error parsing response: {e}")
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

        conversation["conversation_history"].append(
            {"role": "assistant", "content": decision}
        )
        conversation["last_message_timestamp"] = datetime.now()

        if len(conversation["conversation_history"]) > 9:
            conversation["conversation_history"] = (
                conversation["conversation_history"][:1]
                + conversation["conversation_history"][3:]
            )

        self.memory_provider.update_conversation(user_id, conversation)

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
        """Reset conversation - behavior depends on memory provider"""
        user_id = self.session_state.get("user_id", "default")
        fresh_conversation = {
            "conversation_history": [
                {"role": "system", "content": self.get_system_prompt()}
            ],
            "last_message_timestamp": datetime.now(),
        }
        self.memory_provider.update_conversation(user_id, fresh_conversation)
        print("🔄 Conversation history reset")

    def get_conversation_state(self) -> Dict[str, Any]:
        """Get current conversation state for distributed mode"""
        user_id = self.session_state.get("user_id", "default")
        return self.memory_provider.get_or_create_conversation(user_id)
