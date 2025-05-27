import argparse
import os
import shutil
from fastapi import FastAPI, Request, Depends
import uvicorn
from dotenv import load_dotenv
from core.dj_system import DJSystem
from utils.file_utils import cleanup_output_directory

load_dotenv()


def get_dj_system(request: Request):
    """Récupère l'instance DJ System à partir de la requête"""
    # Vérifier d'abord app.dj_system (méthode principale)
    if hasattr(request.app, "dj_system"):
        return request.app.dj_system

    # Vérifier ensuite app.state.dj_system (méthode alternative)
    if hasattr(request.app, "state") and hasattr(request.app.state, "dj_system"):
        return request.app.state.dj_system

    # Si aucune instance n'est trouvée, c'est une erreur grave
    raise RuntimeError("Aucune instance DJSystem trouvée dans l'application FastAPI!")


def create_api_app():
    app = FastAPI(
        title="DJ-IA API", description="API pour le plugin VST DJ-IA", version="1.0.0"
    )
    from server.api.routes import router

    app.include_router(router, prefix="/api/v1", dependencies=[Depends(get_dj_system)])
    return app


def main():
    parser = argparse.ArgumentParser(description="DJ-IA System avec Layer Manager")
    parser.add_argument(
        "--model-path",
        type=str,
        default=os.environ.get("LLM_MODEL_PATH"),
        help="Chemin ou nom du modèle LLM",
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
        default=os.environ.get("AUDIO_MODEL"),
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
        default=6.0,
        help="Durée de génération par défaut (en secondes)",
    )
    parser.add_argument(
        "--clean",
        action="store_true",
        help="Nettoyer tous les fichiers du répertoire de sortie au démarrage",
    )
    parser.add_argument("--host", default="localhost", help="Hôte pour le serveur API")
    parser.add_argument(
        "--port", type=int, default=8000, help="Port pour le serveur API"
    )
    args = parser.parse_args()

    # Nettoyer les répertoires de sortie
    layers_dir = os.path.join(args.output_dir, "layers")

    # Créer les répertoires s'ils n'existent pas
    os.makedirs(args.output_dir, exist_ok=True)

    if args.clean:
        # Option pour supprimer complètement le contenu des répertoires
        if os.path.exists(layers_dir):
            shutil.rmtree(layers_dir)
        print("Répertoires de sortie entièrement nettoyés.")
    else:
        # Nettoyer uniquement les fichiers plus anciens que 60 minutes
        cleanup_output_directory(layers_dir)

    # Recréer les répertoires après nettoyage complet
    os.makedirs(layers_dir, exist_ok=True)

    app = create_api_app()

    # Créer une instance DJSystem partagée pour l'API
    dj_system = DJSystem.get_instance(args)
    # Rendre dj_system accessible aux routes
    app.state.dj_system = dj_system

    # Lancer le serveur
    uvicorn.run(app, host=args.host, port=args.port)

    print("Serveur fermé.")


if __name__ == "__main__":
    main()
