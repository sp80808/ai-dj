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
            profile_name (str): Nom du profil DJ à utiliser
            config (dict): Configuration globale
        """
        self.model_path = model_path
        self.session_state = config

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
        if self.session_state["last_action_time"] > 0:
            elapsed = current_time - self.session_state["last_action_time"]
            self.session_state["session_duration"] += elapsed
        self.session_state["last_action_time"] = current_time

        # Générer la réponse du LLM
        user_prompt = self._build_prompt()
        print("\n🧠 Génération AI-DJ prompt...")
        response = self.model.create_chat_completion(
            [
                {"role": "system", "content": self.get_system_prompt()},
                {"role": "user", "content": user_prompt},
            ]
        )
        print("✅ Génération terminée !")
        try:
            # Extraire et parser la réponse JSON
            response_text = response["choices"][0]["message"]["content"]
            # Trouver le JSON dans la réponse (au cas où le modèle ajoute du texte autour)
            import re

            json_match = re.search(r"({.*})", response_text, re.DOTALL)
            if json_match:
                json_str = json_match.group(1)
                decision = json.loads(json_str)
            else:
                # Fallback si pas de JSON trouvé
                decision = {
                    "action_type": "sample",  # Action par défaut
                    "parameters": {"type": "techno_kick", "intensity": 5},
                    "reasoning": "Fallback: Pas de réponse JSON valide",
                }
        except (json.JSONDecodeError, KeyError) as e:
            print(f"Erreur de parsing de la réponse: {e}")
            print(f"Réponse brute: {response_text}")
            # Décision par défaut en cas d'erreur
            decision = {
                "action_type": "sample",
                "parameters": {"type": "techno_kick", "intensity": 5},
                "reasoning": f"Erreur: {str(e)}",
            }

        # Enregistrer la décision dans l'historique
        self.session_state["history"].append(decision)

        return decision

    def _build_prompt(self):
        """Prompt user minimal"""
        special_instruction = self.session_state.get("special_instruction", "")

        return f"""Mots-clés utilisateur: {special_instruction}"""

    def get_system_prompt(self) -> str:
        return """Tu es un générateur de samples musicaux. L'utilisateur te donne des mots-clés, tu génères un JSON simple.

    FORMAT OBLIGATOIRE :
    {
        "action_type": "generate_sample",
        "parameters": {
            "sample_details": {
                "musicgen_prompt": "[prompt optimisé pour MusicGen basé sur les mots-clés]",
                "key": "[tonalité appropriée ou garde celle fournie]"
            }
        },
        "reasoning": "Explication courte"
    }

    RÈGLES :
    - Crée un prompt MusicGen cohérent à partir des mots-clés de l'user
    - Pour la tonalité : utilise celle fournie ou adapte si le style l'exige
    - Réponds UNIQUEMENT en JSON

    EXEMPLES :
    User: "ambient space" → musicgen_prompt: "ambient atmospheric space soundscape, ethereal pads"
    User: "hard kick techno" → musicgen_prompt: "hard techno kick, driving 4/4 beat, industrial"
    User: "jazzy piano" → musicgen_prompt: "jazz piano, smooth chords, melodic improvisation"
    """
