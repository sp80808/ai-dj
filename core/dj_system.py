import os
from core.llm_interface import DJAILL
from core.music_generator import MusicGenerator
from core.layer_manager import LayerManager
from core.stems_manager import StemsManager

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
        self.output_dir_base = args.output_dir
        self.audio_model = args.audio_model
        self.generation_duration = args.generation_duration
        # Initialiser les composants
        print("Initialisation du système DJ-IA...")
        self.stems_manager = StemsManager()
        # L'état initial du LLM doit refléter qu'il n'y a pas de layers au début
        initial_llm_state = {
            "current_tempo": 126,
            "current_key": "C minor",
            "active_layers": {},  # Dictionnaire pour stocker les layers actifs avec leurs infos
            "set_phase": "intro",  # ou "warmup"
            "time_elapsed_beats": 0,  # Pour aider le LLM à structurer
        }
        self.dj_brain = DJAILL(self.model_path, initial_llm_state)

        print("Chargement de MusicGen...")
        self.music_gen = MusicGenerator(model_name=self.audio_model)

        print("Initialisation du LayerManager...")
        self.layer_manager = LayerManager(
            output_dir=os.path.join(self.output_dir_base, "layers"),
        )
        self.layer_manager.set_master_tempo(initial_llm_state["current_tempo"])

        if not os.path.exists(self.output_dir_base):
            os.makedirs(self.output_dir_base)

        self.initialized = True
        print("✅ Système DJ-IA initialisé avec succès (Singleton)")
