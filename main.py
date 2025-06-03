import argparse
import os
import shutil
import time
from fastapi import FastAPI, Request, Depends
import uvicorn
from dotenv import load_dotenv
from core.dj_system import DJSystem


load_dotenv()


def cleanup_output_directory(directory_path, max_age_minutes=60):
    if not os.path.exists(directory_path):
        os.makedirs(directory_path, exist_ok=True)
        print(f"✅ Directory created: {directory_path}")
        return

    print(f"Cleaning output directory: {directory_path}")
    try:
        now = time.time()
        count = 0
        for filename in os.listdir(directory_path):
            file_path = os.path.join(directory_path, filename)
            if os.path.isfile(file_path):
                file_age_minutes = (now - os.path.getmtime(file_path)) / 60.0
                if file_age_minutes > max_age_minutes:
                    try:
                        os.remove(file_path)
                        count += 1
                    except (PermissionError, OSError) as e:
                        print(f"Error deleting {file_path}: {e}")

        if count > 0:
            print(f"Cleaning: {count} temporary files deleted from directory.")
        else:
            print("No files to clean.")
    except Exception as e:
        print(f"Error cleaning directory: {e}")


def get_dj_system(request: Request):
    if hasattr(request.app, "dj_system"):
        return request.app.dj_system
    if hasattr(request.app, "state") and hasattr(request.app.state, "dj_system"):
        return request.app.state.dj_system
    raise RuntimeError("No DJSystem instance found in FastAPI application!")


def create_api_app():
    app = FastAPI(
        title="OBSIDIAN API",
        description="API for the VST OBSIDIAN plugin",
        version="1.0.0",
    )
    from server.api.routes import router

    app.include_router(router, prefix="/api/v1", dependencies=[Depends(get_dj_system)])
    return app


def main():
    parser = argparse.ArgumentParser(description="OBSIDIAN System with Layer Manager")
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
        "--clean",
        action="store_true",
        help="Clean all files in the output directory on startup",
    )
    parser.add_argument(
        "--host", default=os.environ.get("HOST"), help="Host for API server"
    )
    parser.add_argument(
        "--port",
        type=int,
        default=int(os.environ.get("PORT")),
        help="Port for API server",
    )
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
