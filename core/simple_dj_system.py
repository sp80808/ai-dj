import os
import soundfile as sf
import time
import librosa

# import sounddevice as sd # Semble non utilisé, commenter si c'est le cas
import threading
import pygame
import numpy as np

# Importer nos modules
from core.llm_interface import DJAILL
from core.music_generator import MusicGenerator

# from core.audio_processor import AudioProcessor # Semble non utilisé
from core.tts_engine import DJSpeech
from core.audio_simple import SimpleAudioPlayer  # Utilisé pour le speech, ok
from config.dj_profiles import DJ_PROFILES

BEATS_PER_BAR = 4


class SimpleDJSystem:
    """Système principal du DJ-IA"""

    def __init__(self, args):
        self.model_path = args.model_path
        self.profile_name = args.profile
        self.output_dir = args.output_dir
        self.sample_rate = 44100
        # self.audio_queue = queue.Queue(maxsize=10) # Lié à audio_processor, commenter si non utilisé
        # self.audio_thread = None # Lié à audio_processor
        self.chunk_size = 4096  # Lié à audio_processor

        self.current_sample_path = None  # Garder une trace du sample en cours
        self.current_sample_length_seconds = 0
        self.playback_start_time = 0
        self.is_playing = False

        if self.profile_name not in DJ_PROFILES:
            profiles = ", ".join(DJ_PROFILES.keys())
            raise ValueError(
                f"Profil inconnu: {self.profile_name}. Disponibles: {profiles}"
            )

        print("Initialisation du système DJ-IA...")
        print(f"Chargement du profil {self.profile_name}...")
        self.dj_brain = DJAILL(self.model_path, self.profile_name, {})

        print("Chargement de MusicGen...")
        self.music_gen = MusicGenerator(model_size="medium")

        print("Initialisation du lecteur audio pour le speech...")
        self.speech_audio_player = SimpleAudioPlayer(
            sample_rate=self.sample_rate
        )  # Renommé pour clarté

        print("Initialisation du moteur TTS...")
        self.speech = DJSpeech()

        self.session_running = False
        self.tempo = DJ_PROFILES[self.profile_name].get(
            "default_tempo", 126
        )  # S'assurer d'avoir un tempo

        if not os.path.exists(self.output_dir):
            os.makedirs(self.output_dir)

        self.initialize_pygame_mixer()  # Initialiser Pygame Mixer ici

        print("Système DJ-IA initialisé et prêt!")

    def initialize_pygame_mixer(self):
        """Initialise Pygame Mixer pour la lecture des samples en boucle."""
        pygame.mixer.init(frequency=self.sample_rate)  # Spécifier le sample rate
        print("Pygame Mixer initialisé pour les samples.")

    def prepare_sample_for_loop(self, audio_path, measures=2):
        """Prépare un sample pour qu'il boucle parfaitement sur un nombre de mesures donné."""
        try:
            audio, sr = librosa.load(audio_path, sr=self.sample_rate)
        except Exception as e:
            print(f"Erreur de chargement du sample {audio_path} avec librosa: {e}")
            return None

        if sr != self.sample_rate:
            print(
                f"Attention: Sample rate du fichier ({sr}Hz) différent de celui de la session ({self.sample_rate}Hz). Resampling..."
            )
            audio = librosa.resample(audio, orig_sr=sr, target_sr=self.sample_rate)
            sr = self.sample_rate

        beats_per_second = self.tempo / 60.0
        seconds_per_beat = 1.0 / beats_per_second
        samples_per_beat = int(seconds_per_beat * sr)
        samples_per_bar = samples_per_beat * BEATS_PER_BAR
        total_target_samples = samples_per_bar * measures

        # Détection du premier kick (ou transitoire fort)
        onset_env = librosa.onset.onset_strength(y=audio, sr=sr)
        # onset_frames = librosa.onset.onset_detect(onset_envelope=onset_env, sr=sr, units='samples', backtrack=False)
        # Utiliser des temps pour la détection d'onset pour plus de robustesse
        onsets_time = librosa.onset.onset_detect(
            onset_envelope=onset_env, sr=sr, units="time"
        )

        if len(onsets_time) > 0:
            # Tenter de trouver un onset proche du début qui pourrait être le kick
            # On va privilégier un onset dans la première demi-seconde par exemple
            first_kick_onset_sample = 0
            possible_onsets_samples = librosa.time_to_samples(onsets_time, sr=sr)

            # Chercher un onset dans la première partie du sample (ex: premier quart de seconde)
            # Ceci est une heuristique, pourrait être amélioré
            early_onsets = [s for s in possible_onsets_samples if s < sr * 0.25]
            if early_onsets:
                first_kick_onset_sample = early_onsets[0]
            elif (
                possible_onsets_samples.size > 0
            ):  # Si pas d'onset très tôt, prendre le premier détecté
                first_kick_onset_sample = possible_onsets_samples[0]

            print(
                f"Premier onset détecté (potentiel kick) à {first_kick_onset_sample / sr:.3f}s"
            )
            audio = audio[first_kick_onset_sample:]
        else:
            print("Aucun onset détecté, le sample commence tel quel.")

        # Ajuster la longueur
        current_length = len(audio)
        if current_length == 0:
            print(f"Erreur: Sample {audio_path} est vide après détection d'onset.")
            return None

        if current_length > total_target_samples:
            audio = audio[:total_target_samples]
            print(
                f"Sample coupé à {total_target_samples / sr:.2f}s ({measures} mesures à {self.tempo} BPM)"
            )
        elif current_length < total_target_samples:
            # Répéter pour atteindre la longueur, plus judicieux que le padding avec du silence pour la musique
            num_repeats = int(np.ceil(total_target_samples / current_length))
            audio = np.tile(audio, num_repeats)[:total_target_samples]
            print(f"Sample étendu par répétition à {total_target_samples / sr:.2f}s")

        # Crossfade pour une boucle plus douce
        fade_duration_ms = 20  # 20ms crossfade
        fade_samples = int(sr * (fade_duration_ms / 1000.0))
        if (
            len(audio) > 2 * fade_samples
        ):  # S'assurer que l'audio est assez long pour un crossfade
            fade_out_part = audio[-fade_samples:] * np.linspace(1, 0, fade_samples)
            fade_in_part = audio[:fade_samples] * np.linspace(0, 1, fade_samples)
            audio[:fade_samples] = (
                fade_out_part + fade_in_part
            )  # Crossfade au point de boucle
            # Simple fade sur la fin globale si ce n'est pas déjà fait par le crossfade
            audio[-fade_samples:] *= np.linspace(1, 0, fade_samples)

        loop_output_path = audio_path.replace(".wav", f"_{measures}m_loop.wav")
        try:
            sf.write(loop_output_path, audio, sr)
            print(f"Sample préparé pour la boucle sauvegardé : {loop_output_path}")
            return loop_output_path
        except Exception as e:
            print(
                f"Erreur lors de la sauvegarde du sample bouclé {loop_output_path}: {e}"
            )
            return None

    def get_sample_length_seconds(self, file_path):
        """Obtenir la durée du sample en secondes en utilisant Pygame"""
        try:
            sound = pygame.mixer.Sound(file_path)
            return sound.get_length()
        except pygame.error as e:
            print(
                f"Erreur Pygame lors du chargement de {file_path} pour obtenir la durée: {e}"
            )
            # Fallback avec librosa si Pygame échoue
            try:
                audio, sr = librosa.load(file_path, sr=None)
                return librosa.get_duration(y=audio, sr=sr)
            except Exception as le:
                print(f"Erreur Librosa également pour {file_path}: {le}")
                return 0

    def wait_for_next_bar(self, beats_per_bar=BEATS_PER_BAR):
        """Attend la fin de la mesure en cours pour démarrer le prochain sample en synchro."""
        if not self.is_playing or self.current_sample_length_seconds == 0:
            print(
                "Aucun sample en cours ou durée inconnue, démarrage immédiat du prochain."
            )
            return

        # Durée d'une mesure en secondes
        seconds_per_beat = 60.0 / self.tempo
        bar_duration_seconds = seconds_per_beat * beats_per_bar

        elapsed_since_loop_start = time.time() - self.playback_start_time

        # Combien de mesures complètes se sont écoulées dans la boucle actuelle
        # (même si la boucle fait plus d'une mesure, on se cale sur une structure de mesure)
        # Position dans la structure de mesures depuis le début de LA BOUCLE
        position_in_bar_structure = elapsed_since_loop_start % bar_duration_seconds

        time_remaining_in_bar = bar_duration_seconds - position_in_bar_structure

        # Petite marge pour éviter de démarrer pile sur la fin et causer un glitch
        # ou de calculer un temps négatif si on est pile dessus.
        if time_remaining_in_bar < 0.05:  # Si on est très proche de la fin de la mesure
            time_remaining_in_bar += (
                bar_duration_seconds  # Attendre la prochaine mesure complète
            )

        print(
            f"Attente de {time_remaining_in_bar:.2f}s pour la prochaine mesure (Tempo: {self.tempo} BPM)..."
        )
        time.sleep(time_remaining_in_bar)

    def play_sample_loop(self, file_path, measures_to_loop=2):
        """Joue un sample en boucle, après l'avoir préparé."""
        print(
            f"Préparation du sample: {file_path} pour {measures_to_loop} mesures à {self.tempo} BPM"
        )
        loop_ready_path = self.prepare_sample_for_loop(
            file_path, measures=measures_to_loop
        )

        if not loop_ready_path:
            print(f"Échec de la préparation du sample {file_path}")
            return

        new_sample_length_seconds = self.get_sample_length_seconds(loop_ready_path)
        if new_sample_length_seconds == 0:
            print(f"Erreur: Le sample préparé {loop_ready_path} a une durée de 0.")
            return

        # Si un sample est déjà en lecture, attendre le début de la prochaine mesure
        if self.is_playing:
            print(
                "Un sample est déjà en cours. En attente de la prochaine mesure pour la transition..."
            )
            self.wait_for_next_bar()
            pygame.mixer.music.stop()  # Arrêter l'ancien sample proprement
            print("Ancien sample arrêté.")

        try:
            pygame.mixer.music.load(loop_ready_path)
            pygame.mixer.music.play(-1)  # -1 pour boucle infinie
            self.current_sample_path = loop_ready_path
            self.current_sample_length_seconds = new_sample_length_seconds
            self.playback_start_time = time.time()
            self.is_playing = True
            print(
                f"Lecture en boucle de: {os.path.basename(loop_ready_path)} (Durée: {self.current_sample_length_seconds:.2f}s)"
            )
        except pygame.error as e:
            print(f"Erreur Pygame lors de la lecture de {loop_ready_path}: {e}")
            self.is_playing = False

    def stop_current_sample_playback_on_bar(self):
        """Arrête la lecture du sample en cours à la fin de sa mesure actuelle."""
        if self.is_playing:
            print("Attente de la fin de la mesure pour arrêter le sample...")
            self.wait_for_next_bar()
            pygame.mixer.music.stop()
            self.is_playing = False
            self.current_sample_path = None
            print("Lecture du sample arrêtée.")
        else:
            print("Aucun sample en cours de lecture à arrêter.")

    def start_session(self):
        if self.session_running:
            print("Session déjà en cours")
            return
        self.session_running = True
        print(f"Démarrage d'une session DJ '{self.profile_name}' à {self.tempo} BPM")

        # Le speech player est déjà initialisé, pas besoin de le démarrer spécifiquement ici
        # s'il est utilisé à la demande. Pygame mixer est initialisé dans __init__ ou une méthode dédiée.

        self.dj_thread = threading.Thread(target=self._main_loop)
        self.dj_thread.daemon = True
        self.dj_thread.start()

    def stop_session(self):
        if not self.session_running:
            print("Aucune session en cours")
            return
        print("Arrêt de la session...")
        self.session_running = False  # Signal pour arrêter la boucle principale

        if pygame.mixer.get_init():  # Vérifier si le mixer est initialisé
            pygame.mixer.music.stop()  # Arrêter la musique en boucle
            pygame.mixer.quit()  # Fermer Pygame Mixer proprement
            print("Pygame Mixer arrêté.")

        self.speech_audio_player.stop()  # Si SimpleAudioPlayer a une méthode stop

        if hasattr(self, "dj_thread") and self.dj_thread.is_alive():
            print("Attente de la fin du thread DJ...")
            self.dj_thread.join(timeout=5)  # Attendre avec un timeout
            if self.dj_thread.is_alive():
                print("Le thread DJ n'a pas pu être arrêté proprement.")
        print("Session DJ terminée.")

    def _main_loop(self):
        """Boucle principale du DJ."""
        # Pygame mixer est maintenant initialisé dans __init__ via initialize_pygame_mixer

        # Générer et jouer le premier kick
        try:
            print("Génération du premier kick...")
            kick_params = {
                "type": "techno_kick",
                "intensity": 5,
                "key": self.dj_brain.session_state.get("current_key", "C minor"),
            }
            sample_audio, _ = self.music_gen.generate_sample(
                sample_type=kick_params["type"],
                tempo=self.tempo,
                key=kick_params["key"],
                intensity=kick_params["intensity"],
            )
            kick_file = os.path.join(self.output_dir, "kick_initial.wav")
            if self.music_gen.save_sample(sample_audio, kick_file):
                self.play_sample_loop(
                    kick_file, measures_to_loop=1
                )  # Un kick boucle souvent sur 1 mesure
                # Mettre à jour l'état pour le LLM
                self.dj_brain.update_state(
                    active_samples=[
                        {
                            "type": "techno_kick",
                            "path": self.current_sample_path,
                            "length_seconds": self.current_sample_length_seconds,
                        }
                    ]
                )

            else:
                print("Erreur: Impossible de sauvegarder le kick initial. Arrêt.")
                self.session_running = False
                return
        except Exception as e:
            print(
                f"Erreur critique lors de la génération/lecture du kick initial: {e}. Arrêt."
            )
            self.session_running = False
            return

        # Boucle principale pour les décisions suivantes
        while self.session_running:
            try:
                decision = self.dj_brain.get_next_decision()
                self._process_dj_decision(decision)

                # Le temps d'attente pourrait être dynamique ou basé sur la durée des samples
                # Pour l'instant, on garde une attente fixe, mais `play_sample_loop` gère la synchro.
                # Si aucune action n'a réellement joué de son, on peut attendre moins.
                # Par exemple, si c'est juste un effet (non implémenté ici) ou un speech court.
                # Un sleep plus court ici pour permettre des décisions plus fréquentes.
                # La synchro est gérée par wait_for_next_bar dans play_sample_loop.
                time.sleep(
                    2
                )  # Réduire le sleep pour que le LLM puisse prendre des décisions plus rapidement
                # La vraie attente musicale se fait dans play_sample_loop

            except Exception as e:
                print(f"Erreur dans la boucle principale: {e}")
                time.sleep(5)

        print("Fin de la boucle principale du DJ.")

    def _process_dj_decision(self, decision):
        action_type = decision.get("action_type", "")
        params = decision.get("parameters", {})

        print(f"\n--- Décision DJ ---")
        print(f"Action: {action_type}")
        print(f"Params: {params}")
        print(f"Raisonnement: {decision.get('reasoning', 'N/A')}")
        print(f"--------------------")

        if action_type == "sample":
            sample_type = params.get(
                "type", "techno_kick"
            )  # Fallback au kick si non spécifié
            intensity = params.get("intensity", 5)
            key = params.get(
                "key", self.dj_brain.session_state.get("current_key", "C minor")
            )
            style_tag = params.get("style_tag")  # Peut être None
            # Le LLM pourrait suggérer un nombre de mesures pour la boucle
            measures_for_loop = params.get(
                "measures", 2 if sample_type != "techno_kick" else 1
            )

            print(
                f"Génération demandée pour: {sample_type}, intensité {intensity}, tonalité {key}, style {style_tag}, mesures {measures_for_loop}"
            )

            try:
                sample_audio, sample_info = self.music_gen.generate_sample(
                    sample_type=sample_type,
                    tempo=self.tempo,
                    key=key,
                    intensity=intensity,
                    style_tag=style_tag,
                )

                timestamp = int(time.time())
                sample_filename = os.path.join(
                    self.output_dir, f"{sample_type}_{timestamp}.wav"
                )

                if self.music_gen.save_sample(sample_audio, sample_filename):
                    print(f"Sample {sample_type} sauvegardé: {sample_filename}")

                    # Décider s'il faut remplacer ou superposer (pour l'instant, on remplace)
                    # self.stop_current_sample_playback_on_bar() # Commenter si on veut superposer à terme
                    self.play_sample_loop(
                        sample_filename, measures_to_loop=measures_for_loop
                    )

                    # Mettre à jour l'état du DJ (on suppose qu'un seul sample principal joue à la fois pour l'instant)
                    # Pour la superposition, il faudrait gérer une liste de samples actifs.
                    self.dj_brain.update_state(
                        active_samples=[
                            {
                                "type": sample_type,
                                "path": self.current_sample_path,
                                "length_seconds": self.current_sample_length_seconds,
                                "key": key,
                            }
                        ]
                    )
                    self.dj_brain.session_state["current_key"] = (
                        key  # Mettre à jour la tonalité active
                    )
                else:
                    print(f"Erreur de sauvegarde du sample {sample_type}")

            except Exception as e:
                print(
                    f"Erreur lors de la génération/lecture du sample {sample_type}: {e}"
                )

        elif action_type == "speech":
            text_to_say = params.get("text", "Yeah, feel the beat!")
            energy = params.get("energy", 5)
            print(f'DJ Speech: "{text_to_say}" (Energy: {energy})')
            try:
                # Optionnel : Baisser le volume de la musique pendant le speech
                # current_volume = pygame.mixer.music.get_volume()
                # pygame.mixer.music.set_volume(current_volume * 0.3)
                # time.sleep(0.2) # Laisser le temps au volume de baisser

                speech_audio, speech_sr = self.speech.generate_speech(
                    text=text_to_say, energy_level=energy
                )
                speech_filename = os.path.join(
                    self.output_dir, f"speech_{int(time.time())}.wav"
                )
                sf.write(speech_filename, speech_audio, speech_sr)

                # Jouer le speech avec SimpleAudioPlayer pour ne pas interrompre la musique de fond
                self.speech_audio_player.play_file(speech_filename)

                # Attendre la fin du speech avant de restaurer le volume (si baissé)
                # Cela nécessite que play_file soit bloquant ou qu'on puisse savoir quand il finit
                # Pour l'instant, on fait une estimation ou on ne gère pas la baisse de volume.
                # Exemple simple :
                # speech_duration = len(speech_audio) / speech_sr
                # time.sleep(speech_duration + 0.5) # Attendre la fin du speech + marge
                # pygame.mixer.music.set_volume(current_volume)

            except Exception as e:
                print(f"Erreur lors de la génération/lecture du speech: {e}")

        elif action_type == "effect":
            print("Traitement d'effet non implémenté pour l'instant.")
            # Ici, tu pourrais par exemple :
            # - Changer le volume global (pygame.mixer.music.set_volume())
            # - Appliquer un filtre si tu avais un DSP en temps réel (plus complexe avec Pygame seul)
            # - Le LLM pourrait demander un effet "low_pass_filter" sur le sample en cours.
            #   Cela nécessiterait de recharger le sample, le traiter avec librosa/scipy, et le rejouer.
            #   Ou, idéalement, un moteur audio plus avancé.

        elif action_type == "transition":
            print(
                "Demande de transition. Gérée implicitement par le prochain sample pour l'instant."
            )
            # Le LLM pourrait ici donner des indications sur *comment* transiter
            # (ex: "long_fade_out_kick", "introduce_synth_with_filter_sweep")
            # La logique actuelle arrête l'ancien sample et démarre le nouveau sur la prochaine mesure.
            # Une transition plus élaborée demanderait plus de contrôle (ex: crossfading).
            # Pour l'instant, on peut considérer que `play_sample_loop` qui attend la prochaine mesure
            # EST la transition.
            # On pourrait aussi demander au LLM un type de sample "transition_fx"
            next_sample_type = params.get("next_sample_type", "techno_synth")
            print(
                f"Préparation pour transiter vers un sample de type: {next_sample_type}"
            )
            # La logique actuelle va simplement générer et jouer le prochain sample décidé par le LLM.
            # Tu pourrais ajouter ici une logique pour un effet de transition spécifique si le LLM le demande.

        else:
            print(f"Type d'action inconnu: {action_type}")

    # La méthode _audio_callback et _audio_producer étaient liées à ton AudioProcessor.
    # Si tu n'utilises plus AudioProcessor pour le mixage principal (ce qui semble être le cas
    # avec Pygame pour les boucles), ces méthodes ne sont plus nécessaires pour le DJSystem principal.
    # Elles seraient utiles si tu construisais ton propre moteur de mixage temps réel plus avancé.
