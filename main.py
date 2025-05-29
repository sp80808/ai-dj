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
    if hasattr(request.app, "dj_system"):
        return request.app.dj_system
    if hasattr(request.app, "state") and hasattr(request.app.state, "dj_system"):
        return request.app.state.dj_system
    raise RuntimeError("No DJSystem instance found in FastAPI application!")


def create_api_app():
    app = FastAPI(
        title="DJ-IA API", description="API for the VST DJ-IA plugin", version="1.0.0"
    )
    from server.api.routes import router

    app.include_router(router, prefix="/api/v1", dependencies=[Depends(get_dj_system)])
    return app


def main():
    parser = argparse.ArgumentParser(description="DJ-IA System with Layer Manager")
    parser.add_argument(
        "--model-path",
        type=str,
        default=os.environ.get("LLM_MODEL_PATH"),
        help="Path of the LLM model",
    )
    parser.add_argument(
        "--output-dir",
        type=str,
        default="./output",
        help="Main output directory for the session",
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
        help="Audio model to use (MusicGen or Stable Audio)",
    )
    parser.add_argument(
        "--generation-duration",
        type=float,
        default=6.0,
        help="Default generation time (in seconds)",
    )
    parser.add_argument(
        "--clean",
        action="store_true",
        help="Clean all files in the output directory on startup",
    )
    parser.add_argument("--host", default="localhost", help="Host for API server")
    parser.add_argument("--port", type=int, default=8000, help="Port for API server")
    args = parser.parse_args()
    layers_dir = os.path.join(args.output_dir, "layers")
    os.makedirs(args.output_dir, exist_ok=True)

    if args.clean:
        if os.path.exists(layers_dir):
            shutil.rmtree(layers_dir)
        print("Output directories completely cleaned.")
    else:
        cleanup_output_directory(layers_dir)
    os.makedirs(layers_dir, exist_ok=True)

    app = create_api_app()
    dj_system = DJSystem.get_instance(args)
    app.state.dj_system = dj_system
    uvicorn.run(app, host=args.host, port=args.port)
    print("Server closed.")


if __name__ == "__main__":
    main()
