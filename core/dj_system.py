import os
import soundfile as sf
import time
from typing import Dict, Any

# import sounddevice as sd # Semble non utilisé, commenter si c'est le cas
import threading
import pygame

# Importer nos modules
from core.llm_interface import DJAILL
from core.music_generator import MusicGenerator

# from core.audio_processor import AudioProcessor # Semble non utilisé
from core.tts_engine import DJSpeech
from core.audio_simple import SimpleAudioPlayer  # Utilisé pour le speech, ok
from core.layer_manager import LayerManager
from config.dj_profiles import DJ_PROFILES

BEATS_PER_BAR = 4


class DJSystem:
    def __init__(self, args):
        # ... (début de ton __init__ inchangé) ...
        self.model_path = args.model_path
        self.profile_name = args.profile
        self.output_dir_base = (
            args.output_dir
        )  # Renommer pour éviter conflit avec celui du LayerManager
        self.sample_rate = 44100

        if self.profile_name not in DJ_PROFILES:
            # ... (gestion erreur profil) ...
            profiles = ", ".join(DJ_PROFILES.keys())
            raise ValueError(
                f"Profil inconnu: {self.profile_name}. Disponibles: {profiles}"
            )

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
        self.music_gen = MusicGenerator(model_size="medium")

        print("Initialisation du LayerManager...")
        self.layer_manager = LayerManager(
            sample_rate=self.sample_rate,
            output_dir=os.path.join(self.output_dir_base, "layers"),
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

        print("Système DJ-IA initialisé et prêt!")

    def _get_new_layer_id(self, prefix="layer") -> str:
        self.layer_id_counter += 1
        return f"{prefix}_{self.layer_id_counter}"

    def start_session(self):
        if self.session_running:
            print("Session déjà en cours")
            return
        self.session_running = True
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

    def _main_loop(self):
        try:
            print(
                f"Démarrage d'une session DJ '{self.profile_name}' à {self.layer_manager.master_tempo} BPM"
            )

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

            # Laisser le LLM prendre l'initiative dès le début
            print("\nEn attente de la première décision du DJ-IA...\n")

            while self.session_running:
                try:
                    if (
                        time.time() - last_decision_time
                        < min_interval_between_decisions
                    ):
                        time.sleep(
                            0.5
                        )  # Vérifier régulièrement si la session doit s'arrêter
                        print("En attente...")
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

    def _process_dj_decision(self, decision: Dict[str, Any]):
        action_type = decision.get("action_type", "")
        params = decision.get("parameters", {})
        reasoning = decision.get("reasoning", "N/A")

        print(f"\n🤖 Action LLM: {action_type}")
        print(f"💭 Raison: {reasoning}")
        print(f"⚙️  Paramètres: {params}\n")

        if action_type == "manage_layer":
            layer_id = params.get("layer_id")
            operation = params.get("operation")

            if not layer_id or not operation:
                print("Erreur de décision LLM: layer_id ou operation manquant.")
                return

            sample_details_from_llm = params.get("sample_details", {})
            playback_params_from_llm = params.get("playback_params", {})
            effects_from_llm = params.get("effects", [])
            stop_behavior_from_llm = params.get(
                "stop_behavior", "next_bar"
            )  # Pour l'opération "remove"

            if operation == "add_replace":
                sample_type = sample_details_from_llm.get("type", "techno_synth")
                musicgen_keywords = sample_details_from_llm.get(
                    "musicgen_prompt_keywords", [sample_type]
                )  # Utiliser type si keywords absents
                key = sample_details_from_llm.get(
                    "key", self.dj_brain.session_state["current_key"]
                )
                measures = sample_details_from_llm.get("measures", 2)
                intensity = sample_details_from_llm.get("intensity", 6)

                print(
                    f"🎛️  Génération MusicGen pour layer '{layer_id}':\n"
                    f"🎵 Type={sample_type}\n"
                    f"🔑 Keywords={musicgen_keywords}\n"
                    f"🎹 Key={key}\n"
                    f"📏 Measures={measures}\n"
                )

                try:
                    genre = None
                    if "techno" in self.profile_name:
                        genre = "techno"
                    elif (
                        "hip_hop" in self.profile_name or "hip-hop" in self.profile_name
                    ):
                        genre = "hip-hop"
                    elif "rock" in self.profile_name:
                        genre = "rock"
                    sample_audio, _ = self.music_gen.generate_sample(
                        sample_type=sample_type,  # Ce param est plus pour nous, MusicGen utilise surtout le prompt
                        tempo=self.layer_manager.master_tempo,
                        key=key,
                        intensity=intensity,  # Peut influencer la dynamique
                        musicgen_prompt_keywords=musicgen_keywords,
                        genre=genre,
                    )

                    original_file_path = os.path.join(
                        self.layer_manager.output_dir,
                        f"{layer_id}_orig_{int(time.time())}.wav",
                    )
                    self.music_gen.save_sample(sample_audio, original_file_path)

                    sample_details_for_manager = {
                        "original_file_path": original_file_path,
                        "measures": measures,
                        "type": sample_type,  # Pour info et mise à jour de l'état LLM
                        "key": key,
                    }
                    self.layer_manager.manage_layer(
                        layer_id,
                        operation,
                        sample_details_for_manager,
                        playback_params_from_llm,
                        effects_from_llm,
                    )
                    # Mettre à jour l'état pour le LLM après l'action réussie
                    if layer_id in self.layer_manager.layers:
                        self.dj_brain.session_state["active_layers"][layer_id] = {
                            "type": sample_type,
                            "key": key,
                            "volume": playback_params_from_llm.get("volume", 0.8),
                            "path": self.layer_manager.layers[
                                layer_id
                            ].file_path,  # Chemin du fichier traité/bouclé
                        }
                        self.dj_brain.session_state["current_key"] = (
                            key  # Mettre à jour la tonalité du set
                        )
                    else:  # Si le layer n'a pas pu être ajouté (ex: plus de canaux)
                        if layer_id in self.dj_brain.session_state["active_layers"]:
                            del self.dj_brain.session_state["active_layers"][layer_id]

                except Exception as e:
                    print(
                        f"Erreur pendant la génération ou gestion du layer '{layer_id}': {e}"
                    )

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

            else:
                print(
                    f"Opération '{operation}' sur layer non reconnue par DJSystem._process_dj_decision."
                )

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
