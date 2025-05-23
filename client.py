import argparse
import os
import signal
import shutil
import sys
import time
from server.api.dj_ia_client import DJIAClient, DEFAULT_BPM, DEFAULT_STYLE, DEFAULT_KEY
from utils.file_utils import cleanup_output_directory
from dotenv import load_dotenv

load_dotenv()


def clean_dir():
    output_dir = os.path.join("server", "api", "generated_loops")
    temp_dir = os.path.join("server", "api", "temp_loops")
    layers_dir = os.path.join("output", "layers")
    speech_dir = os.path.join("output", "speech")
    live_samples_dir = os.path.join("output", "live_samples")
    if os.path.exists(output_dir):
        shutil.rmtree(output_dir)
    if os.path.exists(temp_dir):
        shutil.rmtree(temp_dir)
    if os.path.exists(layers_dir):
        shutil.rmtree(layers_dir)
    if os.path.exists(speech_dir):
        shutil.rmtree(speech_dir)
    if os.path.exists(live_samples_dir):
        shutil.rmtree(live_samples_dir)
    cleanup_output_directory(output_dir)
    cleanup_output_directory(temp_dir)
    cleanup_output_directory(layers_dir)
    cleanup_output_directory(speech_dir)
    cleanup_output_directory(live_samples_dir)
    os.makedirs("output", exist_ok=True)
    os.makedirs(output_dir, exist_ok=True)
    os.makedirs(temp_dir, exist_ok=True)
    os.makedirs(layers_dir, exist_ok=True)
    os.makedirs(speech_dir, exist_ok=True)
    print("")


def main():
    """Fonction principale"""
    parser = argparse.ArgumentParser(
        description="Client DJ-IA pour générer des loops musicales"
    )

    parser.add_argument(
        "--api-url", default="http://localhost:8000/api/v1", help="URL de l'API DJ-IA"
    )
    parser.add_argument(
        "--api-key", default=os.getenv("DJ_IA_API_KEY"), help="Clé d'API DJ-IA"
    )
    parser.add_argument(
        "--style", default=DEFAULT_STYLE, help="Style musical par défaut"
    )
    parser.add_argument(
        "--bpm", type=float, default=DEFAULT_BPM, help="Tempo par défaut (BPM)"
    )
    parser.add_argument("--key", default=DEFAULT_KEY, help="Tonalité par défaut")
    parser.add_argument(
        "--sample-rate", type=int, default=44100, help="Taux d'échantillonnage (Hz)"
    )
    parser.add_argument(
        "--adjust-bpm",
        action="store_true",
        help="Ajuster automatiquement le BPM des samples",
    )

    args = parser.parse_args()

    # Vérifier la clé API
    if not args.api_key:
        print(
            "❌ Erreur: Clé d'API non spécifiée. Utilisez --api-key ou définissez DJ_IA_API_KEY dans .env"
        )
        return

    clean_dir()
    # Créer et démarrer le client
    client = DJIAClient(
        api_url=args.api_url,
        api_key=args.api_key,
        style=args.style,
        bpm=args.bpm,
        key=args.key,
        sample_rate=args.sample_rate,
        adjust_bpm=args.adjust_bpm,
    )

    # Configurer le gestionnaire de signal pour l'arrêt propre
    def signal_handler(sig, frame):
        print("\n⚠️  Signal d'interruption reçu, arrêt en cours...")
        client.stop()
        sys.exit(0)

    signal.signal(signal.SIGINT, signal_handler)

    # Démarrer le client
    if client.start():
        # Attendre que le client se termine
        while client.running:
            time.sleep(0.1)

    # Arrêter proprement
    client.stop()


if __name__ == "__main__":
    main()
