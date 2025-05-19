import json
import time
from llama_cpp import Llama


class DJAILL:
    """Interface avec le LLM qui joue le rôle du DJ"""

    def __init__(self, model_path, profile_name, config=None, override_prompt=None):
        """
        Initialise l'interface avec le LLM

        Args:
            model_path (str): Chemin vers le modèle GMA-4B
            profile_name (str): Nom du profil DJ à utiliser
            config (dict): Configuration globale
        """

        if override_prompt:
            self.system_prompt = override_prompt
            self.profile = None
        else:
            from config.dj_profiles import DJ_PROFILES

            self.profile = DJ_PROFILES[profile_name]
            self.system_prompt = self.profile["system_prompt"]

        # Initialiser le modèle LLM
        self.model = Llama(
            model_path=model_path,
            n_ctx=4096,  # Contexte suffisamment grand
            n_gpu_layers=-1,  # Utiliser tous les layers GPU disponibles
            n_threads=4,  # Ajuster selon CPU
            verbose=False,
        )
        if config:
            self.session_state = config
        else:
            default_tempo = (
                126 if self.profile is None else self.profile["default_tempo"]
            )
            self.session_state = {
                "tempo": default_tempo,
                "current_tempo": default_tempo,
                "current_key": "C minor",
                "phase": "intro",
                "current_phase": "intro",
                "energy_level": 3,
                "active_samples": [],
                "session_duration": 0,
                "last_action_time": 0,
                "history": [],
            }

    def _build_prompt(self):
        """Construit le prompt pour le LLM basé sur l'état actuel"""
        if self.session_state.get("mode") == "live":
            return self._build_live_prompt()
        # Historique des dernières actions (limité)
        history_text = "\n".join(
            [
                f"- {action['action_type']}: {json.dumps(action['parameters'])}"
                for action in self.session_state["history"][-5:]  # 5 dernières actions
            ]
        )

        # Informations détaillées sur les layers actifs (avec leurs stems extraits)
        active_layers = self.session_state.get("active_layers", {})
        active_layers_details = []

        for layer_id, layer_info in active_layers.items():
            layer_type = layer_info.get("type", "unknown")
            layer_key = layer_info.get("key", "unknown")
            layer_volume = layer_info.get("volume", 0.8)

            # Information cruciale: le stem utilisé s'il a été extrait par Demucs
            used_stem = layer_info.get("used_stem")
            stem_info = f" (stem extrait: {used_stem})" if used_stem else ""

            spectral_profile = layer_info.get("spectral_profile", {})
            profile_info = ""
            if spectral_profile:
                # Sélectionner les composants principaux
                main_components = sorted(
                    spectral_profile.items(), key=lambda x: x[1], reverse=True
                )[
                    :2
                ]  # Les 2 composants les plus importants
                if main_components:
                    components_str = ", ".join(
                        [f"{k}: {v:.1%}" for k, v in main_components]
                    )
                    profile_info = f" [analyse spectrale: {components_str}]"

            active_layers_details.append(
                f"• {layer_id}: {layer_type}{stem_info}, tonalité: {layer_key}, volume: {layer_volume}{profile_info}"
            )

        active_layers_text = (
            "\n".join(active_layers_details)
            if active_layers_details
            else "Aucun layer actif"
        )

        # Construction du prompt utilisateur
        user_prompt = f"""
    État actuel du mix:
    - Tempo: {self.session_state.get('tempo', self.profile['default_tempo'])} BPM
    - Tonalité actuelle: {self.session_state.get('current_key', 'C minor')}
    - Phase: {self.session_state.get('phase', 'intro')}
    - Durée: {self.session_state.get('session_duration', 0)} secondes

    Layers actifs en détail:
    {active_layers_text}

    Historique récent:
    {history_text}

    Décide maintenant de la prochaine action pour maintenir un set cohérent.
    IMPORTANT: Prends en compte les stems extraits et les analyses spectrales pour tes décisions.
    Par exemple, si tu vois "stem extrait: drums" pour un layer précédent, ajuste ta stratégie en conséquence.
    Réponds UNIQUEMENT en format JSON comme spécifié.
    """
        return user_prompt

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
                {"role": "system", "content": self.system_prompt},
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

    def _build_live_prompt(self):
        """Construit le prompt pour le mode live"""

        # Informations temporelles
        time_elapsed = self.session_state.get("time_elapsed", 0)
        time_to_next_sample = self.session_state.get("time_to_next_sample", 30)
        average_generation_time = self.session_state.get("average_generation_time", 5)
        samples_generated = self.session_state.get("samples_generated", 0)

        # Informations sur le dernier sample généré
        last_sample = self.session_state.get("last_sample", {})
        last_sample_type = last_sample.get("type", "none") if last_sample else "none"
        last_sample_key = last_sample.get("key", "none") if last_sample else "none"

        # Historique des samples (les 3 derniers)
        history = self.session_state.get("samples_history", [])
        history_text = ""
        if history:
            for i, sample in enumerate(history[-3:]):
                if sample.get("decision"):
                    decision = sample["decision"]
                    params = decision.get("parameters", {}).get("sample_details", {})
                    history_text += f"- Sample #{sample.get('id')}: {params.get('type')} ({params.get('key', 'unknown')})\n"

        # Construction du prompt utilisateur pour le mode live
        user_prompt = f"""
        ÉTAT ACTUEL DU MIX LIVE:
        - Temps écoulé: {time_elapsed:.1f} secondes
        - Prochain sample généré dans: {time_to_next_sample:.1f} secondes
        - Samples générés jusqu'à présent: {samples_generated}
        - Temps moyen de génération: {average_generation_time:.1f} secondes

        Dernier sample généré:
        - Type: {last_sample_type}
        - Tonalité: {last_sample_key}

        Historique récent:
        {history_text}

        Tempo actuel: {self.session_state.get('current_tempo', 126)} BPM
        Tonalité actuelle: {self.session_state.get('current_key', 'C minor')}
        Phase actuelle: {self.session_state.get('current_phase', 'intro')}

        Génère maintenant un NOUVEAU sample qui s'intégrera bien avec le dernier sample généré.
        Pense à l'évolution naturelle du morceau en fonction de la phase actuelle et du temps écoulé.

        Réponds UNIQUEMENT en format JSON comme spécifié.
        """
        return user_prompt
