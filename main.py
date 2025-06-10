import argparse
from fastapi import FastAPI, Request, Depends
import uvicorn
from core.dj_system import DJSystem
from config.config import init_config


def get_dj_system(request: Request):
    if hasattr(request.app, "dj_system"):
        return request.app.dj_system
    if hasattr(request.app, "state") and hasattr(request.app.state, "dj_system"):
        return request.app.state.dj_system
    raise RuntimeError("No DJSystem instance found in FastAPI application!")


def create_api_app():
    app = FastAPI(
        title="OBSIDIAN-Neural API",
        description="API for the VST OBSIDIAN-Neural plugin",
        version="1.0.0",
    )
    from server.api.routes import router

    app.include_router(router, prefix="/api/v1", dependencies=[Depends(get_dj_system)])
    return app


def main():
    parser = argparse.ArgumentParser(
        description="OBSIDIAN-Neural System with Layer Manager"
    )
    parser.add_argument("--model-path", type=str, help="Path of the LLM model")
    parser.add_argument("--host", default="127.0.0.1", help="Host for API server")
    parser.add_argument("--port", type=int, default=8000, help="Port for API server")
    parser.add_argument("--environment", default="dev", help="Environment (dev/prod)")
    parser.add_argument(
        "--audio-model", default="stabilityai/stable-audio-open-1.0", help="Audio model"
    )
    parser.add_argument("--api-keys", help="API keys separated by commas")

    args = parser.parse_args()
    api_keys = []
    if args.api_keys:
        api_keys = [key.strip() for key in args.api_keys.split(",") if key.strip()]

    init_config(api_keys=api_keys, environment=args.environment)

    app = create_api_app()
    dj_system = DJSystem.get_instance(args)
    app.state.dj_system = dj_system

    uvicorn.run(app, host=args.host, port=args.port)
    print("✅ Server closed.")


if __name__ == "__main__":
    main()
