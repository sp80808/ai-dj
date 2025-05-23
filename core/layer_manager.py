import os
import time
import random
import pygame
import librosa
import numpy as np
import soundfile as sf
from typing import Dict, List, Optional, Any
from core.audio_layer_sync import AudioLayerSync
from core.audio_layer import AudioLayer
from config.music_prompts import rhythm_keywords, rhythm_types
from config.config import BEATS_PER_BAR


class LayerManager:
    """Gère plusieurs layers audio, leur synchronisation et leurs effets de base."""

    def __init__(
        self,
        sample_rate: int = 44100,
        num_channels: int = 16,
        output_dir: str = "./dj_layers_output",
        on_max_layers_reached=None,
    ):
        self.sample_rate = sample_rate
        self.output_dir = output_dir
        self.on_max_layers_reached = on_max_layers_reached

        if not os.path.exists(self.output_dir):
            os.makedirs(self.output_dir)

        try:
            pygame.mixer.init(
                frequency=self.sample_rate, channels=2, buffer=4096  
            )  # Initialiser avec buffer plus grand
            pygame.mixer.set_num_channels(num_channels)
            print(
                f"🎵 Pygame Mixer initialisé avec {pygame.mixer.get_num_channels()} canaux.\n"
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
        """Trouve un canal vraiment libre"""
        for i in range(self.num_channels):
            channel = pygame.mixer.Channel(i)
            if not channel.get_busy():
                # Force le stop pour être sûr
                channel.stop()
                return channel
        
        # Si aucun canal libre, forcer l'arrêt du premier
        print("⚠️ Forçage libération canal 0")
        pygame.mixer.Channel(0).stop()
        return pygame.mixer.Channel(0)

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
        model_name="musicgen",
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

        # Détection d'onset
        if model_name in "musicgen":
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
                print(f"🔉 Layer '{layer_id}': HPF appliqué à {cutoff}Hz.")
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
                print(f"🔉 Layer '{layer_id}': LPF appliqué à {cutoff}Hz.")
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
            print("⏱️  Horloge maître non démarrée. Démarrage immédiat.")
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
                f"\n⏱️  Attente de synchronisation: {time_remaining_to_sync:.3f}s (Tempo: {self.master_tempo} BPM, intervalle de {beats_to_wait} beats)"
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
        model_name="musicgen",
        prepare_sample_for_loop=True,
        use_sync=False,
        midi_clock_manager=None,
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
            if prepare_sample_for_loop:
                looped_sample_path = self._prepare_sample_for_loop(
                    original_file_path,
                    layer_id,
                    measures,
                    model_name=model_name,
                )
            else:
                looped_sample_path = original_file_path
            if not looped_sample_path:
                print(f"Échec de la préparation de la boucle pour {layer_id}.")
                return

            if prepare_sample_for_loop:
                # 2. Appliquer les effets (si demandé)
                final_sample_path = self._apply_effects_to_sample(
                    looped_sample_path, effects or [], layer_id
                )
            else:
                final_sample_path = looped_sample_path
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
                    print(f"\n🗑️  Fichier audio original supprimé: {original_file_path}")
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
                        f"🧹 Fichier audio bouclé intermédiaire supprimé: {looped_sample_path}"
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
                channel = self.layers[layer_id].channel
                self.layers[layer_id].stop(fadeout_ms=20, cleanup=True)
                time.sleep(0.025) 
                channel.stop()
                del self.layers[layer_id]
                
                print(f"⏹️  Layer '{layer_id}' arrêté et nettoyé pour remplacement.")
                
            else:
                channel = self._get_available_channel()

            if channel is None:
                print(
                    f"Impossible d'ajouter/remplacer le layer '{layer_id}', aucun canal disponible."
                )
                return
            
            import gc
            gc.collect()

            max_active_layers = 3
            if len(self.layers) >= max_active_layers and layer_id not in self.layers:
                print(f"\n⚠️  Limite de {max_active_layers} layers atteinte.\n")

                # Utiliser le callback si disponible
                if self.on_max_layers_reached:
                    layer_info = {
                        layer_id: {
                            "type": getattr(layer, "type", "unknown"),
                            "used_stem": getattr(layer, "used_stem", None),
                        }
                        for layer_id, layer in self.layers.items()
                    }

                    # Appeler le callback avec les infos des layers
                    should_continue = self.on_max_layers_reached(layer_info)

                    if not should_continue:
                        print("Ajout du nouveau layer annulé par le callback.")
                        return

                    # Sinon, on continue et on ajoute quand même ce layer (temporairement à 4)
                else:
                    # Comportement par défaut (comme avant)
                    all_layers = list(self.layers.keys())
                    weights = [
                        3 if i == 0 else 2 if i < len(all_layers) // 2 else 1
                        for i in range(len(all_layers))
                    ]
                    layer_to_remove_id = random.choices(all_layers, weights=weights)[0]

                    print(
                        f"Suppression du layer '{layer_to_remove_id}' pour faire de la place."
                    )
                    layer_to_remove = self.layers.pop(layer_to_remove_id)
                    layer_to_remove.stop(fadeout_ms=200, cleanup=True)
                    print(f"Layer '{layer_to_remove_id}' automatiquement retiré.")

            used_stem_type = sample_details.get("used_stem") or "unknown"
            # On peut aussi extraire du layer_id si ça contient des infos (bass_layer, kick_layer, etc.)
            layer_type = "unknown"
            if "_" in layer_id:
                parts = layer_id.split("_")
                if len(parts) > 0:
                    layer_type = parts[0].lower()

            # Ajuster le volume en fonction du stem et du type de layer
            adjusted_volume = playback_params.get("volume", 0.9)

            # Réglages basés sur le stem extrait
            if used_stem_type == "drums":
                if "kick" in layer_type:
                    # Kick drum - bon volume
                    adjusted_volume *= 1.0
                    print(f"🥁 Volume du kick conservé à {adjusted_volume:.2f}")
                else:
                    # Autres percussions - légèrement réduit
                    adjusted_volume *= 0.85
                    print(f"🥁 Volume des percussions ajusté à {adjusted_volume:.2f}")

            elif used_stem_type == "bass":
                adjusted_volume *= 0.8
                print(f"🔉 Volume de basse réduit à {adjusted_volume:.2f}")

            elif used_stem_type == "other":
                # Éléments "other" - réglage standard avec légère réduction
                adjusted_volume *= 0.8
                print(f"🎹 Volume des éléments autres ajusté à {adjusted_volume:.2f}")

            elif used_stem_type == "vocals":
                # Éléments vocaux - assez fort pour être distincts
                adjusted_volume *= 0.9
                print(f"🎤 Volume des éléments vocaux ajusté à {adjusted_volume:.2f}")

            elif used_stem_type == "full_mix" or used_stem_type == "unknown":
                # Mix complet - réduction significative car contient tous les éléments
                adjusted_volume *= 0.7
                print(f"🎛️  Volume du mix complet ajusté à {adjusted_volume:.2f}")

            # Ensuite création du layer avec le volume ajusté
            if use_sync:
                new_layer = AudioLayerSync(
                    layer_id,
                    final_sample_path,
                    channel,
                    volume=adjusted_volume,  # Volume ajusté selon le stem
                    pan=playback_params.get("pan", 0.0),
                    midi_manager=midi_clock_manager,
                )
            else:
                new_layer = AudioLayer(
                    layer_id,
                    final_sample_path,
                    channel,
                    volume=adjusted_volume,  # Volume ajusté selon le stem
                    pan=playback_params.get("pan", 0.0),
                )

            if new_layer.sound_object:  # Vérifier si le son a bien été chargé
                self.layers[layer_id] = new_layer
                if not self.is_master_clock_running:
                    self.global_playback_start_time = time.time()
                    self.is_master_clock_running = True
                if use_sync:
                    new_layer.play()
                else:
                    new_layer.play(
                        grid_start_time=self.global_playback_start_time,
                        tempo=self.master_tempo,
                    )
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
                print(f"❌ Erreur: Layer '{layer_id}' non trouvé pour suppression.")
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
            import tempfile
            import subprocess
            import soundfile as sf
            import os

            if isinstance(audio, str):
                print(f"📂 Chargement du fichier audio: {audio}")
                try:
                    audio, file_sr = librosa.load(audio, sr=sr)
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

            # Si l'audio est stéréo, le convertir en mono
            if len(audio.shape) > 1 and audio.shape[1] > 1:
                print("⚠️  Audio stéréo détecté, conversion en mono...")
                audio = np.mean(audio, axis=1)

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
        import tempfile
        import subprocess
        import soundfile as sf
        import librosa
        import os

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
            stretched_audio, _ = librosa.load(temp_out.name, sr=sr)

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
