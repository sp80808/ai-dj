import os
import base64
import time
import threading
import requests
from server.api.audio_loop_player import AudioLoopPlayer


DEFAULT_BPM = 126
DEFAULT_KEY = "C minor"
DEFAULT_STYLE = "techno_minimal"
DEFAULT_MEASURES = 1


class DJIAClient:
    """Client pour communiquer avec l'API DJ-IA et générer des boucles musicales"""

    def __init__(
        self,
        api_url,
        api_key,
        style=DEFAULT_STYLE,
        bpm=DEFAULT_BPM,
        key=DEFAULT_KEY,
        sample_rate=44100,
    ):
        """
        Initialise le client DJ-IA

        Args:
            api_url: URL de base de l'API
            api_key: Clé API
            style: Style musical par défaut
            bpm: Tempo par défaut (BPM)
            key: Tonalité par défaut
            sample_rate: Taux d'échantillonnage
        """
        self.api_url = api_url.rstrip("/")
        self.api_key = api_key
        self.style = style
        self.bpm = bpm
        self.key = key
        self.running = False
        self.headers = {"X-API-Key": api_key}
        self.audio_player = AudioLoopPlayer(sample_rate=sample_rate)
        self.generation_active = False
        self.generation_thread = None

        # Chemin pour sauvegarder les fichiers
        self.output_dir = os.path.join(
            os.path.dirname(os.path.abspath(__file__)), "generated_loops"
        )
        os.makedirs(self.output_dir, exist_ok=True)

    def start(self):
        """Démarre le client DJ-IA"""
        try:
            # Vérifier la connexion à l'API
            response = requests.post(f"{self.api_url}/verify_key", headers=self.headers)
            if response.status_code != 200:
                print(f"❌ Erreur de connexion à l'API: {response.status_code}")
                print(response.text)
                return False

            print(f"✅ Connexion à l'API réussie: {self.api_url}")

            # Démarrer le lecteur audio
            self.audio_player.start()

            # Générer la première boucle
            self.running = True
            self._generate_loop()

            # Boucle principale d'interaction utilisateur
            self._start_user_interaction()

            return True

        except Exception as e:
            print(f"❌ Erreur lors du démarrage du client: {e}")
            return False

    def stop(self):
        """Arrête le client DJ-IA"""
        self.running = False

        # Arrêter la génération en cours
        if self.generation_thread and self.generation_thread.is_alive():
            print("⏳ Attente de la fin de la génération en cours...")
            self.generation_thread.join(timeout=5)

        # Arrêter le lecteur audio
        self.audio_player.stop()

        print("👋 Client DJ-IA arrêté")

    def _generate_loop(self, prompt=None, stems=None, measures=DEFAULT_MEASURES):
        """
        Génère une nouvelle boucle musicale

        Args:
            prompt: Description textuelle (optionnel)
            stems: Liste des stems à extraire (optionnel)
            measures: Nombre de mesures (optionnel)
        """
        if self.generation_active:
            print("⚠️ Génération déjà en cours, veuillez patienter...")
            return

        self.generation_active = True

        # Préparer les paramètres de requête
        request_data = {
            "prompt": prompt or f"{self.style} loop",
            "style": self.style,
            "bpm": self.bpm,
            "key": self.key,
            "measures": measures,
        }

        if stems:
            request_data["preferred_stems"] = stems

        # Afficher un message de génération
        stem_info = f" (stems: {', '.join(stems)})" if stems else ""
        print(
            f"\n🎛️  Génération d'une boucle {self.style} à {self.bpm} BPM en {self.key}{stem_info}..."
        )
        print(f"💬 Prompt: \"{request_data['prompt']}\"")

        # Lancer la génération dans un thread séparé
        self.generation_thread = threading.Thread(
            target=self._generate_loop_thread, args=(request_data,)
        )
        self.generation_thread.daemon = True
        self.generation_thread.start()

    def _generate_loop_thread(self, request_data):
        """
        Thread de génération de boucle

        Args:
            request_data: Données de la requête
        """
        try:
            # Envoyer la requête à l'API
            start_time = time.time()
            response = requests.post(
                f"{self.api_url}/generate",
                headers=self.headers,
                json=request_data,
                timeout=120,  # 2 minutes max
            )

            generation_time = time.time() - start_time

            if response.status_code == 200:
                # Traiter la réponse
                result = response.json()

                # Extraire les données audio
                audio_base64 = result.get("audio_data")
                audio_bytes = base64.b64decode(audio_base64)

                # Déterminer un nom de fichier unique
                timestamp = int(time.time())
                file_path = os.path.join(self.output_dir, f"loop_{timestamp}.wav")

                # Sauvegarder le fichier audio
                with open(file_path, "wb") as f:
                    f.write(audio_bytes)

                # Charger l'audio en mémoire
                import soundfile as sf

                audio_data, sample_rate = sf.read(file_path)

                # Ajouter au lecteur
                if self.audio_player.add_loop(audio_data, sample_rate):
                    # Afficher les informations
                    print(f"✅ Boucle générée en {generation_time:.1f}s")
                    print(f"📁 Sauvegardée dans: {file_path}")
                    print(
                        f"🎹 BPM: {result.get('bpm')} | Tonalité: {result.get('key')}"
                    )

                    if result.get("stems_used"):
                        print(
                            f"🔊 Stems utilisés: {', '.join(result.get('stems_used'))}"
                        )

            else:
                print(f"❌ Erreur de génération: {response.status_code}")
                print(response.text)

        except Exception as e:
            print(f"❌ Erreur lors de la génération: {e}")

        finally:
            self.generation_active = False
            print(
                "\n💬 Entrée une description pour générer une nouvelle boucle (ou Entrée pour générer automatiquement) > ",
                end="",
                flush=True,
            )

    def _start_user_interaction(self):
        """Démarre la boucle d'interaction utilisateur"""
        print("\n" + "=" * 60)
        print("🎮 CLIENT DJ-IA 🎮")
        print("=" * 60)
        print(f"🎛️  Style: {self.style} | BPM: {self.bpm} | Tonalité: {self.key}")
        print("=" * 60)
        print("💡 Commandes:")
        print("   - Appuyer sur Entrée: Générer une nouvelle boucle")
        print("   - Saisir un texte: Générer une boucle selon cette description")
        print("   - s [nom]: Spécifier des stems (ex: s drums,bass)")
        print("   - b [bpm]: Changer le BPM (ex: b 130)")
        print("   - k [key]: Changer la tonalité (ex: k F minor)")
        print("   - y [style]: Changer le style (ex: s ambient)")
        print("   - q: Quitter l'application")
        print("=" * 60)

        # Boucle d'interaction utilisateur
        try:
            while self.running:
                # Afficher le prompt si aucune génération en cours
                if not self.generation_active:
                    print(
                        "\n💬 Entrée une description pour générer une nouvelle boucle (ou Entrée pour générer automatiquement) > ",
                        end="",
                        flush=True,
                    )

                user_input = input().strip()

                if not self.running:
                    break

                if user_input.lower() in ["q", "quit", "exit"]:
                    print("👋 Fermeture du client DJ-IA...")
                    self.running = False

                elif user_input.startswith("s "):
                    # Commande pour définir les stems
                    try:
                        stems_input = user_input[2:].strip()
                        stems = [s.strip() for s in stems_input.split(",")]
                        if stems:
                            print(f"🎚️ Stems définis: {', '.join(stems)}")
                            self._generate_loop(stems=stems)
                        else:
                            print("⚠️ Format incorrect. Exemple: s drums,bass")
                    except Exception as e:
                        print(f"❌ Erreur: {e}")

                elif user_input.startswith("b "):
                    # Commande pour changer le BPM
                    try:
                        new_bpm = float(user_input[2:].strip())
                        if 40 <= new_bpm <= 240:
                            self.bpm = new_bpm
                            print(f"🎚️ BPM changé à {self.bpm}")
                        else:
                            print("⚠️ BPM hors limites (40-240)")
                    except ValueError:
                        print("⚠️ Format incorrect. Exemple: b 130")

                elif user_input.startswith("k "):
                    # Commande pour changer la tonalité
                    new_key = user_input[2:].strip()
                    if new_key:
                        self.key = new_key
                        print(f"🎚️ Tonalité changée à {self.key}")
                    else:
                        print("⚠️ Format incorrect. Exemple: k A minor")

                elif user_input.startswith("y "):
                    # Commande pour changer le style
                    new_style = user_input[2:].strip()
                    if new_style:
                        self.style = new_style
                        print(f"🎚️ Style changé à {self.style}")
                    else:
                        print("⚠️ Format incorrect. Exemple: y ambient")

                elif not user_input:
                    # Génération automatique (appui sur Entrée)
                    self._generate_loop()

                else:
                    # Génération avec prompt personnalisé
                    self._generate_loop(prompt=user_input)

        except KeyboardInterrupt:
            print("\n👋 Arrêt demandé par l'utilisateur")
            self.running = False

        except Exception as e:
            print(f"\n❌ Erreur dans la boucle d'interaction: {e}")
            self.running = False
