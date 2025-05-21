import os
import soundfile as sf
import time
from typing import Dict, Any
import threading
import pygame
import numpy as np
from core.llm_interface import DJAILL
from core.music_generator import MusicGenerator
from core.tts_engine import DJSpeech
from core.audio_simple import SimpleAudioPlayer
from core.layer_manager import LayerManager
from core.stems_manager import StemsManager
from config.dj_profiles import DJ_PROFILES

BEATS_PER_BAR = 4


class DJSystem:
    _instance = None

    @classmethod
    def get_instance(cls, *args, **kwargs):
        """Implémentation du pattern Singleton pour éviter les doubles initialisations"""
        if cls._instance is None:
            print("✨ Première initialisation du système DJ-IA (Singleton)...")
            cls._instance = cls(*args, **kwargs)
        else:
            print("♻️ Réutilisation de l'instance DJ-IA existante (Singleton)...")
        return cls._instance

    def __init__(self, args):
        if hasattr(self, "initialized") and self.initialized:
            print("⚠️ Tentative de réinitialisation ignorée - instance déjà initialisée")
            return
        self.model_path = args.model_path
        self.profile_name = args.profile
        self.output_dir_base = args.output_dir
        self.sample_rate = 44100
        self.audio_model = args.audio_model
        self.generation_duration = args.generation_duration
        if self.profile_name not in DJ_PROFILES:
            profiles = ", ".join(DJ_PROFILES.keys())
            raise ValueError(
                f"Profil inconnu: {self.profile_name}. Disponibles: {profiles}"
            )
        self.stems_manager = StemsManager()
        # Initialiser les composants
        print("Initialisation du système DJ-IA...")
        print(f"Chargement du profil {self.profile_name}...")
        # L'état initial du LLM doit refléter qu'il n'y a pas de layers au début
        initial_llm_state = {
            "current_tempo": DJ_PROFILES[self.profile_name].get("default_tempo", 126),
            "current_key": DJ_PROFILES[self.profile_name].get("default_key", "C minor"),
            "active_layers": {},  # Dictionnaire pour stocker les layers actifs avec leurs infos
            "set_phase": "intro",  # ou "warmup"
            "time_elapsed_beats": 0,  # Pour aider le LLM à structurer
        }
        self.dj_brain = DJAILL(self.model_path, self.profile_name, initial_llm_state)

        print("Chargement de MusicGen...")
        self.music_gen = MusicGenerator(
            model_name=self.audio_model, default_duration=self.generation_duration
        )

        print("Initialisation du LayerManager...")
        self.layer_manager = LayerManager(
            sample_rate=self.sample_rate,
            output_dir=os.path.join(self.output_dir_base, "layers"),
            on_max_layers_reached=self.handle_max_layers,
        )
        self.layer_manager.set_master_tempo(initial_llm_state["current_tempo"])

        print("Initialisation du moteur TTS...")
        self.speech_engine = DJSpeech()  # Renommé pour clarté
        self.speech_audio_player = SimpleAudioPlayer(
            sample_rate=self.sample_rate
        )  # Pour le speech

        self.session_running = False
        self.layer_id_counter = 0  # Pour générer des IDs uniques pour les layers

        # Créer le répertoire de sortie principal s'il n'existe pas
        if not os.path.exists(self.output_dir_base):
            os.makedirs(self.output_dir_base)

        self.initialized = True
        print("✅ Système DJ-IA initialisé avec succès (Singleton)")

    def reset_llm(self):
        """Réinitialise uniquement le LLM sans recréer tout le système"""
        if hasattr(self, "dj_brain"):
            print("🔄 Réinitialisation ciblée du LLM uniquement...")
            self.dj_brain._init_model()
            print("✅ LLM réinitialisé avec succès")
        else:
            print("⚠️ Impossible de réinitialiser le LLM - DJ Brain non initialisé")

    def _get_new_layer_id(self, prefix="layer") -> str:
        self.layer_id_counter += 1
        return f"{prefix}_{self.layer_id_counter}"

    def start_session(self):
        if self.session_running:
            print("Session déjà en cours")
            return
        self.session_running = True
        print("")
        print(
            "🎧 Session DJ-IA démarrée sur le profil '%s' à %d BPM"
            % (self.profile_name, self.layer_manager.master_tempo)
        )

        self.speech_audio_player.start()

        self.dj_thread = threading.Thread(target=self._main_loop)
        self.dj_thread.daemon = True
        self.dj_thread.start()

    def stop_session(self):
        if not self.session_running:
            print("Aucune session en cours")
            return
        print("Arrêt de la session...")
        self.session_running = False

        self.layer_manager.stop_all_layers(
            fade_ms=500
        )  # Arrêter tous les layers proprement
        self.speech_audio_player.stop()  # Assumer que SimpleAudioPlayer a une méthode stop

        if hasattr(self, "dj_thread") and self.dj_thread.is_alive():
            print("Attente de la fin du thread DJ...")
            self.dj_thread.join(timeout=5)
            if self.dj_thread.is_alive():
                print("Le thread DJ n'a pas pu être arrêté proprement.")

        if pygame.mixer.get_init():
            pygame.mixer.quit()  # Quitter Pygame Mixer à la fin de la session globale
        print("Session DJ terminée.")

    def _adjust_effects_for_layering(
        self, original_effects, new_profile, existing_profiles
    ):
        """
        Ajuste les effets pour éviter les chevauchements spectraux entre les layers

        Args:
            original_effects (list): Liste des effets originaux demandés par le LLM
            new_profile (dict): Profil spectral du nouveau sample (énergie relative par instrument)
            existing_profiles (dict): Dictionnaire des profils spectraux des layers existants

        Returns:
            list: Liste d'effets ajustée pour éviter les chevauchements
        """
        # Toujours créer une copie pour ne pas modifier l'original
        effects = original_effects.copy() if original_effects else []

        # Vérifier si des effets du même type existent déjà
        def has_effect_type(effect_list, effect_type):
            return any(effect.get("type") == effect_type for effect in effect_list)

        # Fonction pour trouver l'effet existant d'un certain type
        def find_effect(effect_list, effect_type):
            for i, effect in enumerate(effect_list):
                if effect.get("type") == effect_type:
                    return i, effect
            return -1, None

        # Si aucun layer existant, pas besoin d'ajustements
        if not existing_profiles:
            return effects

        # Comptabiliser le nombre de layers avec forte présence pour chaque élément
        strong_elements = {
            "bass": 0,
            "drums": 0,
            "vocals": 0,
            "piano": 0,
            "guitar": 0,
            "other": 0,
        }

        for profile in existing_profiles.values():
            if profile.get("bass", 0) > 0.15:
                strong_elements["bass"] += 1
            if profile.get("drums", 0) > 0.2:
                strong_elements["drums"] += 1
            if profile.get("vocals", 0) > 0.15:
                strong_elements["vocals"] += 1
            if profile.get("piano", 0) > 0.15:
                strong_elements["piano"] += 1
            if profile.get("guitar", 0) > 0.15:
                strong_elements["guitar"] += 1
            if profile.get("other", 0) > 0.25:
                strong_elements["other"] += 1

        print(
            f"\n📊 Analyse spectrale - Éléments forts détectés dans le mix: {strong_elements}"
        )
        print(f"🔬 Profil spectral du nouveau layer: {new_profile}")
        print("")
        # 1. Traitement de la BASSE
        if new_profile.get("bass", 0) > 0.2:  # Si le nouveau sample a une basse forte
            if strong_elements["bass"] > 0:
                # Basse déjà présente, appliquer filtre passe-haut pour éviter chevauchement
                # IMPORTANT: Limiter la fréquence de coupure à des valeurs raisonnables
                cutoff_basse = min(120 + (strong_elements["bass"] * 20), 160)

                if has_effect_type(effects, "hpf"):
                    # Ajuster HPF existant
                    idx, hpf = find_effect(effects, "hpf")
                    current_cutoff = hpf.get("cutoff_hz", 20)
                    if current_cutoff < 40:  # Si le filtre est trop bas
                        effects[idx]["cutoff_hz"] = cutoff_basse
                        print(
                            f"Ajustement du HPF existant à {effects[idx]['cutoff_hz']}Hz pour éviter chevauchement de basses"
                        )
                else:
                    # Ajouter un nouveau HPF avec une fréquence raisonnable
                    effects.append({"type": "hpf", "cutoff_hz": cutoff_basse})
                    print(
                        f"🔉 Ajout d'un HPF à {cutoff_basse}Hz pour éviter chevauchement de basses"
                    )

        # 2. Traitement des DRUMS/PERCUSSIONS
        if (
            new_profile.get("drums", 0) > 0.25
        ):  # Si le nouveau sample a des percussions fortes
            if strong_elements["drums"] > 0:
                # Percussions déjà présentes, appliquer EQ ou compression
                if not has_effect_type(effects, "lpf"):
                    # Ajouter un filtre passe-bas pour adoucir les aigus des percussions
                    cutoff = 8000 - (
                        strong_elements["drums"] * 500
                    )  # Réduction moins agressive
                    effects.append({"type": "lpf", "cutoff_hz": cutoff})
                    print(
                        f"🔉 Ajout d'un LPF à {cutoff}Hz pour adoucir les nouvelles percussions"
                    )

                # CRUCIAL: Si c'est un kick drum, NE PAS ajouter de HPF élevé
                if "kick" in new_profile.get("type", "").lower():
                    # Pour les kicks, on veut un HPF très bas (20-40Hz)
                    if has_effect_type(effects, "hpf"):
                        idx, hpf = find_effect(effects, "hpf")
                        if hpf.get("cutoff_hz", 20) > 50:
                            effects[idx][
                                "cutoff_hz"
                            ] = 30  # Valeur conservatrice pour un kick
                            print(
                                f"🔉 Réduction du HPF à 30Hz pour préserver l'impact du kick"
                            )

                # Appliquer une réduction fixe basée sur le nombre de drums existants
                # Mais pas trop forte pour conserver la présence
                reduction = -0.1 * min(strong_elements["drums"], 2)  # Limite à -0.2 max
                for i, effect in enumerate(effects):
                    if effect.get("type") == "volume":
                        # Remplacement plutôt qu'addition
                        effects[i]["gain"] = reduction
                        break
                else:
                    effects.append({"type": "volume", "gain": reduction})
                    print(f"🔉 Réduction du volume des percussions de {-reduction} dB")

        # 3. Traitement des VOCAUX
        if (
            new_profile.get("vocals", 0) > 0.2
        ):  # Si le nouveau sample a des vocaux significatifs
            if strong_elements["vocals"] > 0:
                # Vocaux déjà présents, ajouter plus de reverb/delay pour créer de l'espace
                has_reverb = has_effect_type(effects, "reverb")
                if not has_reverb:
                    effects.append(
                        {
                            "type": "reverb",
                            "size": "large",
                            "decay": min(3.0 + (strong_elements["vocals"] * 0.5), 5.0),
                            "wet": 0.4 + (strong_elements["vocals"] * 0.1),
                        }
                    )
                    print(f"🔉 Ajout de reverb pour créer de l'espace entre les vocaux")

        # 4. Traitement du PIANO
        if new_profile.get("piano", 0) > 0.2:
            if strong_elements["piano"] > 0:
                # Pianos déjà présents, appliquer un filtre de modulation pour différencier
                if not has_effect_type(effects, "lpf") and not has_effect_type(
                    effects, "hpf"
                ):
                    # Ajouter un filtre passe-bande pour isoler certaines fréquences
                    effects.append({"type": "hpf", "cutoff_hz": 300})
                    effects.append({"type": "lpf", "cutoff_hz": 5000})
                    print(
                        f"🔉 Ajout de filtres pour définir une plage fréquentielle unique pour le piano"
                    )

        # 5. Traitement de la GUITARE
        if new_profile.get("guitar", 0) > 0.2:
            if strong_elements["guitar"] > 0:
                # Guitares déjà présentes, pan différent pour séparation spatiale
                pan_values = [
                    e.get("pan", 0)
                    for e in [
                        p.get("playback_params", {}) for p in existing_profiles.values()
                    ]
                ]
                if pan_values:
                    avg_pan = sum(pan_values) / len(pan_values)
                    # Suggérer un pan opposé
                    print(
                        f"Suggestion pour séparation spatiale : placer guitare à pan={-avg_pan}"
                    )

                # Ajouter un filtre passe-bande pour séparer des autres guitares
                if not has_effect_type(effects, "hpf"):
                    effects.append(
                        {
                            "type": "hpf",
                            "cutoff_hz": 500 + (strong_elements["guitar"] * 100),
                        }
                    )
                    print(
                        f"🔉 Ajout d'un HPF pour séparer la guitare des autres instruments similaires"
                    )

        # 6. Traitement des AUTRES éléments
        if new_profile.get("other", 0) > 0.3:  # Si beaucoup d'éléments "autres"
            if (
                strong_elements["other"] > 1
            ):  # Si déjà plusieurs layers avec beaucoup d'éléments divers
                # Ajouter plus de filtrage pour éviter l'encombrement
                if not has_effect_type(effects, "hpf") and not has_effect_type(
                    effects, "lpf"
                ):
                    # Créer une "fenêtre" de fréquences moins occupée
                    mid_freqs = [200, 500, 1000, 2000, 4000]
                    selected_low = mid_freqs[
                        min(strong_elements["other"] - 1, len(mid_freqs) - 1)
                    ]
                    selected_high = selected_low * 3

                    effects.append({"type": "hpf", "cutoff_hz": selected_low})
                    effects.append({"type": "lpf", "cutoff_hz": selected_high})
                    print(
                        f"🔉 Création d'une fenêtre fréquentielle {selected_low}-{selected_high}Hz pour le nouveau layer"
                    )

        # 7. Traitement global pour éviter la saturation
        total_energy_existing = sum(
            sum(profile.values()) for profile in existing_profiles.values()
        )
        total_energy_new = sum(new_profile.values())

        # Si l'énergie totale dépasse un certain seuil, réduire le volume global
        if total_energy_existing > 1.5 and total_energy_new > 0.8:
            # Limiter la réduction à des valeurs raisonnables
            volume_reduction = min(
                0.3, (total_energy_existing + total_energy_new - 1.5) * 0.15
            )

            if has_effect_type(effects, "volume"):
                idx, vol_effect = find_effect(effects, "volume")
                # Ici on ADDITIONNE pour ne pas perdre l'effet précédent
                effects[idx]["gain"] = vol_effect.get("gain", 0.0) - volume_reduction
            else:
                effects.append({"type": "volume", "gain": -volume_reduction})

            print(
                f"🔈 Réduction du volume global de {volume_reduction} dB pour éviter la saturation du mix"
            )

        # 8. Correction de toute erreur de syntaxe dans les effets
        for effect in effects:
            # Corriger les fautes d'orthographe courantes
            if effect.get("type") == "compression":
                if "threshhold" in effect:
                    effect["threshold"] = effect.pop("threshhold")
                    print("🛠️ Correction: 'threshhold' → 'threshold'")
                if "thresshold" in effect:
                    effect["threshold"] = effect.pop("thresshold")
                    print("🛠️ Correction: 'thresshold' → 'threshold'")

        print(f"🎛️  Effets ajustés pour le layering: {effects}")
        return effects

    def _main_loop(self):
        try:

            # S'assurer que les dictionnaires nécessaires sont initialisés
            if "active_layers" not in self.dj_brain.session_state:
                self.dj_brain.session_state["active_layers"] = {}

            if "time_elapsed_beats" not in self.dj_brain.session_state:
                self.dj_brain.session_state["time_elapsed_beats"] = 0

            # Informer le LLM qu'il doit commencer le set
            self.dj_brain.session_state["set_phase"] = "intro"

            # Boucle principale pour les décisions
            last_decision_time = time.time()
            min_interval_between_decisions = (
                1  # secondes, pour éviter que le LLM ne spamme MusicGen
            )

            while self.session_running:
                try:
                    if (
                        time.time() - last_decision_time
                        < min_interval_between_decisions
                    ):
                        time.sleep(
                            0.5
                        )  # Vérifier régulièrement si la session doit s'arrêter
                        continue

                    # Vérifier que les structures nécessaires existent
                    if "active_layers" not in self.dj_brain.session_state:
                        self.dj_brain.session_state["active_layers"] = {}

                    if "time_elapsed_beats" not in self.dj_brain.session_state:
                        self.dj_brain.session_state["time_elapsed_beats"] = 0

                    # Mettre à jour le temps écoulé pour le LLM (approximatif)
                    if self.layer_manager.global_playback_start_time:
                        elapsed_session_time = (
                            time.time() - self.layer_manager.global_playback_start_time
                        )
                        self.dj_brain.session_state["time_elapsed_beats"] = int(
                            elapsed_session_time
                            / (60.0 / self.layer_manager.master_tempo)
                        )

                    # Déterminer combien de layers sont réellement actifs actuellement
                    # (plutôt que le nombre cumulatif de layers depuis le début)
                    current_active_layers_count = len(self.layer_manager.layers)

                    # Sécuriser l'accès aux clés du dictionnaire
                    current_key = self.dj_brain.session_state.get(
                        "current_key", "C minor"
                    )
                    set_phase = self.dj_brain.session_state.get("set_phase", "intro")
                    active_layers = self.dj_brain.session_state.get("active_layers", {})

                    print(
                        f"\n🎛️  État actuel pour LLM:\n"
                        f"🎵 Tempo={self.layer_manager.master_tempo} BPM\n"
                        f"🎹 Key={current_key}\n"
                        f"📊 Phase={set_phase}\n"
                        f"🔊 Layers actifs={current_active_layers_count}/3\n"
                    )

                    if self.dj_brain.session_state.get("need_layer_removal", False):
                        print(
                            "⚠️  ALERTE: Trop de layers actifs! DJ-AI doit supprimer un layer!"
                        )
                        print(
                            f"🗑️  Layers disponibles pour suppression: {list(self.dj_brain.session_state.get('layers_to_choose_from', {}).keys())}"
                        )
                        self.dj_brain.session_state["need_layer_removal"] = False

                    # Dans _main_loop, après avoir calculé current_active_layers_count
                    if current_active_layers_count == 2:
                        print("ℹ️  INFO: Déjà 2 layers actifs sur 3 possibles.")
                        # Ajouter un état visible par le LLM pour le rendre conscient de la situation
                        self.dj_brain.session_state["approaching_max_layers"] = True
                        self.dj_brain.session_state["current_layers_count"] = 2
                        self.dj_brain.session_state["max_layers_allowed"] = 3
                        print(
                            f"🔊 Layers actifs: {list(self.layer_manager.layers.keys())}"
                        )
                    elif current_active_layers_count < 2:
                        # Réinitialiser la clé si on repasse en dessous de 2 layers
                        if "approaching_max_layers" in self.dj_brain.session_state:
                            self.dj_brain.session_state["approaching_max_layers"] = (
                                False
                            )

                    decision = self.dj_brain.get_next_decision()
                    last_decision_time = time.time()
                    self._process_dj_decision(decision)

                except Exception as e:
                    print(f"Erreur dans la boucle principale du DJ: {e}")
                    import traceback

                    traceback.print_exc()
                    time.sleep(5)  # Attendre un peu avant de réessayer

        except Exception as e:
            print(f"Erreur critique dans _main_loop: {e}")
            import traceback

            traceback.print_exc()
            self.session_running = False

        print("Fin de la boucle principale du DJ (session_running est False).")

    def handle_max_layers(self, layer_info):
        # Mettre à jour l'état du LLM
        self.dj_brain.session_state["need_layer_removal"] = True
        self.dj_brain.session_state["layers_to_choose_from"] = layer_info
        print("LLM informé qu'il doit supprimer un layer à sa prochaine action.")
        return True

    def prepare_sample_details(self, sample_details_from_llm, layer_id):
        """Prépare les détails du sample à générer"""
        sample_type = sample_details_from_llm.get("type", "techno_synth")

        # Modification pour accepter une liste de stems ou une chaîne
        preferred_stems = sample_details_from_llm.get("preferred_stems", None)
        if preferred_stems is None:
            # Rétrocompatibilité avec l'ancien format
            preferred_stems = sample_details_from_llm.get("preferred_stem", None)

        musicgen_keywords = sample_details_from_llm.get(
            "musicgen_prompt_keywords", [sample_type]
        )
        key = sample_details_from_llm.get(
            "key", self.dj_brain.session_state["current_key"]
        )
        measures = sample_details_from_llm.get("measures", 2)
        intensity = sample_details_from_llm.get("intensity", 6)

        stems_str = preferred_stems
        if isinstance(preferred_stems, list):
            stems_str = ", ".join(preferred_stems)

        print(
            f"🎛️  Génération MusicGen pour layer '{layer_id}':\n"
            f"🎵 Type={sample_type}\n"
            f"🔑 Keywords={musicgen_keywords}\n"
            f"🎹 Key={key}\n"
            f"📏 Measures={measures}\n"
            f"🎯 Stems préférés={stems_str if stems_str else 'auto'}\n"
        )
        return (
            sample_type,
            preferred_stems,  # Maintenant peut être une liste ou une chaîne
            musicgen_keywords,
            key,
            measures,
            intensity,
        )

    def determine_genre(self):
        """Détermine le genre musical en fonction du profil DJ"""
        if "techno" in self.profile_name:
            return "techno"
        elif "hip_hop" in self.profile_name or "hip-hop" in self.profile_name:
            return "hip-hop"
        elif "rock" in self.profile_name:
            return "rock"
        elif "classical" in self.profile_name:
            return "classical"
        elif "ambient" in self.profile_name or "downtempo" in self.profile_name:
            return "ambient"
        elif "dub" in self.profile_name:
            return "dub"
        elif "jungle" in self.profile_name or "dnb" in self.profile_name:
            return "jungle_dnb"
        elif "house" in self.profile_name:
            return "deep_house"
        elif "trip_hop" in self.profile_name:
            return "trip-hop"
        return None

    def _process_dj_decision(self, decision: Dict[str, Any]):
        action_type = decision.get("action_type", "")
        params = decision.get("parameters", {})
        reasoning = decision.get("reasoning", "N/A")

        print(f"\n🤖 Action LLM: {action_type}")
        print(f"💭 Raison: {reasoning}")
        print(f"\n⚙️  Paramètres: {params}\n")

        if action_type == "manage_layer":
            layer_id = params.get("layer_id")
            operation = params.get("operation")

            if not layer_id or not operation:
                print("Erreur de décision LLM: layer_id ou operation manquant.")
                return

            if operation == "remove":
                if len(self.layer_manager.layers) <= 1:
                    print(
                        "⚠️ Tentative de suppression du dernier layer actif! Opération annulée."
                    )
                    print("⚠️ Il doit toujours y avoir au moins un layer actif.")
                    # Informer le LLM qu'il doit ajouter un nouveau layer avant d'en supprimer
                    self.dj_brain.session_state["must_add_layer_first"] = True
                    return

            sample_details_from_llm = params.get("sample_details", {})
            playback_params_from_llm = params.get("playback_params", {})
            effects_from_llm = params.get("effects", [])
            stop_behavior_from_llm = params.get(
                "stop_behavior", "next_bar"
            )  # Pour l'opération "remove"

            if operation == "add_replace":
                # Phase 1: Préparation et génération du sample
                try:
                    # Préparation des paramètres
                    (
                        sample_type,
                        preferred_stems,
                        musicgen_keywords,
                        key,
                        measures,
                        intensity,
                    ) = self.prepare_sample_details(sample_details_from_llm, layer_id)
                    genre = self.determine_genre()

                    # Génération du sample audio
                    sample_audio, _ = self.music_gen.generate_sample(
                        sample_type=sample_type,
                        tempo=self.layer_manager.master_tempo,
                        key=key,
                        intensity=intensity,
                        musicgen_prompt_keywords=musicgen_keywords,
                        genre=genre,
                    )

                    # Sauvegarde du sample original
                    original_file_path = os.path.join(
                        self.layer_manager.output_dir,
                        f"{layer_id}_orig_{int(time.time())}.wav",
                    )
                    self.music_gen.save_sample(sample_audio, original_file_path)

                    # Phase 2: Analyse spectrale et sélection de stem
                    print(
                        f"🔍 Analyse spectrale du sample avec Demucs pour optimisation du mixage..."
                    )

                    # Variables par défaut en cas d'échec
                    spectral_profile = None
                    file_to_process = original_file_path
                    used_stem_type = None
                    adjusted_effects = effects_from_llm

                    try:
                        # Analyse avec Demucs
                        temp_analysis_dir = os.path.join(
                            self.layer_manager.output_dir, "temp_analysis"
                        )
                        os.makedirs(temp_analysis_dir, exist_ok=True)
                        spectral_profile, separated_stems_path = (
                            self.stems_manager._analyze_sample_with_demucs(
                                original_file_path, temp_analysis_dir
                            )
                        )
                        print(f"📊 Profil spectral détecté: {spectral_profile}")

                        # Phase 3: Sélection du stem basée sur la préférence du LLM
                        if spectral_profile and separated_stems_path:
                            # Utilisation de la nouvelle méthode qui accepte plusieurs stems
                            file_to_process, used_stems = (
                                self.stems_manager._extract_multiple_stems(
                                    spectral_profile,
                                    separated_stems_path,
                                    layer_id,
                                    preferred_stems,  # Peut être une liste ou une chaîne
                                )
                            )

                            # Si l'extraction a échoué, utiliser le fichier original
                            if not file_to_process:
                                file_to_process = original_file_path
                                used_stems = None
                            else:
                                # Adapter le type du sample en fonction des stems extraits
                                if used_stems:
                                    if len(used_stems) == 1:
                                        # Un seul stem,
                                        stem_type = used_stems[0]
                                        refined_sample_type = f"{stem_type}_{sample_type.split('_')[-1] if '_' in sample_type else 'element'}"
                                    else:
                                        # Plusieurs stems, créer un type combiné
                                        stem_types = "_".join(used_stems)
                                        refined_sample_type = f"mixed_{stem_types}_{sample_type.split('_')[-1] if '_' in sample_type else 'element'}"

                                    print(
                                        f"\n🧠 DJ-IA raffine: Type de sample ajusté de '{sample_type}' à '{refined_sample_type}'"
                                    )
                                    sample_type = refined_sample_type

                        # Phase 4: Ajustement des effets
                        # Récupérer les profils spectraux des layers existants
                        existing_profiles = {}
                        for existing_id, layer in self.layer_manager.layers.items():
                            if hasattr(layer, "spectral_profile"):
                                existing_profiles[existing_id] = layer.spectral_profile
                            else:
                                default_profile = (
                                    self.stems_manager.create_default_profile(
                                        getattr(layer, "type", "unknown")
                                    )
                                )
                                existing_profiles[existing_id] = default_profile
                        print("")
                        # Ajuster les effets pour éviter les chevauchements
                        adjusted_effects = self._adjust_effects_for_layering(
                            effects_from_llm, spectral_profile, existing_profiles
                        )

                        # Ajouter des effets spécifiques pour le stem sélectionné
                        if used_stem_type:
                            if used_stem_type == "bass":
                                print(
                                    f"🔊 Optimisation basse: Ajout d'un compresseur pour plus de punch"
                                )
                                adjusted_effects.append(
                                    {
                                        "type": "compression",
                                        "threshold": -20,
                                        "ratio": 4,
                                    }
                                )
                            elif used_stem_type == "drums":
                                print(
                                    f"🥁 Optimisation batterie: Égalisation pour plus de présence"
                                )
                                # Effets spécifiques pour batterie
                            elif used_stem_type == "vocals":
                                print(
                                    f"🎤 Optimisation vocale: Ajout de reverb et delay"
                                )
                                adjusted_effects.append(
                                    {
                                        "type": "reverb",
                                        "decay_time": "2.5s",
                                        "wet": 0.35,
                                    }
                                )

                        print(f"🎛️  Effets optimisés pour le mixage: {adjusted_effects}")

                    except Exception as e:
                        print(
                            f"⚠️  Erreur lors de l'analyse/extraction: {e}. Poursuite avec les effets originaux."
                        )
                        import traceback

                        traceback.print_exc()

                        # En cas d'échec, utiliser le profil par défaut
                        spectral_profile = self.stems_manager.create_default_profile(
                            sample_type
                        )

                    # Phase 5: Création et gestion du layer final
                    print(
                        f"🔄 Fichier final utilisé: {os.path.basename(file_to_process)}"
                    )

                    # Préparation pour le layer manager
                    sample_details_for_manager = {
                        "original_file_path": file_to_process,
                        "measures": measures,
                        "type": sample_type,
                        "key": key,
                        "used_stem": used_stem_type,
                    }

                    # Gérer le layer avec les effets ajustés
                    self.layer_manager.manage_layer(
                        layer_id,
                        operation,
                        sample_details_for_manager,
                        playback_params_from_llm,
                        adjusted_effects,
                    )

                    # Mise à jour des états et informations du layer
                    if layer_id in self.layer_manager.layers:
                        # Stocker les informations avec le layer
                        self.layer_manager.layers[layer_id].spectral_profile = (
                            spectral_profile
                        )
                        if used_stem_type:
                            self.layer_manager.layers[layer_id].used_stem = (
                                used_stem_type
                            )

                        # Mettre à jour l'état pour le LLM
                        self.dj_brain.session_state["active_layers"][layer_id] = {
                            "type": sample_type,
                            "key": key,
                            "volume": playback_params_from_llm.get("volume", 0.8),
                            "path": self.layer_manager.layers[layer_id].file_path,
                            "spectral_profile": spectral_profile,
                            "used_stem": used_stem_type,
                        }
                        self.dj_brain.session_state["current_key"] = key

                        # Informer sur l'optimisation
                        if used_stem_type:
                            print(
                                f"\n💡 Sample optimisé en utilisant uniquement le stem '{used_stem_type}'"
                            )
                    else:
                        # Si le layer n'a pas pu être ajouté
                        if layer_id in self.dj_brain.session_state["active_layers"]:
                            del self.dj_brain.session_state["active_layers"][layer_id]

                except Exception as e:
                    print(
                        f"❌ Erreur pendant la génération ou gestion du layer '{layer_id}': {e}"
                    )
                    import traceback

                    traceback.print_exc()

            elif operation == "modify":
                self.layer_manager.manage_layer(
                    layer_id,
                    operation,
                    playback_params=playback_params_from_llm,
                    effects=effects_from_llm,
                )
                # Mettre à jour l'état pour le LLM
                if (
                    layer_id in self.dj_brain.session_state["active_layers"]
                    and playback_params_from_llm.get("volume") is not None
                ):
                    self.dj_brain.session_state["active_layers"][layer_id]["volume"] = (
                        playback_params_from_llm.get("volume")
                    )

            elif operation == "remove":
                self.layer_manager.manage_layer(
                    layer_id, operation, stop_behavior=stop_behavior_from_llm
                )
                # Mettre à jour l'état pour le LLM
                if layer_id in self.dj_brain.session_state["active_layers"]:
                    del self.dj_brain.session_state["active_layers"][layer_id]

                # Réinitialiser le flag si présent
                if self.dj_brain.session_state.get("need_layer_removal", False):
                    # Le LLM n'a pas supprimé de layer alors qu'on le lui a demandé
                    print(
                        "⚠️ ALERTE: Le LLM n'a pas supprimé de layer alors qu'il y a déjà 3 layers actifs!"
                    )

                    # Choisir un layer à supprimer automatiquement
                    layers_to_remove = list(self.layer_manager.layers.keys())
                    if layers_to_remove:
                        layer_to_remove_id = layers_to_remove[
                            0
                        ]  # Prendre le premier par simplicité
                        print(
                            f"🚨 Suppression automatique du layer '{layer_to_remove_id}' pour respecter la limite"
                        )

                        # Supprimer le layer
                        layer_to_remove = self.layer_manager.layers.pop(
                            layer_to_remove_id
                        )
                        layer_to_remove.stop(fadeout_ms=200, cleanup=True)

                        # Mettre à jour l'état pour le LLM
                        if (
                            layer_to_remove_id
                            in self.dj_brain.session_state["active_layers"]
                        ):
                            del self.dj_brain.session_state["active_layers"][
                                layer_to_remove_id
                            ]

                        # Réinitialiser le flag
                        self.dj_brain.session_state["need_layer_removal"] = False

                    else:
                        print(
                            f"Opération '{operation}' sur layer non reconnue par DJSystem._process_dj_decision."
                        )
            if self.dj_brain.session_state.get("need_layer_removal", False):
                # Le LLM n'a pas supprimé de layer alors qu'on le lui a demandé
                print(
                    "⚠️ ALERTE: Le LLM n'a pas supprimé de layer alors qu'il y a déjà 3 layers actifs!"
                )

                # Choisir un layer à supprimer automatiquement
                layers_to_remove = list(self.layer_manager.layers.keys())
                if layers_to_remove:
                    layer_to_remove_id = layers_to_remove[
                        0
                    ]  # Prendre le premier par simplicité
                    print(
                        f"🚨 Suppression automatique du layer '{layer_to_remove_id}' pour respecter la limite"
                    )

                    # Supprimer le layer
                    layer_to_remove = self.layer_manager.layers.pop(layer_to_remove_id)
                    layer_to_remove.stop(fadeout_ms=200, cleanup=True)

                    # Mettre à jour l'état pour le LLM
                    if (
                        layer_to_remove_id
                        in self.dj_brain.session_state["active_layers"]
                    ):
                        del self.dj_brain.session_state["active_layers"][
                            layer_to_remove_id
                        ]

                    # Réinitialiser le flag
                    self.dj_brain.session_state["need_layer_removal"] = False

        elif action_type == "speech":
            text_to_say = params.get("text", "Let's go!")
            energy = params.get("energy", 5)
            print(f'DJ Speech: "{text_to_say}" (Energy: {energy})')
            try:
                speech_audio, speech_sr = self.speech_engine.generate_speech(
                    text=text_to_say, energy_level=energy
                )
                speech_filename = os.path.join(
                    self.output_dir_base, "speech", f"speech_{int(time.time())}.wav"
                )
                os.makedirs(os.path.join(self.output_dir_base, "speech"), exist_ok=True)
                sf.write(speech_filename, speech_audio, speech_sr)
                self.speech_audio_player.play_file(
                    speech_filename
                )  # Joue sur son propre lecteur, non bloquant
            except Exception as e:
                print(f"Erreur lors de la génération/lecture du speech: {e}")

        elif action_type == "set_tempo":
            new_tempo = params.get("new_tempo")
            if new_tempo:
                self.layer_manager.set_master_tempo(float(new_tempo))
                self.dj_brain.session_state["current_tempo"] = float(new_tempo)
                print(f"Tempo du set mis à jour à {new_tempo} BPM.")

        elif (
            action_type == "set_key"
        ):  # Si tu veux que le LLM change la tonalité globale
            new_key = params.get("new_key")
            if new_key:
                self.dj_brain.session_state["current_key"] = new_key
                print(f"Tonalité de référence du set mise à jour à {new_key}.")

        elif (
            action_type == "set_phase"
        ):  # Pour informer le LLM de sa propre progression
            new_phase = params.get("new_phase")
            if new_phase:
                self.dj_brain.session_state["set_phase"] = new_phase
                print(f"Phase du set mise à jour à '{new_phase}'.")

        else:
            print(f"Type d'action LLM inconnu: {action_type}")
