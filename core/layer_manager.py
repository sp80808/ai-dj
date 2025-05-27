import os
import tempfile
import subprocess
import librosa
import numpy as np
import soundfile as sf
from typing import Optional
from config.config import BEATS_PER_BAR


class LayerManager:
    """Gère plusieurs layers audio, leur synchronisation et leurs effets de base."""

    def __init__(
        self,
        output_dir,
        sample_rate: int = 48000,
        num_channels: int = 16,
        on_max_layers_reached=None,
    ):
        self.sample_rate = sample_rate
        self.output_dir = output_dir
        self.on_max_layers_reached = on_max_layers_reached
        self.operation_count = 0
        self.max_operations_before_reset = 4
        if not os.path.exists(self.output_dir):
            os.makedirs(self.output_dir)

        self.layers = {}
        self.channel_id_counter = 0
        self.master_tempo: float = 126.0  # BPM
        self.global_playback_start_time: Optional[float] = (
            None  # Heure de démarrage du tout premier sample
        )
        self.is_master_clock_running: bool = False
        self.num_channels = num_channels

    def find_kick_attack_start(self, audio, sr, onset_position, layer_id):
        """Trouve le vrai début de l'attaque du kick pour le préserver complètement"""

        # Chercher dans une fenêtre avant l'onset détecté
        search_window = int(sr * 0.15)  # 150ms avant l'onset
        search_start = max(0, onset_position - search_window)
        search_end = min(len(audio), onset_position + int(sr * 0.05))  # +50ms après
        search_segment = audio[search_start:search_end]

        if len(search_segment) < 100:  # Segment trop court
            return max(0, onset_position - int(sr * 0.02))  # Fallback 20ms avant

        # Calculer l'enveloppe d'énergie avec une résolution fine
        hop_length = 64  # Plus fin pour détecter précisément l'attaque
        rms = librosa.feature.rms(y=search_segment, hop_length=hop_length)[0]

        # Seuil adaptatif pour détecter le début de l'attaque
        max_energy = np.max(rms)
        baseline_energy = np.mean(
            rms[: len(rms) // 4]
        )  # Énergie de base (premier quart)
        threshold = (
            baseline_energy + (max_energy - baseline_energy) * 0.1
        )  # 10% au-dessus du baseline

        # Trouver le premier point où l'énergie dépasse le seuil
        attack_candidates = np.where(rms > threshold)[0]

        if len(attack_candidates) > 0:
            # Prendre le premier dépassement du seuil
            attack_start_frame = attack_candidates[0]

            # Convertir en samples absolus
            relative_sample = attack_start_frame * hop_length
            absolute_sample = search_start + relative_sample

            kick_duration_margin = int(sr * 0.3)
            final_start = max(0, absolute_sample - kick_duration_margin)

            print(
                f"🎯 Attaque kick détectée à {absolute_sample/sr:.3f}s, démarrage pour kick complet à {final_start/sr:.3f}s ('{layer_id}')"
            )
            return final_start

        conservative_start = max(0, onset_position - int(sr * 0.3))
        print(
            f"🤔 Attaque kick non détectée, démarrage conservateur à {conservative_start/sr:.3f}s ('{layer_id}')"
        )
        return conservative_start

    def _prepare_sample_for_loop(
        self,
        original_audio_path: str,
        layer_id: str,
        measures: int,
        time_stretch=True,
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
        target_total_samples = int(target_total_samples * 1.2)

        # Détection d'onset
        onset_env = librosa.onset.onset_strength(y=audio, sr=sr)
        onsets_samples = librosa.onset.onset_detect(
            onset_envelope=onset_env, sr=sr, units="samples", backtrack=False
        )

        start_offset_samples = 0

        if len(onsets_samples) > 0:
            # Chercher un onset dans les premières 200ms
            early_onsets = [o for o in onsets_samples if o < sr * 0.2]

            if early_onsets:
                detected_onset = early_onsets[0]
            else:
                detected_onset = onsets_samples[0]

            # Trouver l'attaque du kick
            start_offset_samples = self.find_kick_attack_start(
                audio, sr, detected_onset, layer_id
            )

            # Si le kick est vraiment tout au début (dans les 10ms), ne pas trimmer
            if start_offset_samples < sr * 0.01:  # 10ms
                print(
                    f"✅ Kick immédiat détecté ('{layer_id}'), pas de trim nécessaire"
                )
                start_offset_samples = 0

        else:
            print(f"⚠️  Aucun onset détecté pour '{layer_id}', démarrage sans trim")
            start_offset_samples = 0

        # Appliquer le trim intelligent
        if start_offset_samples > 0:
            print(
                f"✂️  Trim appliqué: {start_offset_samples/sr:.3f}s supprimées ('{layer_id}')"
            )
            audio = audio[start_offset_samples:]
        else:
            print(f"🎵 Aucun trim nécessaire pour '{layer_id}'")

        current_length = len(audio)
        if current_length == 0:
            print(f"❌ Erreur: Layer '{layer_id}' vide après trim.")

        current_length = len(audio)
        if current_length == 0:
            print(f"Erreur: Layer '{layer_id}' vide après trim.")
            return None

        if current_length > target_total_samples:
            fade_samples = int(sr * 0.1)  # 100ms
            audio[
                target_total_samples - fade_samples : target_total_samples
            ] *= np.linspace(1.0, 0.0, fade_samples)
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
        temp_path = os.path.join(self.output_dir, "temp_" + looped_sample_filename)
        looped_sample_path = os.path.join(self.output_dir, looped_sample_filename)

        # Sauvegarder d'abord une version temporaire
        sf.write(temp_path, audio, sr)

        try:
            if not time_stretch:
                stretched_audio = audio
            else:
                stretched_audio = self.match_sample_to_tempo(
                    temp_path,  # Le chemin du fichier temporaire
                    target_tempo=self.master_tempo,
                    sr=self.sample_rate,
                    preserve_measures=False,
                )

            if isinstance(stretched_audio, np.ndarray):
                # Si le résultat est un array, l'utiliser pour la sauvegarde finale
                sf.write(looped_sample_path, stretched_audio, sr)
                print(f"⏩ Sample bouclé avec tempo adapté: {looped_sample_path}")
            else:
                # Sinon, copier le fichier temporaire au bon endroit
                sf.write(looped_sample_path, audio, sr)
                print(
                    f"💾 Layer '{layer_id}': Sample bouclé sauvegardé : {looped_sample_path}"
                )

            # Supprimer le fichier temporaire
            if os.path.exists(temp_path):
                os.remove(temp_path)

            return looped_sample_path
        except Exception as e:
            print(f"Erreur de sauvegarde du sample bouclé pour {layer_id}: {e}")
            return None

    def set_master_tempo(self, new_tempo: float):
        """Change le tempo maître. ATTENTION: Ne re-pitche pas les samples en cours."""
        if new_tempo > 50 and new_tempo < 300:
            print(
                f"Changement du tempo maître de {self.master_tempo} BPM à {new_tempo} BPM."
            )
            self.master_tempo = new_tempo
        else:
            print(f"Tempo invalide: {new_tempo}. Doit être entre 50 et 300 BPM.")

    def match_sample_to_tempo(
        self, audio, target_tempo, sr, preserve_measures=True, beats_per_measure=4
    ):
        """
        Détecte le tempo d'un sample audio et l'adapte au tempo cible sans modifier sa hauteur.
        Utilise Rubber Band pour un time stretch de qualité professionnelle.

        Args:
            audio: Audio à adapter (numpy array ou tout objet convertible)
            target_tempo (float): Tempo cible en BPM
            sr (int): Taux d'échantillonnage
            preserve_measures (bool): Si True, préserve le nombre de mesures musicales
            beats_per_measure (int): Nombre de temps par mesure (généralement 4 pour 4/4)

        Returns:
            np.array: Audio adapté au nouveau tempo
        """
        try:
            import numpy as np
            import librosa

            if isinstance(audio, str):
                print(f"📂 Chargement du fichier audio: {audio}")
                try:
                    # ✅ soundfile au lieu de librosa
                    audio, file_sr = sf.read(audio, always_2d=False)

                    # Resampler si nécessaire
                    if file_sr != sr:
                        audio = librosa.resample(audio, orig_sr=file_sr, target_sr=sr)

                    print(f"✅ Fichier audio chargé: {audio.shape}, sr={file_sr}")
                except Exception as e:
                    print(f"❌ Échec du chargement du fichier: {e}")
                    return audio

            # S'assurer que l'audio est un numpy array
            if not isinstance(audio, np.ndarray):
                print(
                    f"⚠️  Conversion de l'audio en numpy array (type actuel: {type(audio)})"
                )
                try:
                    audio = np.array(audio, dtype=np.float32)
                except Exception as e:
                    print(f"❌ Échec de la conversion: {e}")
                    return audio

            # Vérifier que l'array contient des données
            if audio.size == 0:
                print("❌ L'audio est vide!")
                return audio

            print(f"ℹ️  Audio shape: {audio.shape}, dtype: {audio.dtype}")

            # Longueur originale en échantillons et en secondes
            original_length = len(audio)
            original_duration = original_length / sr

            # Étape 1: Estimer le tempo du sample
            onset_env = librosa.onset.onset_strength(y=audio, sr=sr)
            estimated_tempo = librosa.beat.tempo(onset_envelope=onset_env, sr=sr)[0]
            print(f"🎵 Tempo estimé du sample: {estimated_tempo:.1f} BPM")

            # Si le tempo estimé semble anormal, utiliser une valeur par défaut
            if estimated_tempo < 40 or estimated_tempo > 220:
                print(
                    f"⚠️  Tempo estimé peu plausible ({estimated_tempo:.1f} BPM), utilisation d'une valeur par défaut"
                )
                estimated_tempo = 120  # Tempo par défaut si l'estimation échoue

            # Si les tempos sont très proches, pas besoin de time stretching
            tempo_ratio = abs(estimated_tempo - target_tempo) / target_tempo
            if tempo_ratio < 0.02:  # Moins de 2% de différence
                print(
                    f"ℹ️  Tempos similaires ({estimated_tempo:.1f} vs {target_tempo:.1f} BPM), pas de stretching"
                )
                return audio

            # Calculer le ratio de time stretching
            stretch_ratio = estimated_tempo / target_tempo

            # === TIME STRETCHING AVEC RUBBER BAND ===
            print(f"🔧 Utilisation de Rubber Band pour le time stretch...")

            try:
                stretched_audio = self._time_stretch_rubberband(
                    audio, stretch_ratio, sr
                )
                print(f"✅ Rubber Band time stretch réussi")
            except Exception as rb_error:
                print(f"⚠️  Erreur Rubber Band: {rb_error}")
                print("🔄 Fallback vers librosa time stretch...")
                # Fallback vers librosa si Rubber Band échoue
                stretched_audio = librosa.effects.time_stretch(
                    audio, rate=stretch_ratio
                )

            stretched_length = len(stretched_audio)

            # Si on veut préserver le nombre de mesures musicales
            if preserve_measures:
                # Calculer le nombre de mesures dans l'audio original
                beats_in_original = (original_duration / 60.0) * estimated_tempo
                measures_in_original = beats_in_original / beats_per_measure

                # Arrondir au nombre entier de mesures le plus proche, au moins 1
                whole_measures = max(1, round(measures_in_original))

                print(
                    f"📏 Nombre estimé de mesures: {measures_in_original:.2f} → {whole_measures}"
                )

                # Calculer la durée idéale en nombre entier de mesures au nouveau tempo
                target_beats = whole_measures * beats_per_measure
                target_duration = (target_beats / target_tempo) * 60.0
                target_duration *= 1.2
                target_length = int(target_duration * sr)

                # Redimensionner l'audio adapté pour avoir un nombre exact de mesures
                if (
                    abs(target_length - stretched_length) > sr * 0.1
                ):  # Si différence > 100ms
                    print(
                        f"✂️  Ajustement à un nombre exact de mesures: {target_duration:.2f}s ({whole_measures} mesures)"
                    )

                    # Utiliser scipy pour une interpolation de meilleure qualité si disponible
                    try:
                        from scipy import signal

                        stretched_audio = signal.resample(
                            stretched_audio, target_length
                        )
                        print("🔬 Interpolation haute qualité avec scipy")
                    except ImportError:
                        # Fallback vers numpy interpolation
                        x_original = np.linspace(0, 1, stretched_length)
                        x_target = np.linspace(0, 1, target_length)
                        stretched_audio = np.interp(
                            x_target, x_original, stretched_audio
                        )
                        print("📐 Interpolation standard avec numpy")

            print(
                f"⏩ Time stretching appliqué : {estimated_tempo:.1f} → {target_tempo:.1f} BPM (ratio: {stretch_ratio:.2f})"
            )

            # Informations sur les changements de durée
            final_duration = len(stretched_audio) / sr
            print(f"⏱️  Durée: {original_duration:.2f}s → {final_duration:.2f}s")

            return stretched_audio

        except Exception as e:
            print(f"⚠️  Erreur lors du time stretching: {e}")
            import traceback

            traceback.print_exc()
            print("⚠️  Retour de l'audio original sans modification")
            return audio

    def _time_stretch_rubberband(self, audio, stretch_ratio, sr):
        """
        Time stretch avec Rubber Band CLI - qualité professionnelle

        Args:
            audio: Signal audio (numpy array)
            stretch_ratio: Ratio de stretch (estimated_tempo / target_tempo)
            sr: Sample rate

        Returns:
            np.array: Audio étiré temporellement
        """

        temp_files = []

        try:
            # Créer des fichiers temporaires
            temp_in = tempfile.NamedTemporaryFile(suffix=".wav", delete=False)
            temp_out = tempfile.NamedTemporaryFile(suffix=".wav", delete=False)
            temp_files = [temp_in.name, temp_out.name]

            # Sauvegarder l'audio d'entrée
            sf.write(temp_in.name, audio, sr)
            temp_in.close()

            # Construire la commande Rubber Band
            cmd = [
                "rubberband",
                "-t",
                str(stretch_ratio),  # Juste le time stretch
                temp_in.name,
                temp_out.name,
            ]

            print(f"🎛️  Commande Rubber Band: {' '.join(cmd)}")

            # Exécuter Rubber Band
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=30,  # Timeout de 30 secondes
            )

            if result.returncode != 0:
                raise Exception(
                    f"Rubber Band failed with code {result.returncode}: {result.stderr}"
                )

            temp_out.close()

            # Charger le résultat
            stretched_audio, _ = sf.read(temp_out.name, always_2d=False)

            print(f"🎯 Rubber Band: {len(audio)} → {len(stretched_audio)} samples")

            return stretched_audio

        except subprocess.TimeoutExpired:
            raise Exception("Rubber Band timeout (>30s)")
        except FileNotFoundError:
            raise Exception(
                "Rubber Band non trouvé. Installez-le : apt install rubberband-cli"
            )
        except Exception as e:
            raise Exception(f"Erreur Rubber Band: {str(e)}")
        finally:
            # Nettoyer les fichiers temporaires
            for temp_file in temp_files:
                try:
                    if os.path.exists(temp_file):
                        os.unlink(temp_file)
                except Exception as cleanup_error:
                    print(f"⚠️  Erreur nettoyage fichier {temp_file}: {cleanup_error}")

    def _time_stretch_pyrubberband(self, audio, stretch_ratio, sr):
        """
        Alternative avec pyrubberband (Python binding) - plus efficace

        Installé avec: pip install pyrubberband
        """
        try:
            import pyrubberband as pyrb

            # Options de qualité élevée
            options = (
                pyrb.RubberBandOption.OptionProcessingOffline
                | pyrb.RubberBandOption.OptionStretchHighQuality
                | pyrb.RubberBandOption.OptionTransientsCrisp  # Bon pour les drums
            )

            stretched_audio = pyrb.time_stretch(
                audio, sr, stretch_ratio, rbargs=options
            )

            print(f"🎯 PyRubberBand: {len(audio)} → {len(stretched_audio)} samples")
            return stretched_audio

        except ImportError:
            raise Exception(
                "pyrubberband non disponible. Installez avec: pip install pyrubberband"
            )
