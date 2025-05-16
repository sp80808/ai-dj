import numpy as np
import librosa
import pyrubberband as pyrb
from pedalboard import Pedalboard, Gain, Reverb, Delay, Compressor, LowpassFilter
import soundfile as sf


class AudioProcessor:
    """Traitement et mixage audio avec effets et synchronisation"""

    def __init__(self, sample_rate=32000):
        """
        Initialise le processeur audio

        Args:
            sample_rate (int): Taux d'échantillonnage en Hz
        """
        self.sample_rate = sample_rate
        self.active_tracks = []
        self.master_tempo = 125  # BPM par défaut

        # Effets audio disponibles
        self.effects = {
            "reverb": Reverb(wet_level=0.2, room_size=0.5),
            "delay": Delay(delay_seconds=0.25, feedback=0.4, mix=0.3),
            "filter_low": LowpassFilter(cutoff_frequency_hz=500),
            "compressor": Compressor(threshold_db=-20, ratio=4),
            "gain": Gain(gain_db=0),
        }

        # Pedalboard principal (chaîne d'effets master)
        self.master_fx = Pedalboard(
            [Compressor(threshold_db=-20, ratio=2), Gain(gain_db=0)]
        )
        self.continuous_output = np.zeros(0)  # Buffer de sortie continu
        self.playback_position = 0  # Position de lecture actuelle
        self.buffer_size = sample_rate * 4  # 4 secondes de buffer (ajustable)

        # Créer un buffer initial de silence
        self.continuous_output = np.zeros(self.buffer_size)

    def process_sample(self, sample_audio, sample_info):
        """
        Traite un sample pour le synchroniser et préparer au mixage

        Args:
            sample_audio (np.array): Audio du sample
            sample_info (dict): Métadonnées du sample

        Returns:
            np.array: Sample traité
        """
        # Synchroniser le tempo si nécessaire
        if sample_info["tempo"] != self.master_tempo:
            ratio = self.master_tempo / sample_info["tempo"]
            sample_audio = pyrb.time_stretch(sample_audio, self.sample_rate, ratio)

        # Calculer la durée d'une mesure en échantillons
        beat_duration = 60.0 / self.master_tempo  # durée d'un temps en secondes
        bar_duration = beat_duration * 4  # durée d'une mesure (4 temps) en secondes
        bar_samples = int(bar_duration * self.sample_rate)

        # Pour les samples rythmiques qui doivent commencer sur le kick
        if sample_info.get("should_start_with_kick", False):
            # Détecter le premier kick et aligner
            processed_audio = self._align_to_kick(sample_audio)

            # S'assurer que la longueur est un multiple de mesures
            # pour permettre une boucle parfaite
            remainder = len(processed_audio) % bar_samples
            if remainder > 0:
                # Compléter pour avoir un nombre entier de mesures
                padding = bar_samples - remainder
                processed_audio = np.pad(processed_audio, (0, padding))
        else:
            # Pour les autres samples (pads, fx, etc.)
            processed_audio = sample_audio

        return processed_audio

    def add_track(self, audio, info, effects=None):
        """
        Ajoute un nouveau track au mix de manière synchronisée

        Args:
            audio (np.array): Audio du sample
            info (dict): Métadonnées du sample
            effects (list): Liste d'effets à appliquer
        """
        # Traiter le sample
        processed = self.process_sample(audio, info)

        # Appliquer les effets si spécifiés
        if effects:
            for effect in effects:
                processed = self.apply_effect(
                    processed, effect["name"], effect.get("params", {})
                )

        # Pour les samples rythmiques, synchroniser avec le mix actuel
        # pour éviter les coupures abruptes
        if info.get("should_start_with_kick", False) and self.active_tracks:
            # Calculer combien de temps jusqu'à la prochaine mesure
            # (simplifié: on attend la fin de la mesure actuelle)
            beat_duration = 60.0 / self.master_tempo
            bar_duration = beat_duration * 4
            bar_samples = int(bar_duration * self.sample_rate)

            # Préparer le sample pour commencer à la prochaine mesure complète
            # dans le buffer continu
            current_pos = self.playback_position
            samples_to_next_bar = bar_samples - (current_pos % bar_samples)

            # Si on est presque à la fin d'une mesure, attendre une mesure de plus
            # pour avoir le temps de préparer
            if samples_to_next_bar < self.sample_rate * 0.5:  # moins de 0.5 sec
                samples_to_next_bar += bar_samples

            # Log pour debug
            print(
                f"Sample sera introduit dans {samples_to_next_bar/self.sample_rate:.2f} secondes"
            )

        # Ajouter à la liste des tracks actifs
        self.active_tracks.append(
            {
                "audio": processed,
                "info": info,
                "effects": effects or [],
                "volume": 1.0,
                "active": True,
                "loop": True,  # Par défaut, tous les samples tournent en boucle
            }
        )

    def _align_to_kick(self, audio):
        """Trouve le premier kick et aligne le sample"""
        # Détecter l'onset (attaque) du premier kick
        onset_env = librosa.onset.onset_strength(
            y=audio, sr=self.sample_rate, hop_length=512, aggregate=np.median
        )

        # Trouver les onsets significatifs
        peaks = librosa.util.peak_pick(
            onset_env, pre_max=3, post_max=3, pre_avg=3, post_avg=5, delta=0.2, wait=1
        )

        if len(peaks) > 0:
            # Prendre le premier onset significatif
            first_kick = peaks[0] * 512  # hop_length

            # Si l'onset est trop loin du début, on coupe
            if first_kick < len(audio) * 0.25:  # Dans le premier quart
                return audio[first_kick:]

        # Si aucun kick trouvé ou trop loin, retourner tel quel
        return audio

    def apply_effect(self, audio, effect_name, params=None):
        """Applique un effet audio"""
        if effect_name not in self.effects:
            return audio

        # Créer une copie de l'effet pour ne pas modifier l'original
        effect = self.effects[effect_name]

        # Mettre à jour les paramètres si fournis
        if params and hasattr(effect, "set_params"):
            effect.set_params(**params)

        # Créer un Pedalboard temporaire avec cet effet
        fx_chain = Pedalboard([effect])

        # Appliquer l'effet
        processed = fx_chain(audio, self.sample_rate)

        return processed

    def add_track(self, audio, info, effects=None):
        """
        Ajoute un nouveau track au mix

        Args:
            audio (np.array): Audio du sample
            info (dict): Métadonnées du sample
            effects (list): Liste d'effets à appliquer
        """
        # Traiter le sample
        processed = self.process_sample(audio, info)

        # Appliquer les effets si spécifiés
        if effects:
            for effect in effects:
                processed = self.apply_effect(
                    processed, effect["name"], effect.get("params", {})
                )

        # Ajouter à la liste des tracks actifs
        self.active_tracks.append(
            {
                "audio": processed,
                "info": info,
                "effects": effects or [],
                "volume": 1.0,
                "active": True,
            }
        )

    def create_transition(
        self, from_track_index, to_track_index, transition_type, duration_sec=8.0
    ):
        """
        Crée une transition DJ entre deux tracks

        Args:
            from_track_index (int): Index du track source
            to_track_index (int): Index du track destination
            transition_type (str): Type de transition (crossfade, filter_fade, cut, etc.)
            duration_sec (float): Durée de la transition en secondes
        """
        if (
            from_track_index < 0
            or from_track_index >= len(self.active_tracks)
            or to_track_index < 0
            or to_track_index >= len(self.active_tracks)
        ):
            print("Indices de tracks invalides pour la transition")
            return

        # Tracks source et destination
        from_track = self.active_tracks[from_track_index]
        to_track = self.active_tracks[to_track_index]

        # S'assurer que les deux tracks sont actifs
        if not from_track["active"] or not to_track["active"]:
            print("Les deux tracks doivent être actifs pour une transition")
            return

        # Durée en échantillons
        duration_samples = int(duration_sec * self.sample_rate)

        # Traitement selon le type de transition
        if transition_type == "crossfade":
            # Crossfade simple - diminuer progressivement from_track et augmenter to_track
            steps = 20  # Nombre d'étapes
            from_vol = from_track["volume"]
            to_vol = to_track["volume"]

            # Planifier les changements de volume
            self.scheduled_transitions.append(
                {
                    "type": "crossfade",
                    "from_track": from_track_index,
                    "to_track": to_track_index,
                    "steps": steps,
                    "current_step": 0,
                    "from_vol": from_vol,
                    "to_vol": to_vol,
                    "step_duration": duration_samples // steps,
                }
            )

        elif transition_type == "filter_fade":
            # Transition avec filtre - appliquer un filtre passe-bas au from_track
            # et l'ouvrir progressivement sur le to_track
            from_track_effects = from_track.get("effects", []).copy()
            to_track_effects = to_track.get("effects", []).copy()

            # Ajouter un filtre passe-bas au from_track
            from_track_effects.append(
                {
                    "name": "filter_low",
                    "params": {"cutoff_frequency_hz": 20000},  # Commencer ouvert
                }
            )

            # Ajouter un filtre passe-bas au to_track (commencer fermé)
            to_track_effects.append(
                {
                    "name": "filter_low",
                    "params": {"cutoff_frequency_hz": 300},  # Commencer presque fermé
                }
            )

            # Mettre à jour les effets
            self.update_track(from_track_index, effects=from_track_effects)
            self.update_track(to_track_index, effects=to_track_effects)

            # Planifier les changements de filtre
            self.scheduled_transitions.append(
                {
                    "type": "filter_fade",
                    "from_track": from_track_index,
                    "to_track": to_track_index,
                    "steps": 30,  # Plus d'étapes pour un fondu plus fluide
                    "current_step": 0,
                    "step_duration": duration_samples // 30,
                    "from_cutoff_start": 20000,
                    "from_cutoff_end": 300,
                    "to_cutoff_start": 300,
                    "to_cutoff_end": 20000,
                }
            )

        elif transition_type == "cut":
            # Cut direct - désactiver le from_track immédiatement
            self.update_track(from_track_index, active=False)

        elif transition_type == "echo_out":
            # Ajouter un delay au from_track et le fade out
            from_track_effects = from_track.get("effects", []).copy()

            # Ajouter un delay avec feedback élevé
            from_track_effects.append(
                {
                    "name": "delay",
                    "params": {"delay_seconds": 0.5, "feedback": 0.7, "mix": 0.6},
                }
            )

            # Mettre à jour les effets
            self.update_track(from_track_index, effects=from_track_effects)

            # Planifier un fade out
            steps = 15
            self.scheduled_transitions.append(
                {
                    "type": "fade_out",
                    "track": from_track_index,
                    "steps": steps,
                    "current_step": 0,
                    "start_vol": from_track["volume"],
                    "step_duration": duration_samples // steps,
                }
            )

        print(
            f"Transition {transition_type} programmée entre tracks {from_track_index} et {to_track_index}"
        )

    # Nouvelle méthode pour traiter les transitions programmées
    def process_scheduled_transitions(self):
        """Traite les transitions programmées"""
        if not hasattr(self, "scheduled_transitions"):
            self.scheduled_transitions = []

        # Traiter chaque transition
        completed = []
        for i, transition in enumerate(self.scheduled_transitions):
            # Augmenter l'étape courante
            transition["current_step"] += 1

            if transition["type"] == "crossfade":
                # Transition par crossfade
                progress = transition["current_step"] / transition["steps"]
                from_vol = transition["from_vol"] * (1 - progress)
                to_vol = transition["to_vol"] * progress

                # Mettre à jour les volumes
                self.update_track(transition["from_track"], volume=from_vol)
                self.update_track(transition["to_track"], volume=to_vol)

                # Si terminé, désactiver le track source
                if transition["current_step"] >= transition["steps"]:
                    self.update_track(transition["from_track"], active=False)
                    completed.append(i)

            elif transition["type"] == "filter_fade":
                # Transition par filtre
                progress = transition["current_step"] / transition["steps"]

                # Calculer les fréquences de coupure actuelles
                from_cutoff = (
                    transition["from_cutoff_start"] * (1 - progress)
                    + transition["from_cutoff_end"] * progress
                )
                to_cutoff = (
                    transition["to_cutoff_start"] * (1 - progress)
                    + transition["to_cutoff_end"] * progress
                )

                # Mettre à jour les filtres
                from_track = self.active_tracks[transition["from_track"]]
                to_track = self.active_tracks[transition["to_track"]]

                # Trouver l'index du filtre
                for j, effect in enumerate(from_track["effects"]):
                    if effect["name"] == "filter_low":
                        from_track["effects"][j]["params"][
                            "cutoff_frequency_hz"
                        ] = from_cutoff
                        break

                for j, effect in enumerate(to_track["effects"]):
                    if effect["name"] == "filter_low":
                        to_track["effects"][j]["params"][
                            "cutoff_frequency_hz"
                        ] = to_cutoff
                        break

                # Si terminé, désactiver le track source et enlever les filtres
                if transition["current_step"] >= transition["steps"]:
                    self.update_track(transition["from_track"], active=False)

                    # Enlever le filtre du track de destination
                    to_track_effects = [
                        e for e in to_track["effects"] if e["name"] != "filter_low"
                    ]
                    self.update_track(transition["to_track"], effects=to_track_effects)

                    completed.append(i)

            elif transition["type"] == "fade_out":
                # Fade out simple
                progress = transition["current_step"] / transition["steps"]
                volume = transition["start_vol"] * (1 - progress)

                # Mettre à jour le volume
                self.update_track(transition["track"], volume=volume)

                # Si terminé, désactiver le track
                if transition["current_step"] >= transition["steps"]:
                    self.update_track(transition["track"], active=False)
                    completed.append(i)

            elif transition["type"] == "filter_sweep":
                # Automation de filtre: changement progressif de fréquence de coupure
                progress = transition["current_step"] / transition["steps"]

                # Calculer la fréquence de coupure actuelle
                cutoff = (
                    transition["cutoff_start"] * (1 - progress)
                    + transition["cutoff_end"] * progress
                )

                # Appliquer à tous les tracks qui ont un filtre
                for track in self.active_tracks:
                    if track["active"]:
                        for effect in track["effects"]:
                            if effect["name"] in ["filter_low", "filter_high"]:
                                effect["params"]["cutoff_frequency_hz"] = cutoff

                # Si terminé
                if transition["current_step"] >= transition["steps"]:
                    completed.append(i)

        # Supprimer les transitions terminées (dans l'ordre inverse pour éviter les problèmes d'index)
        for i in sorted(completed, reverse=True):
            del self.scheduled_transitions[i]

    def mix_tracks(self):
        """
        Mixe tous les tracks actifs en mode boucle

        Returns:
            np.array: Mix audio final d'une durée fixe
        """
        if not self.active_tracks:
            return np.zeros(self.sample_rate * 4)  # 4 secondes de silence

        # Durée fixe de sortie (4 mesures par défaut)
        beat_duration = 60.0 / self.master_tempo
        bar_duration = beat_duration * 4
        output_bars = 4  # Nombre de mesures à générer
        output_duration = bar_duration * output_bars
        output_samples = int(output_duration * self.sample_rate)

        # Créer un buffer de sortie
        mix = np.zeros(output_samples)

        # Position actuelle dans le mix
        # (utilisée pour les effets temporels comme le filtre sweep)
        current_position = self.playback_position

        # Ajouter chaque track
        active_count = 0
        for track in self.active_tracks:
            if track["active"]:
                # Récupérer l'audio du track
                audio = track["audio"]

                # Si le track est plus court que la sortie et en mode boucle
                if len(audio) < output_samples and track.get("loop", True):
                    # Calculer combien de répétitions sont nécessaires
                    repeats = (output_samples // len(audio)) + 1
                    # Répéter l'audio
                    audio_repeated = np.tile(audio, repeats)
                    # Tronquer à la longueur de sortie
                    audio_to_mix = audio_repeated[:output_samples]
                else:
                    # Utiliser l'audio tel quel
                    audio_to_mix = audio[:output_samples]

                    # Si l'audio est plus court que la sortie, le compléter avec des zéros
                    if len(audio_to_mix) < output_samples:
                        audio_to_mix = np.pad(
                            audio_to_mix, (0, output_samples - len(audio_to_mix))
                        )

                # Ajuster le volume
                audio_to_mix = audio_to_mix * track["volume"]

                # Ajouter au mix
                mix += audio_to_mix
                active_count += 1

        # Normaliser pour éviter l'écrêtage
        if active_count > 0:
            # Facteur de mélange qui dépend du nombre de tracks
            # Plus il y a de tracks, plus on réduit le volume
            mix_factor = 1.0 / max(1.0, active_count * 0.7)
            mix = mix * mix_factor

            # Compresser légèrement le master
            mix = self.master_fx(mix, self.sample_rate)

            # Normaliser entre -1 et 1
            max_val = np.max(np.abs(mix))
            if max_val > 0:
                mix = mix / max_val * 0.9  # Légère marge

        return mix

    def update_track(self, track_index, volume=None, active=None, effects=None):
        """Met à jour les paramètres d'un track"""
        if track_index < 0 or track_index >= len(self.active_tracks):
            return

        track = self.active_tracks[track_index]

        if volume is not None:
            track["volume"] = max(0.0, min(1.0, volume))

        if active is not None:
            track["active"] = active

        if effects is not None:
            # Retraiter l'audio avec les nouveaux effets
            processed = self.process_sample(track["audio"], track["info"])

            for effect in effects:
                processed = self.apply_effect(
                    processed, effect["name"], effect.get("params", {})
                )

            track["audio"] = processed
            track["effects"] = effects

    def save_mix(self, filename, duration=None):
        """
        Sauvegarde le mix actuel dans un fichier

        Args:
            filename (str): Chemin du fichier de sortie
            duration (float, optional): Durée en secondes (tronque si nécessaire)
        """
        # Obtenir le mix actuel
        mix = self.mix_tracks()

        # Tronquer si nécessaire
        if duration and duration > 0:
            samples = int(duration * self.sample_rate)
            if len(mix) > samples:
                mix = mix[:samples]

        # Sauvegarder
        sf.write(filename, mix, self.sample_rate)

        return filename

    def get_next_audio_chunk(self, chunk_size):
        """
        Obtient le prochain chunk audio pour une lecture continue

        Args:
            chunk_size (int): Taille du chunk en échantillons

        Returns:
            np.array: Chunk audio pour lecture
        """
        # Si on approche de la fin du buffer, mixer et ajouter plus d'audio
        remaining = len(self.continuous_output) - self.playback_position

        if remaining < self.buffer_size / 2:
            # Obtenir le mix actuel
            current_mix = self.mix_tracks()

            # Si aucun track actif, générer du silence
            if len(current_mix) == 0:
                current_mix = np.zeros(self.buffer_size)

            # AJOUTER CE CODE: appliquer un fondu (crossfade) pour éviter les clics
            crossfade_len = min(1024, len(current_mix) // 4)  # ~23ms à 44.1kHz

            if len(self.continuous_output) > self.playback_position + crossfade_len:
                # Prendre les derniers échantillons pour le crossfade
                fade_out = np.linspace(1.0, 0.0, crossfade_len)
                fade_in = np.linspace(0.0, 1.0, crossfade_len)

                # Appliquer le crossfade
                end_samples = self.continuous_output[
                    self.playback_position : self.playback_position + crossfade_len
                ]
                start_samples = current_mix[:crossfade_len]

                crossfaded = (end_samples * fade_out) + (start_samples * fade_in)

                # Remplacer le début du nouveau buffer
                current_mix[:crossfade_len] = crossfaded

            # Ajouter au buffer continu
            self.continuous_output = np.concatenate(
                [self.continuous_output[self.playback_position :], current_mix]
            )

            # Réinitialiser la position
            self.playback_position = 0

        # Extraire le chunk actuel
        chunk = self.continuous_output[
            self.playback_position : self.playback_position + chunk_size
        ]

        # Mettre à jour la position
        self.playback_position += chunk_size

        # Si le chunk est trop petit (fin du buffer), le compléter avec des zéros
        if len(chunk) < chunk_size:
            chunk = np.pad(chunk, (0, chunk_size - len(chunk)))

        # AJOUTER: appliquer une légère normalisation pour éviter les clics
        if np.max(np.abs(chunk)) > 0:
            chunk = chunk / np.max(np.abs(chunk)) * 0.9

        return chunk

    def ensure_continuous_playback(self):
        """
        S'assure qu'il y a toujours du contenu audio en cours de lecture
        En vérifiant s'il y a des tracks rythmiques actifs (kick/bass)
        """
        # Vérifier si des tracks rythmiques sont actifs
        has_rhythmic_track = False
        for track in self.active_tracks:
            if track["active"] and track["info"]["type"] in [
                "techno_kick",
                "techno_bass",
                "techno_percussion",
            ]:
                has_rhythmic_track = True
                break

        # Si aucun track rythmique actif et qu'on a au moins une piste inactive
        if not has_rhythmic_track and len(self.active_tracks) > 0:
            # Chercher le dernier track rythmique inactif
            for i in range(len(self.active_tracks) - 1, -1, -1):
                track = self.active_tracks[i]
                if not track["active"] and track["info"]["type"] in [
                    "techno_kick",
                    "techno_bass",
                ]:
                    # Réactiver ce track pour assurer la continuité
                    print(
                        "Réactivation automatique d'un track rythmique pour continuité"
                    )
                    self.update_track(i, active=True, volume=0.8)
                    break
