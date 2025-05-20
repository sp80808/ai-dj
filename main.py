import time
import argparse
import pygame
import os
import shutil
from fastapi import FastAPI, Request, Depends
from functools import partial
import uvicorn
from core.dj_system import DJSystem
from core.live_session import LiveSession


def get_dj_system(request: Request):
    return request.app.dj_system


def create_api_app(args):
    app = FastAPI(
        title="DJ-IA API", description="API pour le plugin VST DJ-IA", version="1.0.0"
    )

    # Créer une instance DJSystem
    dj_system = DJSystem(args)
    app.dj_system = dj_system

    # Importer et inclure les routes
    from server.api.routes import router

    # Injecter dj_system dans le router
    app.include_router(
        router, prefix="/api/v1", dependencies=[Depends(partial(get_dj_system))]
    )

    return app


def cleanup_output_directory(directory_path, max_age_minutes=60):
    """Nettoie les fichiers temporaires du répertoire de sortie."""
    if not os.path.exists(directory_path):
        os.makedirs(directory_path, exist_ok=True)
        print(f"Répertoire créé: {directory_path}")
        return

    print(f"Nettoyage du répertoire de sortie: {directory_path}")
    try:
        now = time.time()
        count = 0
        for filename in os.listdir(directory_path):
            file_path = os.path.join(directory_path, filename)
            if os.path.isfile(file_path):
                # Vérifier l'âge du fichier
                file_age_minutes = (now - os.path.getmtime(file_path)) / 60.0
                if file_age_minutes > max_age_minutes:
                    try:
                        os.remove(file_path)
                        count += 1
                    except (PermissionError, OSError) as e:
                        print(f"Erreur lors de la suppression de {file_path}: {e}")

        if count > 0:
            print(f"Nettoyage: {count} fichiers temporaires supprimés du répertoire.")
        else:
            print("Aucun fichier à nettoyer.")
    except Exception as e:
        print(f"Erreur lors du nettoyage du répertoire: {e}")


def main():
    parser = argparse.ArgumentParser(description="DJ-IA System avec Layer Manager")
    parser.add_argument(
        "--model-path",
        type=str,
        default="google/gemma-2b-it",
        help="Chemin ou nom du modèle LLM",
    )
    parser.add_argument(
        "--profile", type=str, default="techno_minimal", help="Profil DJ à utiliser"
    )
    parser.add_argument(
        "--output-dir",
        type=str,
        default="./output",
        help="Répertoire de sortie principal pour la session",
    )
    parser.add_argument(
        "--audio-model",
        type=str,
        default="musicgen-medium",
        choices=[
            "musicgen-small",
            "musicgen-medium",
            "musicgen-large",
            "stable-audio-open",
            "stable-audio-pro",
        ],
        help="Modèle audio à utiliser (MusicGen ou Stable Audio)",
    )
    parser.add_argument(
        "--generation-duration",
        type=float,
        default=8.0,
        help="Durée de génération par défaut (en secondes)",
    )
    parser.add_argument(
        "--clean",
        action="store_true",
        help="Nettoyer tous les fichiers du répertoire de sortie au démarrage",
    )
    parser.add_argument(
        "--mode",
        default="legacy",
        choices=["legacy", "live", "api"],
        help="Mode de fonctionnement: legacy (DJ complet), live (génération 30s) ou api (serveur FastAPI)",
    )
    parser.add_argument("--host", default="localhost", help="Hôte pour le serveur API")
    parser.add_argument(
        "--port", type=int, default=8000, help="Port pour le serveur API"
    )
    parser.add_argument("--sample-interval", type=int, default=30)
    args = parser.parse_args()

    # Nettoyer les répertoires de sortie
    layers_dir = os.path.join(args.output_dir, "layers")
    speech_dir = os.path.join(args.output_dir, "speech")
    live_samples_dir = os.path.join(args.output_dir, "live_samples")

    # Créer les répertoires s'ils n'existent pas
    os.makedirs(args.output_dir, exist_ok=True)

    if args.clean:
        # Option pour supprimer complètement le contenu des répertoires
        if os.path.exists(layers_dir):
            shutil.rmtree(layers_dir)
        if os.path.exists(speech_dir):
            shutil.rmtree(speech_dir)
        if os.path.exists(live_samples_dir):
            shutil.rmtree(live_samples_dir)
        print("Répertoires de sortie entièrement nettoyés.")
    else:
        # Nettoyer uniquement les fichiers plus anciens que 60 minutes
        cleanup_output_directory(layers_dir)
        cleanup_output_directory(speech_dir)
        cleanup_output_directory(live_samples_dir)

    # Recréer les répertoires après nettoyage complet
    os.makedirs(layers_dir, exist_ok=True)
    os.makedirs(speech_dir, exist_ok=True)

    if args.mode == "api":
        print(f"🚀 Démarrage du serveur API sur {args.host}:{args.port}")
        app = create_api_app(args)

        # Créer une instance DJSystem partagée pour l'API
        dj_system = DJSystem(args)
        # Rendre dj_system accessible aux routes
        app.state.dj_system = dj_system

        # Lancer le serveur
        uvicorn.run(app, host=args.host, port=args.port)

    elif args.mode == "live":
        live_session = LiveSession(args)
        try:
            live_session.start_session()

            # Garder le script en vie jusqu'à interruption
            print("")
            print("💡 DJ-IA en cours d'exécution. Appuyez sur Ctrl+C pour arrêter.")
            while True:
                time.sleep(1)
        except KeyboardInterrupt:
            print("\nArrêt demandé par l'utilisateur.")
        finally:
            live_session.stop_session()
    else:
        dj_system_instance = None  # Pour le finally
        try:
            dj_system_instance = DJSystem(args)
            dj_system_instance.start_session()
            print("")
            print("💡 DJ-IA en cours d'exécution. Appuyez sur Ctrl+C pour arrêter.")
            while dj_system_instance.session_running:
                if (
                    hasattr(dj_system_instance, "dj_thread")
                    and not dj_system_instance.dj_thread.is_alive()
                ):
                    print(
                        "ALERTE: Le thread principal du DJ s'est terminé de manière inattendue!"
                    )
                    dj_system_instance.session_running = (
                        False  # Forcer l'arrêt de la boucle main
                    )
                    break
                time.sleep(1)
        except KeyboardInterrupt:
            print("\nArrêt manuel demandé par l'utilisateur...")
        except Exception as e:
            print(f"Une erreur non gérée est survenue dans main(): {e}")
            import traceback

            traceback.print_exc()
        finally:
            if dj_system_instance:
                print("Nettoyage final de la session DJ...")
                dj_system_instance.stop_session()
            elif (
                pygame.mixer.get_init()
            ):  # Si DJSystem n'a pas été créé mais pygame oui
                pygame.mixer.quit()
    print("Programme DJ-IA terminé.")


if __name__ == "__main__":
    main()
