import json
import time
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

    def _init_model(self):
        """Initialise ou réinitialise le modèle LLM"""
        print(f"⚡ Initialisation du modèle LLM depuis {self.model_path}...")

        # Si un modèle existe déjà, le détruire explicitement
        if hasattr(self, "model"):
            try:
                del self.model
                import gc

                gc.collect()  # Force la libération de mémoire
                print("🧹 Ancien modèle détruit")
            except Exception as e:
                print(f"⚠️ Erreur lors de la destruction du modèle: {e}")

        # Initialiser un nouveau modèle LLM
        self.model = Llama(
            model_path=self.model_path,
            n_ctx=4096,  # Contexte suffisamment grand
            n_gpu_layers=-1,  # Utiliser tous les layers GPU disponibles
            n_threads=4,  # Ajuster selon CPU
            verbose=False,
        )
        print("✅ Nouveau modèle LLM initialisé")

    def get_next_decision(self):
        """Obtient la prochaine décision du DJ IA"""

        # Mise à jour du temps de session
        current_time = time.time()
        if self.session_state.get("last_action_time", 0) > 0:
            elapsed = current_time - self.session_state["last_action_time"]
            self.session_state["session_duration"] = (
                self.session_state.get("session_duration", 0) + elapsed
            )
        self.session_state["last_action_time"] = current_time

        # Construire le prompt utilisateur
        user_prompt = self._build_prompt()

        # Ajouter à l'historique de conversation
        self.conversation_history.append({"role": "user", "content": user_prompt})

        # Garder seulement les 10 derniers échanges (system + 9 pairs user/assistant)
        # pour éviter de dépasser le contexte
        if len(self.conversation_history) > 19:  # system + 9*2 messages
            # Garder le system prompt + les 8 derniers échanges
            self.conversation_history = [
                self.conversation_history[0]
            ] + self.conversation_history[-16:]
            print("🧹 Historique tronqué pour rester dans le contexte")

        print(
            f"\n🧠 Génération AI-DJ avec {len(self.conversation_history)} messages d'historique..."
        )

        # Générer avec tout l'historique
        response = self.model.create_chat_completion(self.conversation_history)

        print("✅ Génération terminée !")

        try:
            # Extraire et parser la réponse JSON
            response_text = response["choices"][0]["message"]["content"]

            # Ajouter la réponse à l'historique AVANT de parser
            self.conversation_history.append(
                {"role": "assistant", "content": response_text}
            )

            # Trouver le JSON dans la réponse
            import re

            json_match = re.search(r"({.*})", response_text, re.DOTALL)
            if json_match:
                json_str = json_match.group(1)
                decision = json.loads(json_str)
            else:
                # Fallback si pas de JSON trouvé
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
