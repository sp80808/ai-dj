import os
import time
import random
import pygame
import librosa
import numpy as np
import soundfile as sf
from typing import Dict, List, Optional, Any

# Constantes
BEATS_PER_BAR = 4


class AudioLayer:
    """Représente un layer audio individuel dans le mix."""

    def __init__(
        self,
        layer_id: str,
        file_path: str,
        channel: pygame.mixer.Channel,
        volume: float = 0.8,
        pan: float = 0.0,
    ):
        self.layer_id = layer_id
        self.file_path = file_path  # Chemin vers le sample bouclé et traité
        self.sound_object: Optional[pygame.mixer.Sound] = None
        self.channel = channel
        self.volume = volume
        self.pan = pan  # -1.0 (gauche) à 1.0 (droite), 0.0 (centre)
        self.is_playing = False
        self.length_seconds = 0.0

        try:
            self.sound_object = pygame.mixer.Sound(self.file_path)
            self.length_seconds = self.sound_object.get_length()
            self.channel.set_volume(self.volume)
            # La gestion du pan avec set_volume((L, R)) est plus flexible que set_pan
            left_vol = self.volume * (1.0 - max(0, self.pan))
            right_vol = self.volume * (1.0 + min(0, self.pan))
            self.channel.set_volume(left_vol, right_vol)

        except pygame.error as e:
            print(
                f"Erreur Pygame lors de la création du Layer {self.layer_id} avec {self.file_path}: {e}"
            )
            self.sound_object = None  # Indiquer que le son n'a pas pu être chargé

    def play(self, loops=-1):
        """Joue le sample sur son canal."""
        if self.sound_object and not self.is_playing:
            try:
                self.channel.play(self.sound_object, loops=loops)
                self.is_playing = True
                print(f"Layer '{self.layer_id}' démarré sur le canal {self.channel}.")
            except pygame.error as e:
                print(f"Erreur Pygame lors de la lecture du layer {self.layer_id}: {e}")
        elif not self.sound_object:
            print(
                f"Impossible de jouer le layer '{self.layer_id}', sound_object non chargé."
            )
        elif self.is_playing:
            print(f"Layer '{self.layer_id}' est déjà en cours de lecture.")

    def stop(self, fadeout_ms: int = 0, cleanup: bool = True):
        """
        Arrête le sample sur son canal, avec un fadeout optionnel.

        Args:
            fadeout_ms: Durée du fadeout en millisecondes
            cleanup: Si True, nettoie le fichier audio du disque après l'arrêt
        """
        if self.is_playing:
            if fadeout_ms > 0 and self.sound_object:
                self.channel.fadeout(fadeout_ms)
            else:
                self.channel.stop()

            self.is_playing = False
            print(f"Layer '{self.layer_id}' arrêté.")

            # Nettoyer les ressources
            if cleanup:
                # Attendre que le fadeout soit terminé avant de nettoyer
                if fadeout_ms > 0:
                    time.sleep(fadeout_ms / 1000.0)

                # Libérer le son de la mémoire
                if self.sound_object:
                    # Supprimer la référence à l'objet son pour que le GC puisse le collecter
                    self.sound_object = None

                # Si c'est un fichier temporaire, le supprimer du disque
                if self.file_path and os.path.exists(self.file_path):
                    # Vérifier si c'est un fichier temporaire (par exemple, contient "_loop_" ou "_fx_")
                    is_temp_file = any(
                        marker in self.file_path
                        for marker in ["_loop_", "_fx_", "temp_", "_orig_"]
                    )

                    if is_temp_file:
                        try:
                            os.remove(self.file_path)
                            print(
                                f"Fichier audio temporaire supprimé: {self.file_path}"
                            )
                        except (PermissionError, OSError) as e:
                            # Ignorer l'erreur si le fichier est encore utilisé
                            print(
                                f"Impossible de supprimer le fichier {self.file_path}: {e}"
                            )

    def set_volume(self, volume: float):
        """Ajuste le volume du layer."""
        self.volume = np.clip(volume, 0.0, 1.0)
        if (
            self.channel and self.sound_object
        ):  # Vérifier si le canal est toujours valide
            # Recalculer L/R avec le nouveau volume et le pan existant
            left_vol = self.volume * (1.0 - max(0, self.pan))
            right_vol = self.volume * (1.0 + min(0, self.pan))
            self.channel.set_volume(left_vol, right_vol)

    def set_pan(self, pan: float):
        """Ajuste le panoramique du layer."""
        self.pan = np.clip(pan, -1.0, 1.0)
        if (
            self.channel and self.sound_object
        ):  # Vérifier si le canal est toujours valide
            left_vol = self.volume * (1.0 - max(0, self.pan))  # Si pan > 0, L diminue
            right_vol = self.volume * (1.0 + min(0, self.pan))  # Si pan < 0, R diminue
            self.channel.set_volume(left_vol, right_vol)


class LayerManager:
    """Gère plusieurs layers audio, leur synchronisation et leurs effets de base."""

    def __init__(
        self,
        sample_rate: int = 44100,
        num_channels: int = 16,
        output_dir: str = "./dj_layers_output",
    ):
        self.sample_rate = sample_rate
        self.output_dir = output_dir
        if not os.path.exists(self.output_dir):
            os.makedirs(self.output_dir)

        try:
            pygame.mixer.init(
                frequency=self.sample_rate, channels=2, buffer=2048
            )  # Initialiser avec buffer plus grand
            pygame.mixer.set_num_channels(num_channels)
            print(
                f"Pygame Mixer initialisé avec {pygame.mixer.get_num_channels()} canaux."
            )
        except pygame.error as e:
            print(f"Erreur d'initialisation de Pygame Mixer: {e}")
            raise  # Relancer l'exception car c'est critique

        self.layers: Dict[str, AudioLayer] = {}
        self.master_tempo: float = 126.0  # BPM
        self.global_playback_start_time: Optional[float] = (
            None  # Heure de démarrage du tout premier sample
        )
        self.is_master_clock_running: bool = False
        self.num_channels = num_channels

    def _get_available_channel(self) -> Optional[pygame.mixer.Channel]:
        """Trouve un canal Pygame non utilisé."""
        for i in range(self.num_channels):
            channel = pygame.mixer.Channel(i)
            if not channel.get_busy():
                return channel
        print("Attention: Plus de canaux disponibles.")
        return None

    def _prepare_sample_for_loop(
        self, original_audio_path: str, layer_id: str, measures: int
    ) -> Optional[str]:
        """Prépare un sample pour qu'il boucle (détection d'onset, calage, crossfade)."""
        try:
            audio, sr_orig = librosa.load(
                original_audio_path, sr=None
            )  # Charger avec son SR original d'abord
            if sr_orig != self.sample_rate:
                audio = librosa.resample(
                    audio, orig_sr=sr_orig, target_sr=self.sample_rate
                )
            sr = self.sample_rate
        except Exception as e:
            print(
                f"Erreur de chargement du sample {original_audio_path} avec librosa: {e}"
            )
            return None

        seconds_per_beat = 60.0 / self.master_tempo
        samples_per_beat = int(seconds_per_beat * sr)
        samples_per_bar = samples_per_beat * BEATS_PER_BAR
        target_total_samples = samples_per_bar * measures

        # Détection d'onset
        onset_env = librosa.onset.onset_strength(y=audio, sr=sr)
        onsets_samples = librosa.onset.onset_detect(
            onset_envelope=onset_env, sr=sr, units="samples", backtrack=False
        )

        start_offset_samples = 0
        if len(onsets_samples) > 0:
            # Heuristique simple: prendre le premier onset, ou un proche du début.
            # Pour un kick, on s'attend à ce qu'il soit très tôt.
            # Pour d'autres, c'est moins critique, mais on veut éviter un long silence.
            potential_starts = [
                o for o in onsets_samples if o < sr * 0.1
            ]  # Cherche un onset dans les 100 premières ms
            if potential_starts:
                start_offset_samples = potential_starts[0]
            else:
                start_offset_samples = onsets_samples[0]  # Sinon, le premier détecté

            print(
                f"Layer '{layer_id}': Premier onset (trim) à {start_offset_samples / sr:.3f}s."
            )
            audio = audio[start_offset_samples:]
        else:
            print(
                f"Layer '{layer_id}': Aucun onset détecté, le sample commence tel quel."
            )

        current_length = len(audio)
        if current_length == 0:
            print(f"Erreur: Layer '{layer_id}' vide après trim.")
            return None

        if current_length > target_total_samples:
            audio = audio[:target_total_samples]
        elif current_length < target_total_samples:
            num_repeats = int(np.ceil(target_total_samples / current_length))
            audio = np.tile(audio, num_repeats)[:target_total_samples]

        # Crossfade pour la boucle
        fade_ms = 10  # 10ms pour un crossfade subtil, typique en techno
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
            )  # Mélange au début
            # Pour assurer que la fin est bien à zéro et éviter un clic si la boucle est relancée manuellement
            audio[-fade_samples:] = end_part * fade_out_ramp
        else:
            print(f"Layer '{layer_id}' trop court pour le crossfade de {fade_ms}ms.")

        looped_sample_filename = f"{os.path.splitext(os.path.basename(original_audio_path))[0]}_loop_{layer_id}.wav"
        looped_sample_path = os.path.join(self.output_dir, looped_sample_filename)

        try:
            sf.write(looped_sample_path, audio, sr)
            print(
                f"Layer '{layer_id}': Sample bouclé sauvegardé : {looped_sample_path}"
            )
            return looped_sample_path
        except Exception as e:
            print(f"Erreur de sauvegarde du sample bouclé pour {layer_id}: {e}")
            return None

    def _apply_effects_to_sample(
        self, audio_path: str, effects: List[Dict[str, Any]], layer_id: str
    ) -> Optional[str]:
        """Applique des effets (actuellement HPF/LPF) à un fichier audio."""
        if not effects:
            return audio_path  # Pas d'effets à appliquer

        try:
            audio, sr = librosa.load(audio_path, sr=self.sample_rate)
        except Exception as e:
            print(f"Erreur chargement pour effets (Layer {layer_id}): {e}")
            return None

        for effect in effects:
            effect_type = effect.get("type")
            if effect_type == "hpf":
                cutoff = effect.get("cutoff_hz", 20)  # Défaut à 20Hz si non spécifié
                # Librosa n'a pas de HPF direct, mais on peut utiliser un LPF sur un signal inversé ou butterworth
                # Utilisons scipy pour un filtre Butterworth propre
                from scipy.signal import butter, sosfilt

                nyquist = 0.5 * sr
                normal_cutoff = cutoff / nyquist
                if normal_cutoff >= 1.0:  # cutoff doit être < nyquist
                    print(
                        f"Attention (Layer {layer_id}): Cutoff HPF {cutoff}Hz >= Nyquist {nyquist}Hz. Filtre ignoré."
                    )
                    continue
                # Ordre du filtre, 4 est un bon compromis
                sos = butter(
                    4, normal_cutoff, btype="highpass", analog=False, output="sos"
                )
                audio = sosfilt(sos, audio)
                print(f"Layer '{layer_id}': HPF appliqué à {cutoff}Hz.")
            elif effect_type == "lpf":
                cutoff = effect.get("cutoff_hz", 20000)
                from scipy.signal import butter, sosfilt

                nyquist = 0.5 * sr
                normal_cutoff = cutoff / nyquist
                if normal_cutoff >= 1.0:
                    print(
                        f"Attention (Layer {layer_id}): Cutoff LPF {cutoff}Hz >= Nyquist {nyquist}Hz. Filtre ignoré."
                    )
                    continue
                sos = butter(
                    4, normal_cutoff, btype="lowpass", analog=False, output="sos"
                )
                audio = sosfilt(sos, audio)
                print(f"Layer '{layer_id}': LPF appliqué à {cutoff}Hz.")
            # Ajouter d'autres effets ici (compresseur, etc. plus complexe avec numpy/scipy)

        effected_sample_filename = (
            f"{os.path.splitext(os.path.basename(audio_path))[0]}_fx.wav"
        )
        effected_sample_path = os.path.join(self.output_dir, effected_sample_filename)
        try:
            sf.write(effected_sample_path, audio, sr)
            return effected_sample_path
        except Exception as e:
            print(f"Erreur sauvegarde sample avec effets (Layer {layer_id}): {e}")
            return None

    def _wait_for_sync_point(self, beats_to_wait: float = BEATS_PER_BAR):
        """Attend le prochain point de synchronisation (début de mesure par défaut)."""
        if not self.is_master_clock_running or self.global_playback_start_time is None:
            print("Horloge maître non démarrée. Démarrage immédiat.")
            if not self.is_master_clock_running:  # Si c'est le tout premier sample
                self.global_playback_start_time = time.time()
                self.is_master_clock_running = True
            return

        seconds_per_beat = 60.0 / self.master_tempo
        sync_interval_seconds = (
            seconds_per_beat * beats_to_wait
        )  # Ex: 4 beats pour une mesure

        elapsed_since_global_start = time.time() - self.global_playback_start_time

        time_into_current_interval = elapsed_since_global_start % sync_interval_seconds
        time_remaining_to_sync = sync_interval_seconds - time_into_current_interval

        # Petite marge pour la précision du sleep et éviter les valeurs négatives si on est pile dessus
        if (
            time_remaining_to_sync < 0.02
        ):  # Moins de 20ms, on attend l'intervalle suivant
            time_remaining_to_sync += sync_interval_seconds

        if time_remaining_to_sync > 0:
            print(
                f"Attente de synchronisation: {time_remaining_to_sync:.3f}s (Tempo: {self.master_tempo} BPM, intervalle de {beats_to_wait} beats)"
            )
            time.sleep(time_remaining_to_sync)

        # Mettre à jour global_playback_start_time si c'est le premier sample qui le définit
        if not self.is_master_clock_running:
            self.global_playback_start_time = time.time()
            self.is_master_clock_running = True

    def manage_layer(
        self,
        layer_id: str,
        operation: str,
        sample_details: Optional[Dict[str, Any]] = None,
        playback_params: Optional[Dict[str, Any]] = None,
        effects: Optional[List[Dict[str, Any]]] = None,
        stop_behavior: str = "next_bar",
    ):
        """Gère un layer: ajout/remplacement, modification, suppression."""

        if operation == "add_replace":

            if (
                not sample_details or "original_file_path" not in sample_details
            ):  # On attend le chemin du fichier généré par MusicGen
                print(
                    f"Erreur (Layer {layer_id}): 'original_file_path' manquant pour 'add_replace'."
                )
                return

            original_file_path = sample_details["original_file_path"]
            measures = sample_details.get("measures", 2)  # Le LLM doit spécifier ça

            sample_type = sample_details.get("type", "").lower()

            # Liste exhaustive des types rythmiques basée sur les observations et prévisions
            rhythm_types = [
                # Techno/Electronic
                "techno_kick",
                "techno_percussion",
                "techno_drums",
                "techno_beat",
                "electronic_drums",
                "electronic_percussion",
                "electro_beat",
                "techno_loop",
                "minimal_beat",
                "minimal_drums",
                "minimal_percussion",
                "acid_percussion",
                "industrial_drums",
                # House
                "house_kick",
                "house_percussion",
                "house_drums",
                "house_beat",
                "house_rhythm",
                "deep_house_drums",
                "deep_house_percussion",
                "deep_house_kick",
                "deep_house_rhythm",
                "soulful_house_drums",
                "jazzy_house_percussion",
                "progressive_house_beat",
                "tech_house_percussion",
                "tech_house_kick",
                "afro_house_percussion",
                "tribal_house_drums",
                "chicago_house_beat",
                "disco_house_rhythm",
                # Hip-Hop
                "hiphop_beat",
                "hip_hop_beat",
                "hiphop_drums",
                "trap_beat",
                "boom_bap_beat",
                "hiphop_percussion",
                "lo-fi_beat",
                "lo-fi_drums",
                "rap_beat",
                "urban_drums",
                "808_beat",
                "breakbeat",
                "beat_loop",
                "drill_beat",
                "crunk_drums",
                "phonk_rhythm",
                "old_school_beat",
                "dirty_south_drums",
                # Rock
                "rock_drums",
                "rock_beat",
                "rock_percussion",
                "indie_drums",
                "alternative_beat",
                "drum_kit",
                "live_drums",
                "acoustic_drums",
                "band_percussion",
                "metal_drums",
                "punk_beat",
                "progressive_rock_drums",
                "psychedelic_percussion",
                "garage_rock_beat",
                # Général
                "drum_loop",
                "percussion_loop",
                "beat_pattern",
                "rhythm_section",
                "groove_loop",
                "drum_pattern",
                "rhythm_loop",
                "percussion_pattern",
                "beat_section",
                "metronome",
                "click_track",
                "polyrhythm_loop",
                # Styles spécifiques
                "glitch_perc",
                "glitch_drums",
                "experimental_percussion",
                "abstract_beat",
                "jungle_drums",
                "dnb_beat",
                "breakcore_drums",
                "ambient_percussion",
                "industrial_beat",
                "noise_rhythm",
                "footwork_beat",
                "juke_drums",
                "uk_garage_beat",
                "2step_rhythm",
                "grime_drums",
                "dubstep_beat",
                "future_garage_rhythm",
                "neuro_drums",
                "liquid_breaks",
                # Dub/Reggae
                "dub_riddim",
                "dub_drums",
                "dub_percussion",
                "dub_beat",
                "dub_steppers",
                "reggae_riddim",
                "reggae_drums",
                "reggae_percussion",
                "steppers_beat",
                "one_drop_drums",
                "rockers_rhythm",
                "nyabinghi_drums",
                "roots_percussion",
                "dub_echo_drums",
                "dub_one_drop",
                "dancehall_beat",
                "roots_beat",
                "ska_drums",
                "rocksteady_rhythm",
                # Deep House Spécifique
                "deep_house_beat",
                "jazzy_house_drums",
                "soulful_rhythm",
                "organic_house_percussion",
                "lounge_house_beat",
                "afro_deep_percussion",
                "nu_jazz_drums",
                "downtempo_house_beat",
                "spiritual_house_percussion",
                "mellow_house_rhythm",
                "detroit_house_beat",
                "rhodes_house_groove",
                # Autres Genres
                "trance_percussion",
                "hardstyle_kick",
                "gabber_drums",
                "hardcore_beat",
                "psytrance_rhythm",
                "goa_beat",
                "idm_drums",
                "ambient_beat",
                "downtempo_rhythm",
                "trip_hop_drums",
                "chillout_percussion",
                "balearic_rhythm",
                "latin_percussion",
                "afrobeat_drums",
                "world_percussion",
                "fusion_rhythm",
                "cinematic_drums",
                "orchestral_percussion",
                "big_band_drums",
                "jazz_rhythm",
                "funk_beat",
                "soul_groove",
                "disco_beat",
                "vaporwave_drums",
                "synthwave_percussion",
                "8bit_beat",  # Dans la liste rhythm_types, ajouter:
                "triphop_beat",
                "triphop_drums",
                "triphop_percussion",
                "triphop_rhythm",
                "broken_beat",
                "vinyl_drums",
                "scratchy_beat",
            ]

            # Liste étendue des mots-clés qui suggèrent un élément rythmique
            rhythm_keywords = [
                # Éléments de batterie
                "kick",
                "drum",
                "snare",
                "hat",
                "hihat",
                "hi-hat",
                "cymbal",
                "clap",
                "rim",
                "tom",
                "conga",
                "bongo",
                "shaker",
                "tambourine",
                "cowbell",
                "woodblock",
                "ride",
                "crash",
                "stick",
                "brushes",
                "clave",
                "timbale",
                "djembe",
                "tabla",
                "cajon",
                "guiro",
                "maracas",
                # Types de patterns
                "beat",
                "rhythm",
                "groove",
                "percussion",
                "loop",
                "break",
                "pattern",
                "fill",
                "shuffle",
                "swing",
                "cadence",
                "pulse",
                "meter",
                "measure",
                "downbeat",
                "upbeat",
                "backbeat",
                "accent",
                "ghost note",
                "syncopation",
                # Descriptions spécifiques
                "808",
                "909",
                "606",
                "707",
                "727",
                "LinnDrum",
                "DMX",
                "kit",
                "top",
                "backbeat",
                "downbeat",
                "bpm",
                "tempo",
                "rolls",
                "flam",
                "paradiddle",
                "drag",
                "rimshot",
                "sidestick",
                "cross-stick",
                # Technologies/terminologie
                "sequenced",
                "programmed",
                "quantized",
                "acoustic",
                "electronic",
                "sampled",
                "live",
                "triggered",
                "MIDI",
                "velocity",
                "dynamic",
                "transient",
                "attack",
                "decay",
                "sustain",
                "release",
                "envelope",
                "modulation",
                "automation",
                # Styles rythmiques
                "four-on-floor",
                "breakbeat",
                "footwork",
                "swing",
                "shuffle",
                "trap",
                "boom-bap",
                "jungle",
                "dubstep",
                "step",
                "riddim",
                "polyrhythm",
                "syncopated",
                "riddim",
                "steppers",
                "one_drop",
                "rockers",
                "nyabinghi",
                "skank",
                "bubble",
                "roots",
                "dembow",
                "clave",
                "son",
                "tresillo",
                "cinquillo",
                # Deep House Spécifique
                "jazzy",
                "soulful",
                "organic",
                "vinyl",
                "analogue",
                "analog",
                "dusty",
                "lofi",
                "warm",
                "mellow",
                "smooth",
                "rhodes",
                "finger_snap",
                "brushed",
                "subtle",
                "gentle",
                "crisp",
                "tight",
                # Autres termes rythmiques
                "metric",
                "compound",
                "simple",
                "triple",
                "duple",
                "quadruple",
                "half-time",
                "double-time",
                "off-beat",
                "on-beat",
                "drop",
                "build",
                "breakdown",
                "build-up",
                "crescendo",
                "diminuendo",
                "cross-rhythm",
                "polymeter",
                "hemiola",
                "isorhythm",
                "ostinato",
                "groove",
                "pocket",
                "time-feel",
                "timing",
                "micro-timing",
                "humanized",
                "machine",
                "robotic",
                "tribal",
                "ethnic",
                "world",
                "fusion",
            ]
            is_rhythm_layer = sample_type in rhythm_types or any(
                keyword in sample_type for keyword in rhythm_keywords
            )

            # Si c'est un layer rythmique, vérifier s'il y en a déjà un actif
            if is_rhythm_layer:
                # Identifier tous les layers rythmiques actuellement actifs
                rhythm_layers = []
                for l_id, layer in self.layers.items():
                    # Accéder au type via sample_details si disponible
                    if hasattr(layer, "type"):
                        layer_type = layer.type.lower()
                        if layer_type in rhythm_types or any(
                            keyword in layer_type for keyword in rhythm_keywords
                        ):
                            rhythm_layers.append(l_id)

                # S'il y a déjà des layers rythmiques (et ce n'est pas un remplacement)
                if rhythm_layers and layer_id not in rhythm_layers:
                    rhythm_layer_to_remove = rhythm_layers[0]  # Prendre le premier
                    print(
                        f"Un seul layer rythmique autorisé. Remplacement de '{rhythm_layer_to_remove}' par '{layer_id}'"
                    )

                    # Supprimer l'ancien layer rythmique
                    layer_to_remove = self.layers.pop(rhythm_layer_to_remove)
                    layer_to_remove.stop(fadeout_ms=200, cleanup=True)
                    print(
                        f"Layer rythmique '{rhythm_layer_to_remove}' automatiquement retiré"
                    )

            # 1. Préparer le sample pour la boucle (trim, calage en durée)
            looped_sample_path = self._prepare_sample_for_loop(
                original_file_path, layer_id, measures
            )
            if not looped_sample_path:
                print(f"Échec de la préparation de la boucle pour {layer_id}.")
                return

            # 2. Appliquer les effets (si demandé)
            final_sample_path = self._apply_effects_to_sample(
                looped_sample_path, effects or [], layer_id
            )
            if not final_sample_path:
                print(f"Échec de l'application des effets pour {layer_id}.")
                # On pourrait décider de jouer le sample non-effecté ou d'annuler
                final_sample_path = looped_sample_path  # Jouer sans effets dans ce cas
            if (
                os.path.exists(original_file_path)
                and final_sample_path != original_file_path
            ):
                try:
                    os.remove(original_file_path)
                    print(f"Fichier audio original supprimé: {original_file_path}")
                except (PermissionError, OSError) as e:
                    print(
                        f"Impossible de supprimer le fichier original {original_file_path}: {e}"
                    )
            if (
                os.path.exists(looped_sample_path)
                and final_sample_path != looped_sample_path
                and final_sample_path != original_file_path
            ):
                try:
                    os.remove(looped_sample_path)
                    print(
                        f"Fichier audio bouclé intermédiaire supprimé: {looped_sample_path}"
                    )
                except (PermissionError, OSError) as e:
                    print(
                        f"Impossible de supprimer le fichier bouclé {looped_sample_path}: {e}"
                    )
            # Attendre le point de synchronisation
            start_sync_beats = playback_params.get(
                "start_beats_sync", BEATS_PER_BAR
            )  # Par défaut, sur la prochaine mesure
            self._wait_for_sync_point(beats_to_wait=start_sync_beats)

            # Si le layer existe déjà, l'arrêter avant de le remplacer
            if layer_id in self.layers:
                self.layers[layer_id].stop(
                    fadeout_ms=50, cleanup=True
                )  # Petit fade pour éviter clic
                print(f"Layer '{layer_id}' existant arrêté pour remplacement.")
                # Important: libérer le canal? Ou réutiliser le même?
                # Pour l'instant, on réutilise le canal si possible, sinon on en prend un nouveau.
                channel = self.layers[layer_id].channel
            else:
                channel = self._get_available_channel()

            if channel is None:
                print(
                    f"Impossible d'ajouter/remplacer le layer '{layer_id}', aucun canal disponible."
                )
                return

            max_active_layers = 3
            if len(self.layers) >= max_active_layers and layer_id not in self.layers:
                print(
                    f"Limite de {max_active_layers} layers atteinte. Sélection d'un layer à supprimer..."
                )
                # Liste des layers non essentiels (exclut kicks et basses)
                non_essential_layers = [
                    l_id
                    for l_id in self.layers.keys()
                    if not (l_id.startswith("kick") or l_id.startswith("bass"))
                ]
                if non_essential_layers:
                    # Choisir aléatoirement parmi les layers non essentiels
                    layer_to_remove_id = random.choice(non_essential_layers)
                    print(f"Layers non essentiels disponibles: {non_essential_layers}")
                else:
                    # S'il n'y a que des kicks/basses, prendre un layer aléatoire
                    layer_to_remove_id = random.choice(list(self.layers.keys()))
                    print("Pas de layers non essentiels, choix aléatoire forcé.")
                print(
                    f"Suppression aléatoire du layer '{layer_to_remove_id}' pour faire de la place."
                )
                # Suppression avec fade out et nettoyage du fichier
                layer_to_remove = self.layers.pop(layer_to_remove_id)
                layer_to_remove.stop(
                    fadeout_ms=200, cleanup=True
                )  # Ajout du paramètre cleanup=True
                # Informer le LLM ou les logs
                print(
                    f"Layer '{layer_to_remove_id}' automatiquement retiré pour respecter la limite de {max_active_layers} layers."
                )

            new_layer = AudioLayer(
                layer_id,
                final_sample_path,
                channel,
                volume=playback_params.get("volume", 0.8),
                pan=playback_params.get("pan", 0.0),
            )

            if new_layer.sound_object:  # Vérifier si le son a bien été chargé
                self.layers[layer_id] = new_layer
                new_layer.play()
                if (
                    not self.is_master_clock_running or len(self.layers) == 1
                ):  # Si c'est le premier layer à jouer
                    self.global_playback_start_time = time.time() - (
                        (time.time() - self.global_playback_start_time)
                        % ((60.0 / self.master_tempo) * BEATS_PER_BAR)
                        if self.global_playback_start_time
                        else 0
                    )  # Recalibrer l'heure de début si on a attendu
                    self.is_master_clock_running = True
            else:
                print(
                    f"Layer '{layer_id}' n'a pas pu être créé car le son n'a pas été chargé."
                )

        elif operation == "modify":
            if layer_id not in self.layers:
                print(f"Erreur: Layer '{layer_id}' non trouvé pour modification.")
                return

            layer = self.layers[layer_id]
            if playback_params:
                if "volume" in playback_params:
                    layer.set_volume(playback_params["volume"])
                    print(
                        f"Layer '{layer_id}': Volume mis à {playback_params['volume']}."
                    )
                if "pan" in playback_params:
                    layer.set_pan(playback_params["pan"])
                    print(f"Layer '{layer_id}': Pan mis à {playback_params['pan']}.")

            if effects:  # La modification d'effets en temps réel est plus complexe
                print(
                    f"Layer '{layer_id}': Modification d'effets demandée. Cela nécessiterait de re-traiter et re-jouer le sample. Non implémenté dynamiquement de manière fluide pour l'instant."
                )
                # Pour une vraie modification d'effet, il faudrait:
                # 1. Garder le chemin du sample *original non bouclé et non effecté*
                # 2. Arrêter le layer actuel (synchronisé)
                # 3. Re-préparer la boucle à partir de l'original
                # 4. Appliquer la *nouvelle chaîne d'effets complète*
                # 5. Rejouer (synchronisé)
                # C'est lourd et peut causer une coupure.

        elif operation == "remove":
            if layer_id not in self.layers:
                print(f"Erreur: Layer '{layer_id}' non trouvé pour suppression.")
                return

            layer_to_remove = self.layers.pop(layer_id)  # Retirer du dictionnaire

            stop_sync_beats = BEATS_PER_BAR  # Par défaut, arrêt sur la prochaine mesure
            fade_duration = 100  # ms
            if stop_behavior == "immediate":
                layer_to_remove.stop(cleanup=True)
            elif stop_behavior == "fade_out_bar":
                self._wait_for_sync_point(beats_to_wait=stop_sync_beats)
                layer_to_remove.stop(fadeout_ms=fade_duration, cleanup=True)
            elif (
                stop_behavior == "next_bar_end"
            ):  # Arrêt à la fin de la mesure en cours
                self._wait_for_sync_point(beats_to_wait=stop_sync_beats)
                layer_to_remove.stop(cleanup=True)
            else:  # Comportement par défaut
                self._wait_for_sync_point(beats_to_wait=stop_sync_beats)
                layer_to_remove.stop(fadeout_ms=fade_duration, cleanup=True)

        else:
            print(f"Opération inconnue '{operation}' pour le layer manager.")

    def set_master_tempo(self, new_tempo: float):
        """Change le tempo maître. ATTENTION: Ne re-pitche pas les samples en cours."""
        if new_tempo > 50 and new_tempo < 300:  # Limites raisonnables
            print(
                f"Changement du tempo maître de {self.master_tempo} BPM à {new_tempo} BPM."
            )
            self.master_tempo = new_tempo
            # Note: Changer le tempo n'affecte pas la vitesse de lecture des samples déjà joués avec Pygame.
            # Pour un vrai time-stretching/pitch-shifting en temps réel, il faudrait des outils plus avancés.
            # Le LLM devrait être conscient de cela et peut-être demander de remplacer les layers
            # avec de nouveaux samples générés au nouveau tempo.
        else:
            print(f"Tempo invalide: {new_tempo}. Doit être entre 50 et 300 BPM.")

    def stop_all_layers(self, fade_ms: int = 200):
        """Arrête tous les layers en cours avec un fadeout."""
        print(f"Arrêt de tous les layers avec un fadeout de {fade_ms}ms...")

        # Parcourir tous les layers actifs et les arrêter
        for layer_id in list(self.layers.keys()):
            if layer_id in self.layers:  # Vérifier qu'il existe toujours
                layer = self.layers.pop(layer_id)  # Retirer du dictionnaire

                # Faire un fadeout et nettoyer
                if layer.is_playing:
                    layer.stop(fadeout_ms=fade_ms, cleanup=True)

        # Réinitialiser l'état du gestionnaire de layers
        self.layers.clear()
        self.is_master_clock_running = False
        self.global_playback_start_time = None

        # Forcer le garbage collector pour libérer la mémoire
        import gc

        gc.collect()

        print("Tous les layers ont été arrêtés et nettoyés.")
