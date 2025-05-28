import json
import re
import time
import gc
from llama_cpp import Llama


class DJAILL:
    """Interface avec le LLM qui joue le rôle du DJ"""

    def __init__(self, model_path, config=None):
        """
        Initialise l'interface avec le LLM

        Args:
            model_path (str): Chemin vers le modèle GMA-4B
            config (dict): Configuration globale
        """
        self.model_path = model_path
        self.session_state = config or {}

        # Historique de conversation pour le LLM
        self.conversation_history = [
            {"role": "system", "content": self.get_system_prompt()}
        ]

    def destroy_model(self):
        if hasattr(self, "model"):
            try:
                del self.model
                gc.collect()
                print("🧹 Modèle détruit")
            except Exception as e:
                print(f"⚠️ Erreur lors de la destruction du modèle: {e}")

    def init_model(self):
        """Initialise ou réinitialise le modèle LLM"""
        print(f"⚡ Initialisation du modèle LLM depuis {self.model_path}...")

        self.model = Llama(
            model_path=self.model_path,
            n_ctx=4096,
            n_gpu_layers=-1,
            n_threads=4,
            verbose=False,
        )
        print("✅ Nouveau modèle LLM initialisé")

    def _ensure_alternating_roles(self):
        """S'assure que les rôles alternent correctement user/assistant"""
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
        """Ajoute un message en vérifiant l'alternance"""
        if not self.conversation_history:
            self.conversation_history.append({"role": role, "content": content})
            return

        last_msg = self.conversation_history[-1]
        if last_msg.get("role") == role:
            last_msg["content"] += f"\n\n{content}"
        else:
            self.conversation_history.append({"role": role, "content": content})

    def get_next_decision(self):
        """Obtient la prochaine décision du DJ IA"""
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
            f"\n🧠 Génération AI-DJ avec {len(self.conversation_history)} messages d'historique..."
        )
        response = self.model.create_chat_completion(self.conversation_history)

        print("✅ Génération terminée !")

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
                    "reasoning": "Fallback: Pas de réponse JSON valide",
                }

        except (json.JSONDecodeError, KeyError) as e:
            print(f"Erreur de parsing de la réponse: {e}")
            print(f"Réponse brute: {response_text}")

            # Décision par défaut en cas d'erreur
            decision = {
                "action_type": "generate_sample",
                "parameters": {
                    "sample_details": {
                        "musicgen_prompt": "electronic music sample",
                        "key": self.session_state.get("current_key", "C minor"),
                    }
                },
                "reasoning": f"Erreur de parsing: {str(e)}",
            }

        # Enregistrer aussi dans l'historique legacy (si besoin pour autres parties du code)
        if "history" not in self.session_state:
            self.session_state["history"] = []
        self.session_state["history"].append(decision)

        return decision

    def _build_prompt(self):
        """Prompt user avec priorité sur la demande actuelle"""
        user_prompt = self.session_state.get("user_prompt", "")
        current_tempo = self.session_state.get("current_tempo", 126)
        current_key = self.session_state.get("current_key", "C minor")

        return f"""⚠️ NOUVELLE DEMANDE UTILISATEUR ⚠️
Mots-clés: {user_prompt}

Context:
- Tempo: {current_tempo} BPM  
- Tonalité: {current_key}

IMPORTANT: Cette nouvelle demande est PRIORITAIRE. Si elle est différente de tes générations précédentes, ABANDONNE complètement le style précédent et concentre-toi sur cette nouvelle demande."""

    def get_system_prompt(self) -> str:
        return """Tu es un générateur de samples musicaux intelligent. L'utilisateur te donne des mots-clés, tu génères un JSON cohérent.

FORMAT OBLIGATOIRE :
{
    "action_type": "generate_sample",
    "parameters": {
        "sample_details": {
            "musicgen_prompt": "[prompt optimisé pour MusicGen basé sur les mots-clés]",
            "key": "[tonalité appropriée ou garde celle fournie]"
        }
    },
    "reasoning": "Explication courte de tes choix"
}

RÈGLES DE PRIORITÉ :
1. 🔥 SI l'utilisateur demande un style/genre spécifique → IGNORE l'historique et génère exactement ce qu'il demande
2. 📝 SI c'est une demande vague ou similaire → Tu peux tenir compte de l'historique pour la variété
3. 🎯 TOUJOURS respecter les mots-clés exacts de l'utilisateur

RÈGLES TECHNIQUES :
- Crée un prompt MusicGen cohérent et précis
- Pour la tonalité : utilise celle fournie ou adapte si nécessaire
- Réponds UNIQUEMENT en JSON

EXEMPLES :
User: "deep techno rhythm kick hardcore" → musicgen_prompt: "deep techno kick drum, hardcore rhythm, driving 4/4 beat, industrial"
User: "ambient space" → musicgen_prompt: "ambient atmospheric space soundscape, ethereal pads"
User: "jazzy piano" → musicgen_prompt: "jazz piano, smooth chords, melodic improvisation"
"""

    def reset_conversation(self):
        """Remet à zéro l'historique de conversation"""
        self.conversation_history = [
            {"role": "system", "content": self.get_system_prompt()}
        ]
        print("🔄 Historique de conversation remis à zéro")
