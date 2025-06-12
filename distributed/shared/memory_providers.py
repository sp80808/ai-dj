from abc import ABC, abstractmethod
from datetime import datetime
from typing import Dict, Any
import threading


class MemoryProvider(ABC):
    @abstractmethod
    def get_or_create_conversation(self, user_id: str) -> Dict[str, Any]:
        pass

    @abstractmethod
    def update_conversation(self, user_id: str, conversation_data: Dict[str, Any]):
        pass


class LocalMemoryProvider(MemoryProvider):
    def __init__(self):
        self.conversations = {}
        self.conversations_lock = threading.Lock()

    def get_or_create_conversation(self, user_id: str) -> Dict[str, Any]:
        with self.conversations_lock:
            if user_id not in self.conversations:
                self.conversations[user_id] = {
                    "conversation_history": [],
                    "last_message_timestamp": datetime.now(),
                }
            return self.conversations[user_id]

    def update_conversation(self, user_id: str, conversation_data: Dict[str, Any]):
        with self.conversations_lock:
            self.conversations[user_id] = conversation_data


class DistributedMemoryProvider(MemoryProvider):
    def __init__(self, session_memory: Dict[str, Any] = None):
        self.session_memory = session_memory or {}
        self.default_conversation = {
            "conversation_history": [],
            "last_message_timestamp": datetime.now(),
        }

    def get_or_create_conversation(self, user_id: str) -> Dict[str, Any]:
        return self.session_memory.get("conversation", self.default_conversation)

    def update_conversation(self, user_id: str, conversation_data: Dict[str, Any]):
        self.session_memory["conversation"] = conversation_data


class StatelessMemoryProvider(MemoryProvider):
    def __init__(self, system_prompt: str = None):
        self.system_prompt = system_prompt
        self.default_conversation = {
            "conversation_history": (
                [{"role": "system", "content": system_prompt}] if system_prompt else []
            ),
            "last_message_timestamp": datetime.now(),
        }

    def get_or_create_conversation(self, user_id: str) -> Dict[str, Any]:
        return self.default_conversation.copy()

    def update_conversation(self, user_id: str, conversation_data: Dict[str, Any]):
        pass
