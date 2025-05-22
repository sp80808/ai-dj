import os
import time
import threading
import numpy as np
import soundfile as sf
from core.llm_interface import DJAILL
from core.music_generator import MusicGenerator
from core.audio_simple import SimpleAudioPlayer
from core.tts_engine import DJSpeech
from core.layer_manager import LayerManager
from config.live_profiles import LIVE_PROFILES


class LiveSession:
    """
    Système de génération musicale en temps réel pour performances live.
    Génère un nouveau sample toutes les 30 secondes pour que le DJ puisse s'adapter.
    """

    def __init__(self, args):
        """
        Initialise la session live

        Args:
            args: Arguments de ligne de commande contenant les configurations
        """
        # Configuration de base
        self.model_path = args.model_path
        self.profile_name = args.profile
        self.output_dir_base = args.output_dir
        self.sample_rate = 44100
        self.audio_model = args.audio_model
        self.generation_duration = args.generation_duration
        self.current_layer_id = "current_loop"
        self.next_layer_id = "next_loop"
        self.is_using_current = True
        # Vérifier que le profil existe
        if self.profile_name not in LIVE_PROFILES:
            profiles = ", ".join(LIVE_PROFILES.keys())
            raise ValueError(
                f"Profil inconnu: {self.profile_name}. Disponibles: {profiles}"
            )

        # Paramètres spécifiques au mode live
        self.sample_interval = (
            args.sample_interval if hasattr(args, "sample_interval") else 30
        )  # secondes
        self.last_generation_time = 0
        self.generation_in_progress = False
        self.samples_generated = 0
        self.average_generation_time = 5  # estimation initiale en secondes
        self.samples_history = []  # Historique des samples générés
        self.session_running = False
        self.global_start_time = None  # Heure de démarrage de la session

        # Créer le répertoire de sortie s'il n'existe pas
        os.makedirs(self.output_dir_base, exist_ok=True)

        # Créer les sous-répertoires
        self.samples_dir = os.path.join(self.output_dir_base, "live_samples")
        os.makedirs(self.samples_dir, exist_ok=True)

        # L'état initial du LLM
        initial_llm_state = {
            "mode": "live",
            "tempo": LIVE_PROFILES[self.profile_name].get("default_tempo", 126),
            "current_tempo": LIVE_PROFILES[self.profile_name].get("default_tempo", 126),
            "current_key": LIVE_PROFILES[self.profile_name].get(
                "default_key", "C minor"
            ),
            "phase": "intro",
            "current_phase": "intro",
            "energy_level": 5,
            "time_elapsed": 0,
            "time_to_next_sample": self.sample_interval,
            "average_generation_time": self.average_generation_time,
            "samples_generated": 0,
            "samples_history": [],
            "last_sample": None,
            "last_action_time": 0,
            "history": [],
            "session_duration": 0,
            "active_samples": [],
            "active_layers": {},
        }

        print("Initialisation du système Live DJ-IA...")
        print(f"Chargement du profil {self.profile_name}...")

        live_system_prompt = LIVE_PROFILES[self.profile_name]["system_prompt"]

        # Initialiser les composants
        self.dj_brain = DJAILL(
            model_path=self.model_path,
            profile_name=None,
            config=initial_llm_state,
            override_prompt=live_system_prompt,
        )

        print("Chargement de MusicGen...")
        self.music_gen = MusicGenerator(
            model_name=self.audio_model, default_duration=self.generation_duration
        )

        self.layer_manager = LayerManager(
            sample_rate=self.sample_rate,
            num_channels=4,  # Un nombre raisonnable pour le mode live
            output_dir=self.samples_dir,
        )
        self.speech_engine = DJSpeech()  # Renommé pour clarté
        self.speech_audio_player = SimpleAudioPlayer(
            sample_rate=self.sample_rate
        )  # Pour le speech
        # Définir le tempo
        self.layer_manager.set_master_tempo(
            LIVE_PROFILES[self.profile_name].get("default_tempo", 126)
        )

    def start_session(self):
        """Démarre la session live avec une génération régulière de samples"""
        if self.session_running:
            print("Session déjà en cours")
            return

        self.session_running = True
        self.global_start_time = time.time()
        print("")
        print(
            f"🎮 Session LIVE démarrée sur le profil '{LIVE_PROFILES[self.profile_name]['name']}' à {self.dj_brain.session_state['current_tempo']} BPM"
        )
        print(
            f"⏱️  Génération d'un nouveau sample toutes les {self.sample_interval} secondes"
        )

        # Démarrer le lecteur audio (pour les prévisualisations)
        self.speech_audio_player.start()
        self.keyboard_thread = threading.Thread(target=self._keyboard_monitor)
        self.keyboard_thread.daemon = True
        self.keyboard_thread.start()
        print("\n" + "=" * 60)
        print("🎮  SESSION LIVE DÉMARRÉE  🎮")
        print("=" * 60)
        print("🔄  r, next     : Rejeter le sample actuel et générer un nouveau")
        print("🎯  g, generate : Générer un sample spécifique (ex: g kick)")
        print("❌  q, quit     : Quitter l'application")
        print("ℹ️   h, help     : Afficher ce message d'aide")
        print("=" * 60)
        print(
            f"🎛️  Profil: {self.profile_name} | ⏱️  Intervalle: {self.sample_interval}s"
        )
        print("=" * 60)

        # Démarrer le thread principal
        self.live_thread = threading.Thread(target=self._main_loop)
        self.live_thread.daemon = True
        self.live_thread.start()

    def stop_session(self):
        """Arrête la session live"""
        if not self.session_running:
            print("Aucune session en cours")
            return

        print("Arrêt de la session live...")
        self.session_running = False

        # Arrêter le lecteur audio
        self.speech_audio_player.stop()

        # Attendre la fin du thread principal
        if hasattr(self, "live_thread") and self.live_thread.is_alive():
            print("Attente de la fin du thread live...")
            self.live_thread.join(timeout=5)
            if self.live_thread.is_alive():
                print("Le thread live n'a pas pu être arrêté proprement.")

        print("Session live terminée.")

    def _normalize_audio(self, audio_data, target_level=-3.0):
        """
        Normalise un échantillon audio à un niveau cible en dB.

        Args:
            audio_data (np.array): Données audio à normaliser
            target_level (float): Niveau cible en dB (ex: -3.0 dB)

        Returns:
            np.array: Audio normalisé
        """
        # Vérifier si l'audio n'est pas vide
        if np.max(np.abs(audio_data)) == 0:
            print("⚠️ Audio vide, impossible de normaliser")
            return audio_data

        # Calculer le niveau actuel en dB
        current_peak = np.max(np.abs(audio_data))
        current_level_db = 20 * np.log10(current_peak)

        # Calculer le gain nécessaire
        gain_db = target_level - current_level_db
        gain_linear = 10 ** (gain_db / 20)

        # Appliquer le gain
        normalized_audio = audio_data * gain_linear

        # Protection contre l'écrêtage
        if np.max(np.abs(normalized_audio)) > 0.99:
            normalized_audio = normalized_audio * (
                0.99 / np.max(np.abs(normalized_audio))
            )
            print(f"⚠️ Ajustement pour éviter l'écrêtage")

        print(
            f"🎚️ Normalisation: {current_level_db:.1f} dB → {target_level:.1f} dB (gain: {gain_db:.1f} dB)"
        )

        return normalized_audio

    def _display_prompt(self):
        """Affiche l'invite de commande pour l'utilisateur"""
        print("\n" + "-" * 60)
        print(
            "💬 Commande (r=rejeter, g=générer, s=sauver, q=quitter, h=aide) > ",
            end="",
            flush=True,
        )

    def _main_loop(self):
        """Boucle principale pour la génération régulière de samples"""
        try:
            # Démarrer par une première génération
            self._generate_next_sample()
            last_status_display = 0
            status_interval = 15

            # Boucle principale
            while self.session_running:
                current_time = time.time()

                # Mettre à jour le temps écoulé dans la session
                elapsed_session_time = current_time - self.global_start_time
                self.dj_brain.session_state["time_elapsed"] = elapsed_session_time

                # Calculer le temps restant avant la prochaine génération
                time_since_last_gen = (
                    current_time - self.last_generation_time
                    if self.last_generation_time > 0
                    else self.sample_interval
                )
                time_to_next_sample = max(0, self.sample_interval - time_since_last_gen)
                self.dj_brain.session_state["time_to_next_sample"] = time_to_next_sample

                current_status_period = int(elapsed_session_time / status_interval)
                if current_status_period > last_status_display:
                    print(
                        f"\r⏱️  Temps écoulé: {elapsed_session_time:.1f}s | Prochain sample dans: {time_to_next_sample:.1f}s",
                        end="",
                    )
                    self._display_prompt()
                    last_status_display = current_status_period

                if (
                    time_since_last_gen >= self.sample_interval
                    and not self.generation_in_progress
                ):
                    print(
                        f"\r⏱️  Temps écoulé depuis dernière génération: {time_since_last_gen:.1f}s - Génération d'un nouveau sample"
                    )
                    self._generate_next_sample()
                    self._display_prompt()

                time.sleep(0.1)

        except Exception as e:
            print(f"Erreur critique dans _main_loop: {e}")
            import traceback

            traceback.print_exc()
            self.session_running = False

    def _generate_next_sample(self):
        """Génère un nouveau sample via le LLM et MusicGen"""
        try:
            # Marquer qu'une génération est en cours
            self.generation_in_progress = True

            # Obtenir la décision du LLM pour le nouveau sample
            start_time = time.time()
            decision = self.dj_brain.get_next_decision()

            # Traiter la décision
            sample_path = self._process_live_decision(decision)

            # Mettre à jour les métriques de génération
            generation_time = time.time() - start_time
            self.last_generation_time = time.time()
            self.samples_generated += 1

            # Mettre à jour le temps moyen de génération (moyenne mobile)
            self.average_generation_time = (
                self.average_generation_time * (self.samples_generated - 1)
                + generation_time
            ) / self.samples_generated
            self.dj_brain.session_state["average_generation_time"] = (
                self.average_generation_time
            )
            self.dj_brain.session_state["samples_generated"] = self.samples_generated

            # Ajouter à l'historique des samples
            if sample_path:
                sample_info = {
                    "id": self.samples_generated,
                    "path": sample_path,
                    "time_generated": self.last_generation_time,
                    "generation_time": generation_time,
                    "decision": decision,
                }
                self.samples_history.append(sample_info)
                self.dj_brain.session_state["samples_history"] = self.samples_history[
                    -5:
                ]  # Garder les 5 derniers samples

            # Marquer que la génération est terminée
            self.generation_in_progress = False

            print(
                f"✅ Nouveau sample #{self.samples_generated} généré en {generation_time:.1f}s (moyenne: {self.average_generation_time:.1f}s)"
            )
            print(f"⏱️  Prochain sample dans {self.sample_interval} secondes")

        except Exception as e:
            print(f"❌ Erreur lors de la génération du sample: {e}")
            import traceback

            traceback.print_exc()
            self.generation_in_progress = False

    def _process_live_decision(self, decision):
        """
        Traite la décision du LLM pour le mode live

        Args:
            decision: Décision du LLM au format dictionnaire

        Returns:
            str: Chemin du sample généré, ou None en cas d'erreur
        """
        action_type = decision.get("action_type", "")
        params = decision.get("parameters", {})
        reasoning = decision.get("reasoning", "N/A")

        print(f"\n🤖 Action LLM: {action_type}")
        print(f"💭 Raison: {reasoning}")
        print(f"\n⚙️  Paramètres: {params}\n")

        # Action principale: générer un sample
        if action_type == "generate_sample":
            sample_details = params.get("sample_details", {})
            if "user_request" in self.dj_brain.session_state:
                user_request = self.dj_brain.session_state.get("user_request", {})

                # Vérifier si la requête est récente (moins de 60 secondes)
                if time.time() - user_request.get("timestamp", 0) < 60:
                    # Modifier les paramètres de génération pour correspondre à la requête
                    request_text = user_request.get("text", "")
                    request_type = user_request.get("sample_type", "")

                    print(
                        f"🎯 Modification de la génération pour répondre à la requête: '{request_text}'"
                    )

                    # Utiliser le type demandé si disponible
                    if request_type:
                        sample_details["type"] = request_type

                    # Ajouter le texte de la requête aux mots-clés pour MusicGen
                    if request_text:
                        if "musicgen_prompt_keywords" not in sample_details:
                            sample_details["musicgen_prompt_keywords"] = []

                        # Ajouter les mots-clés de la requête
                        keywords = [
                            word for word in request_text.split() if len(word) > 3
                        ]
                        if keywords:
                            sample_details["musicgen_prompt_keywords"].extend(keywords)

                    # Supprimer la requête pour ne pas l'utiliser à nouveau
                    del self.dj_brain.session_state["user_request"]

            # Générer un ID unique pour le sample
            sample_id = f"live_{self.samples_generated}"

            try:
                # Préparation des paramètres
                sample_type = sample_details.get("type", "techno_synth")
                musicgen_keywords = sample_details.get(
                    "musicgen_prompt_keywords", [sample_type]
                )
                key = sample_details.get(
                    "key", self.dj_brain.session_state.get("current_key", "C minor")
                )
                measures = sample_details.get("measures", 2)
                intensity = sample_details.get("intensity", 6)

                # Déterminer le genre en fonction du profil DJ
                genre = self._determine_genre()

                print(
                    f"🎛️  Génération MusicGen pour sample '{sample_id}':\n"
                    f"🎵 Type={sample_type}\n"
                    f"🔑 Keywords={musicgen_keywords}\n"
                    f"🎹 Key={key}\n"
                    f"📏 Measures={measures}\n"
                )

                # Générer le sample audio avec MusicGen
                sample_audio, sample_info = self.music_gen.generate_sample(
                    sample_type=sample_type,
                    tempo=self.dj_brain.session_state["current_tempo"],
                    key=key,
                    intensity=intensity,
                    musicgen_prompt_keywords=musicgen_keywords,
                    genre=genre,
                )

                # Sauvegarder le sample brut
                original_file_path = os.path.join(
                    self.samples_dir, f"{sample_id}_orig_{int(time.time())}.wav"
                )
                self.music_gen.save_sample(sample_audio, original_file_path)

                # Préparer le sample pour la boucle (découpage et time stretching)
                processed_sample_path = self._prepare_sample_for_loop(
                    original_file_path,
                    sample_id,
                    measures,
                    self.dj_brain.session_state["current_tempo"],
                )

                if processed_sample_path:
                    print(
                        f"🔄 Sample live préparé: {os.path.basename(processed_sample_path)}"
                    )

                    # Préparation des informations de sample pour le layer manager
                    sample_details_for_layer = {
                        "original_file_path": processed_sample_path,
                        "measures": measures,
                        "type": sample_type,
                        "key": key,
                    }

                    # Paramètres de lecture
                    playback_params = {"volume": 0.9, "pan": 0.0}

                    # Utiliser TOUJOURS le même ID de layer pour remplacer automatiquement
                    layer_id = "live_loop"

                    # Ajouter/remplacer le layer - LayerManager gère automatiquement le remplacement
                    print(
                        f"🔊 Ajout du nouveau sample '{sample_id}' (remplacera automatiquement le précédent)"
                    )
                    self.layer_manager.manage_layer(
                        layer_id=layer_id,
                        operation="add_replace",  # Remplace automatiquement si le layer existe
                        sample_details=sample_details_for_layer,
                        playback_params=playback_params,
                        effects=[],
                    )

                    # Stocker le chemin pour référence future
                    self.dj_brain.session_state["last_sample"] = {
                        "id": sample_id,
                        "type": sample_type,
                        "key": key,
                        "path": processed_sample_path,
                        "generated_at": time.time(),
                    }
                    self.dj_brain.session_state["current_key"] = key

                    return processed_sample_path
                else:
                    print(f"❌ Échec de la préparation du sample live '{sample_id}'")
                    return None

            except Exception as e:
                print(
                    f"❌ Erreur pendant la génération du sample live '{sample_id}': {e}"
                )
                import traceback

                traceback.print_exc()
                return None

        # Action secondaire: speech (si implémenté)
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

        # Action tertiaire: changer la phase
        elif action_type == "set_phase":
            new_phase = params.get("new_phase")
            if new_phase:
                self.dj_brain.session_state["current_phase"] = new_phase
                print(f"Phase du set mise à jour à '{new_phase}'.")

            return None

        # Action non reconnue
        else:
            print(f"Type d'action LLM inconnu pour le mode live: {action_type}")
            return None

    def _determine_genre(self):
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
        return "electronic"  # Par défaut

    def _prepare_sample_for_loop(self, original_audio_path, sample_id, measures, tempo):
        """
        Prépare un sample pour qu'il boucle proprement
        (détection d'onset, calage sur le tempo, crossfade)

        Args:
            original_audio_path: Chemin vers le fichier audio original
            sample_id: Identifiant unique du sample
            measures: Nombre de mesures souhaitées
            tempo: Tempo en BPM

        Returns:
            str: Chemin vers le sample préparé, ou None en cas d'erreur
        """
        try:
            import librosa
            from scipy.signal import butter, sosfilt

            # Charger l'audio
            audio, sr_orig = librosa.load(original_audio_path, sr=None)
            if sr_orig != self.sample_rate:
                audio = librosa.resample(
                    audio, orig_sr=sr_orig, target_sr=self.sample_rate
                )
            sr = self.sample_rate

            # Calculer la longueur idéale basée sur le tempo et les mesures
            seconds_per_beat = 60.0 / tempo
            samples_per_beat = int(seconds_per_beat * sr)
            samples_per_bar = samples_per_beat * 4  # Assumant une mesure 4/4
            target_total_samples = samples_per_bar * measures

            # Détection d'onset pour trouver le meilleur point de départ
            onset_env = librosa.onset.onset_strength(y=audio, sr=sr)
            onsets_samples = librosa.onset.onset_detect(
                onset_envelope=onset_env, sr=sr, units="samples", backtrack=False
            )

            # Trouver un point de départ optimal
            start_offset_samples = 0
            if len(onsets_samples) > 0:
                # Chercher un onset dans les 100 premières ms
                potential_starts = [o for o in onsets_samples if o < sr * 0.1]
                if potential_starts:
                    start_offset_samples = potential_starts[0]
                else:
                    start_offset_samples = onsets_samples[0]  # Premier onset détecté

                print(
                    f"✂️  Sample '{sample_id}': Début optimisé à {start_offset_samples / sr:.3f}s."
                )
                audio = audio[start_offset_samples:]
            else:
                print(f"Sample '{sample_id}': Aucun onset détecté, début non modifié.")

            # Ajuster la longueur du sample
            current_length = len(audio)
            if current_length == 0:
                print(f"Erreur: Sample '{sample_id}' vide après découpage.")
                return None

            # Soit tronquer, soit répéter pour atteindre la longueur cible
            if current_length > target_total_samples:
                audio = audio[:target_total_samples]
            elif current_length < target_total_samples:
                num_repeats = int(np.ceil(target_total_samples / current_length))
                audio = np.tile(audio, num_repeats)[:target_total_samples]

            # Appliquer un crossfade pour une boucle fluide
            fade_ms = 10  # 10ms pour un crossfade subtil
            fade_samples = int(sr * (fade_ms / 1000.0))

            if len(audio) > 2 * fade_samples:
                # Prendre la fin et le début
                end_part = audio[-fade_samples:]
                start_part = audio[:fade_samples]

                # Créer les rampes de fade
                fade_out_ramp = np.linspace(1.0, 0.0, fade_samples)
                fade_in_ramp = np.linspace(0.0, 1.0, fade_samples)

                # Appliquer le crossfade
                audio[:fade_samples] = (
                    start_part * fade_in_ramp + end_part * fade_out_ramp
                )
                audio[-fade_samples:] = end_part * fade_out_ramp
            else:
                print(
                    f"Sample '{sample_id}' trop court pour le crossfade de {fade_ms}ms."
                )

            # Définir les chemins de fichiers
            temp_path = os.path.join(self.samples_dir, f"temp_{sample_id}.wav")
            final_path = os.path.join(self.samples_dir, f"{sample_id}_loop.wav")

            # Sauvegarder d'abord une version temporaire
            sf.write(temp_path, audio, sr)

            # Appliquer le time-stretching pour être sûr que le tempo est exact
            # Ceci utilise la fonction existante de layer_manager si disponible
            try:
                if "stable-audio" in self.audio_model:
                    print(
                        f"🚀 Stable Audio : on fait confiance au BPM du prompt ({tempo} BPM)"
                    )
                    stretched_audio = audio
                else:
                    # Version simplified du code de layermanager.match_sample_to_tempo
                    from librosa.effects import time_stretch

                    # Estimer le tempo actuel
                    estimated_tempo = librosa.beat.tempo(
                        onset_envelope=onset_env, sr=sr
                    )[0]
                    print(f"🎵 Tempo estimé du sample: {estimated_tempo:.1f} BPM")

                    # Si les tempos sont très proches, pas besoin de time stretching
                    tempo_ratio = abs(estimated_tempo - tempo) / tempo
                    if tempo_ratio < 0.02:  # Moins de 2% de différence
                        print(
                            f"Tempos similaires ({estimated_tempo:.1f} vs {tempo:.1f} BPM), pas de stretching"
                        )
                        stretched_audio = audio
                    else:
                        # Calculer le ratio de time stretching et l'appliquer
                        stretch_ratio = estimated_tempo / tempo
                        print(
                            f"Time stretching: {estimated_tempo:.1f} → {tempo:.1f} BPM (ratio: {stretch_ratio:.2f})"
                        )
                        stretched_audio = time_stretch(audio, rate=stretch_ratio)

                        # Ajuster à la longueur exacte si nécessaire
                        if len(stretched_audio) != target_total_samples:
                            x_original = np.linspace(0, 1, len(stretched_audio))
                            x_target = np.linspace(0, 1, target_total_samples)
                            stretched_audio = np.interp(
                                x_target, x_original, stretched_audio
                            )
                stretched_audio = self._normalize_audio(
                    stretched_audio, target_level=-3.0
                )
                # Sauvegarder la version finale
                sf.write(final_path, stretched_audio, sr)

                # Supprimer le fichier temporaire
                if os.path.exists(temp_path):
                    os.remove(temp_path)

                # Supprimer éventuellement le fichier original pour économiser de l'espace
                if (
                    os.path.exists(original_audio_path)
                    and original_audio_path != final_path
                ):
                    os.remove(original_audio_path)

                print(f"💾 Sample '{sample_id}' préparé et sauvegardé: {final_path}")
                return final_path

            except Exception as e:
                print(f"Erreur lors du time stretching: {e}")
                # En cas d'échec, utiliser la version sans time stretching
                sf.write(final_path, audio, sr)
                return final_path

        except Exception as e:
            print(f"Erreur lors de la préparation du sample '{sample_id}': {e}")
            import traceback

            traceback.print_exc()
            return None

    def get_last_sample_path(self):
        """Retourne le chemin du dernier sample généré"""
        if self.dj_brain.session_state.get("last_sample"):
            return self.dj_brain.session_state["last_sample"].get("path")
        return None

    def reject_current_sample(self):
        """Rejette le sample actuel et demande immédiatement une nouvelle génération"""
        print("⚠️ Sample rejeté par l'utilisateur - génération d'un nouveau sample...")

        # Ajouter une note dans l'état du LLM pour qu'il sache que le dernier sample a été rejeté
        if "last_sample" in self.dj_brain.session_state:
            self.dj_brain.session_state["last_sample"]["rejected"] = True
            self.dj_brain.session_state["rejected_samples"] = (
                self.dj_brain.session_state.get("rejected_samples", 0) + 1
            )

        # Réinitialiser le timer pour forcer une génération immédiate
        self.last_generation_time = time.time() - self.sample_interval - 1

        # Vous pourriez aussi vouloir ajouter une instruction spéciale au LLM
        self.dj_brain.session_state["special_instruction"] = (
            "Le dernier sample a été rejeté. Génère quelque chose de significativement différent."
        )

    def _keyboard_monitor(self):
        """Thread pour surveiller les entrées clavier"""
        while self.session_running:
            try:
                cmd = input("").strip().lower()

                # Commandes simples
                if cmd in ["r", "reject", "n", "next"]:
                    self.reject_current_sample()
                    print("✓ Génération d'un nouveau sample demandée...")
                    self._display_prompt()
                elif cmd in ["q", "quit", "exit"]:
                    print("Arrêt demandé par l'utilisateur...")
                    self.session_running = False
                elif cmd in ["h", "help", "?"]:
                    print("\n" + "=" * 60)
                    print("📋 COMMANDES DISPONIBLES")
                    print("=" * 60)
                    print(
                        "🔄 r, next     : Rejeter le sample actuel et générer un nouveau"
                    )
                    print("🎯 g, generate : Générer un sample spécifique (ex: g kick)")
                    print("❌ q, quit     : Quitter l'application")
                    print("ℹ️  h, help     : Afficher ce message d'aide")
                    print("=" * 60)
                    self._display_prompt()  # Réafficher l'invite

                # Commande de génération spécifique
                elif cmd.startswith("g ") or cmd.startswith("generate "):
                    # Extraire la requête après la commande
                    if cmd.startswith("g "):
                        request = cmd[2:].strip()
                    else:
                        request = cmd[9:].strip()

                    if request:
                        self.generate_specific_sample(request)
                        self._display_prompt()
                    else:
                        print(
                            "⚠️ Veuillez spécifier ce que vous souhaitez générer (ex: g kick)"
                        )
                        self._display_prompt()
                elif cmd.strip() == "":
                    # Si l'utilisateur appuie juste sur Entrée, réafficher l'invite
                    self._display_prompt()
                else:
                    # Commande non reconnue
                    print(f"❓ Commande non reconnue: '{cmd}'. Tapez 'h' pour l'aide.")
                    self._display_prompt()  # Réafficher l'invite

            except Exception as e:
                print(f"Erreur dans le moniteur clavier: {e}")
                self._display_prompt()

    def generate_specific_sample(self, request):
        """
        Génère un sample spécifique demandé par l'utilisateur

        Args:
            request (str): Description du sample demandé (ex: "kick", "bass", "ambient pad")
        """
        print(f"🎯 Génération d'un sample spécifique: '{request}'")

        # Déterminer le type de sample en fonction de la requête
        sample_type = self._determine_sample_type(request)

        # Ajouter l'instruction spéciale au LLM
        self.dj_brain.session_state["special_instruction"] = (
            f"L'utilisateur a demandé spécifiquement: '{request}'. Génère un sample de type {sample_type} qui répond à cette demande."
        )

        # Réinitialiser le timer pour forcer une génération immédiate
        self.last_generation_time = time.time() - self.sample_interval - 1

        # Stocker la requête pour l'utiliser lors de la génération
        self.dj_brain.session_state["user_request"] = {
            "text": request,
            "sample_type": sample_type,
            "timestamp": time.time(),
        }

        print(f"✓ Génération d'un '{sample_type}' en cours...")

    def _determine_sample_type(self, request):
        """
        Détermine le type de sample le plus approprié en fonction de la requête

        Args:
            request (str): Description du sample demandé

        Returns:
            str: Type de sample approprié
        """
        request = request.lower()

        # Types de samples disponibles par profil
        available_types = LIVE_PROFILES[self.profile_name].get("sample_types", [])

        # Mappings de mots-clés vers types de samples
        type_mappings = {
            # Éléments rythmiques
            "kick": [
                t for t in available_types if "kick" in t or "beat" in t or "drum" in t
            ],
            "beat": [t for t in available_types if "beat" in t or "drum" in t],
            "drum": [t for t in available_types if "drum" in t or "percussion" in t],
            "percussion": [t for t in available_types if "percussion" in t],
            # Éléments harmoniques
            "bass": [t for t in available_types if "bass" in t],
            "synth": [t for t in available_types if "synth" in t],
            "pad": [t for t in available_types if "pad" in t],
            "chord": [t for t in available_types if "chord" in t or "pad" in t],
            "melody": [t for t in available_types if "melody" in t or "synth" in t],
            # Ambiances et effets
            "fx": [t for t in available_types if "fx" in t],
            "ambient": [
                t
                for t in available_types
                if "pad" in t or "ambient" in t or "texture" in t
            ],
            "atmosphere": [
                t
                for t in available_types
                if "pad" in t or "ambient" in t or "texture" in t
            ],
            "drone": [t for t in available_types if "drone" in t or "pad" in t],
            "texture": [t for t in available_types if "texture" in t or "pad" in t],
        }

        # Chercher des correspondances dans la requête
        for keyword, types in type_mappings.items():
            if keyword in request and types:
                return types[0]  # Retourner le premier type correspondant

        # Si aucune correspondance, retourner un type par défaut
        if "techno_synth" in available_types:
            return "techno_synth"
        elif available_types:
            return available_types[0]
        else:
            return "techno_synth"  # Fallback absolu
