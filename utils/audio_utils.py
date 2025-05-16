import numpy as np
import librosa
import soundfile as sf
from pydub import AudioSegment
import io
import tempfile
import os


def detect_bpm(audio, sr):
    """
    Détecte le BPM d'un fichier audio

    Args:
        audio (np.array): Signal audio
        sr (int): Taux d'échantillonnage

    Returns:
        float: BPM estimé
    """
    # Méthode 1: Utiliser librosa
    onset_env = librosa.onset.onset_strength(y=audio, sr=sr)
    tempo = librosa.beat.tempo(onset_envelope=onset_env, sr=sr)[0]

    return tempo


def detect_key(audio, sr):
    """
    Détecte la tonalité d'un fichier audio

    Args:
        audio (np.array): Signal audio
        sr (int): Taux d'échantillonnage

    Returns:
        str: Tonalité estimée
    """
    # Calculer les caractéristiques du chroma
    chroma = librosa.feature.chroma_cqt(y=audio, sr=sr)

    # Calculer le profil de tonalité
    chroma_avg = np.mean(chroma, axis=1)

    # Trouver la note la plus forte
    key_idx = np.argmax(chroma_avg)

    # Noms des tonalités
    key_names = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]

    # Déterminer si majeur ou mineur (heuristique simplifiée)
    minor_profile = librosa.feature.tonnetz(y=audio, sr=sr)
    minor_sum = np.sum(np.abs(minor_profile[3:]))
    major_sum = np.sum(np.abs(minor_profile[:3]))

    mode = "minor" if minor_sum > major_sum else "major"

    return f"{key_names[key_idx]} {mode}"


def beatmatch(audio, sr, target_bpm):
    """
    Ajuste le tempo d'un sample pour correspondre au BPM cible

    Args:
        audio (np.array): Signal audio
        sr (int): Taux d'échantillonnage
        target_bpm (float): BPM cible

    Returns:
        np.array: Audio adapté au tempo
    """
    # Détecter le BPM actuel
    current_bpm = detect_bpm(audio, sr)

    # Calculer le ratio de time-stretch
    ratio = target_bpm / current_bpm

    # Appliquer le time-stretch avec RubberBand
    import pyrubberband as pyrb

    stretched = pyrb.time_stretch(audio, sr, ratio)

    return stretched


def crossfade(audio1, audio2, fade_duration_sec, sr):
    """
    Crée un crossfade entre deux samples audio

    Args:
        audio1 (np.array): Premier signal audio
        audio2 (np.array): Deuxième signal audio
        fade_duration_sec (float): Durée du crossfade en secondes
        sr (int): Taux d'échantillonnage

    Returns:
        np.array: Audio avec crossfade
    """
    # Convertir la durée en échantillons
    fade_samples = int(fade_duration_sec * sr)

    # S'assurer que les deux audio sont assez longs
    if len(audio1) < fade_samples or len(audio2) < fade_samples:
        fade_samples = min(len(audio1), len(audio2))

    # Créer les courbes de fondu
    fade_out = np.linspace(1.0, 0.0, fade_samples)
    fade_in = np.linspace(0.0, 1.0, fade_samples)

    # Appliquer le fondu
    audio1_end = audio1[-fade_samples:] * fade_out
    audio2_start = audio2[:fade_samples] * fade_in

    # Mixer les parties de fondu
    crossfade_part = audio1_end + audio2_start

    # Combiner les parties
    result = np.concatenate(
        [audio1[:-fade_samples], crossfade_part, audio2[fade_samples:]]
    )

    return result


def apply_filter(audio, sr, filter_type, cutoff_freq=None, resonance=1.0):
    """
    Applique un filtre audio (lowpass, highpass, etc.)

    Args:
        audio (np.array): Signal audio
        sr (int): Taux d'échantillonnage
        filter_type (str): Type de filtre ('lowpass', 'highpass', 'bandpass')
        cutoff_freq (float): Fréquence de coupure en Hz
        resonance (float): Résonance du filtre (Q)

    Returns:
        np.array: Audio filtré
    """
    from pedalboard import Pedalboard, LowpassFilter, HighpassFilter, BandpassFilter

    # Définir la fréquence de coupure par défaut si non spécifiée
    if cutoff_freq is None:
        if filter_type == "lowpass":
            cutoff_freq = 1000
        elif filter_type == "highpass":
            cutoff_freq = 200
        else:
            cutoff_freq = 500

    # Créer le filtre approprié
    if filter_type == "lowpass":
        filter_effect = LowpassFilter(
            cutoff_frequency_hz=cutoff_freq, resonance=resonance
        )
    elif filter_type == "highpass":
        filter_effect = HighpassFilter(
            cutoff_frequency_hz=cutoff_freq, resonance=resonance
        )
    elif filter_type == "bandpass":
        filter_effect = BandpassFilter(
            cutoff_frequency_hz=cutoff_freq, resonance=resonance
        )
    else:
        return audio  # Type de filtre inconnu

    # Créer le Pedalboard avec le filtre
    board = Pedalboard([filter_effect])

    # Appliquer le filtre
    filtered_audio = board(audio, sr)

    return filtered_audio
