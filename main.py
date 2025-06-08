import argparse
import os
import shutil
import time
from fastapi import FastAPI, Request, Depends
import uvicorn
from dotenv import load_dotenv
from core.dj_system import DJSystem


load_dotenv()


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
    parser.add_argument(
        "--model-path",
        type=str,
        default=os.environ.get("LLM_MODEL_PATH"),
        help="Path of the LLM model",
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

    app = create_api_app()
    dj_system = DJSystem.get_instance(args)
    app.state.dj_system = dj_system
    uvicorn.run(app, host=args.host, port=args.port)
    print("✅ Server closed.")


if __name__ == "__main__":
    main()
