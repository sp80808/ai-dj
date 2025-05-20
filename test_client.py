# test_client.py
import requests
import os
from dotenv import load_dotenv
import simpleaudio as sa
import wave
import time

# Charger la clé API depuis .env
load_dotenv()
API_KEY = os.getenv("DJ_IA_API_KEY")

# Configuration
BASE_URL = "http://localhost:8000/api/v1"
HEADERS = {"X-API-Key": API_KEY}


def play_wav_file(file_path):
    """Joue un fichier WAV et attend la fin de la lecture"""
    try:
        # Ouvrir et lire les infos du WAV
        wave_file = wave.open(file_path, "rb")
        duration = wave_file.getnframes() / wave_file.getframerate()
        wave_file.close()

        # Jouer le fichier
        wave_obj = sa.WaveObject.from_wave_file(file_path)
        play_obj = wave_obj.play()

        print(f"\n▶️  Lecture du fichier ({duration:.2f}s)...")
        play_obj.wait_done()  # Attendre la fin de la lecture
        print("⏹️  Lecture terminée")

    except Exception as e:
        print(f"❌ Erreur lors de la lecture: {str(e)}")


def test_generate_loop():
    # Requête de génération
    data = {
        "prompt": "dark techno loop with powerful kick and acid elements",
        "style": "techno",
        "bpm": 126,
        "key": "C minor",
        "measures": 4,
        "preferred_stems": ["drums", "bass"],
    }

    try:
        # Vérifier que le serveur est en ligne
        health = requests.get(f"{BASE_URL}/health", headers=HEADERS)
        print("\n🔍 Status serveur:", health.json())

        # Générer la loop
        print("\n🎵 Génération de la loop...")
        response = requests.post(f"{BASE_URL}/generate", headers=HEADERS, json=data)

        if response.status_code == 200:
            result = response.json()
            print("\n✅ Loop générée avec succès!")
            print(f"📁 Fichier: {result['file_path']}")
            print(f"⏱️  Durée: {result['duration']:.2f}s")
            print(f"🎼 BPM: {result['bpm']}")
            print(f"🎹 Tonalité: {result['key']}")
            if result.get("stems_used"):
                print(f"🎛️  Stems utilisés: {', '.join(result['stems_used'])}")
            if result.get("llm_reasoning"):
                print(f"\n💭 Raisonnement LLM: {result['llm_reasoning']}")

            # Jouer le fichier généré
            if os.path.exists(result["file_path"]):
                print("\n🔊 Lecture de la loop générée...")
                play_wav_file(result["file_path"])
            else:
                print(f"\n❌ Fichier non trouvé: {result['file_path']}")

        else:
            print(f"\n❌ Erreur: {response.status_code}")
            print(response.text)

    except Exception as e:
        print(f"\n❌ Erreur: {str(e)}")


if __name__ == "__main__":
    test_generate_loop()
