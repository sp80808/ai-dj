import argparse
import os
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

    from fastapi.exceptions import RequestValidationError
    from fastapi.responses import JSONResponse

    @app.exception_handler(RequestValidationError)
    async def validation_exception_handler(
        request: Request, exc: RequestValidationError
    ):
        print(f"❌ Validation Error on {request.method} {request.url}")
        print(f"❌ Error details: {exc.errors()}")

        try:
            body = await request.body()
            print(f"❌ Raw request body: {body.decode('utf-8')}")
        except:
            print("❌ Could not read request body")

        return JSONResponse(
            status_code=422,
            content={
                "error": {
                    "code": "VALIDATION_ERROR",
                    "message": f"Request validation failed: {exc.errors()}",
                }
            },
        )

    from server.api.routes import router

    app.include_router(router, prefix="/api/v1", dependencies=[Depends(get_dj_system)])

    return app


def load_encrypted_api_keys():
    try:
        from core.secure_storage import SecureStorage
        from pathlib import Path
        import sqlite3

        db_path = Path.home() / ".obsidian_neural" / "config.db"
        if not db_path.exists():
            return []

        secure_storage = SecureStorage(db_path)

        conn = sqlite3.connect(db_path)
        cursor = conn.cursor()

        cursor.execute("SELECT key_value_encrypted FROM api_keys ORDER BY created_at")
        rows = cursor.fetchall()

        api_keys = []
        for row in rows:
            decrypted_key = secure_storage.decrypt(row[0])
            if decrypted_key:
                api_keys.append(decrypted_key)

        conn.close()
        return api_keys

    except Exception as e:
        print(f"Warning: Could not load encrypted API keys: {e}")
        return []


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
    parser.add_argument(
        "--use-stored-keys",
        action="store_true",
        help="Load API keys from encrypted database",
    )
    parser.add_argument(
        "--is-test",
        action="store_true",
        help="Bypass models generations for faster testing",
    )
    parser.add_argument(
        "--bypass-llm",
        action="store_true",
        help="Bypass LLM for direct stable audio generation",
    )

    args = parser.parse_args()

    model_path = args.model_path or os.environ.get("LLM_MODEL_PATH")
    host = args.host or os.environ.get("HOST", "127.0.0.1")
    port = args.port or int(os.environ.get("PORT", 8000))
    environment = args.environment or os.environ.get("ENVIRONMENT", "dev")
    audio_model = args.audio_model or os.environ.get(
        "AUDIO_MODEL", "stabilityai/stable-audio-open-1.0"
    )

    api_keys = []

    if environment == "prod":
        api_keys = load_encrypted_api_keys()
        print(f"Production mode: loaded {len(api_keys)} API keys from database")
    elif args.use_stored_keys:
        api_keys = load_encrypted_api_keys()
        print(f"Development mode: loaded {len(api_keys)} API keys from database")
    else:
        print("Development mode: no API keys loaded (dev bypass active)")

    print(f"🎵 Starting OBSIDIAN-Neural Server")
    print(f"   Host: {host}:{port}")
    print(f"   Environment: {environment}")
    print(f"   Model: {model_path}")
    print(f"   Audio Model: {audio_model}")
    print(f"   Use LLM: {not args.bypass_llm}")
    print(
        f"   API Authentication: {len(api_keys)} keys"
        if api_keys
        else "   API Authentication: Development bypass"
    )

    from config.config import init_config_from_args

    config_args = argparse.Namespace(
        api_keys=",".join(api_keys) if api_keys else "",
        environment=environment,
        audio_model=audio_model,
        use_stored_keys=args.use_stored_keys,
        is_test=args.is_test,
    )

    init_config_from_args(config_args)

    app = create_api_app()
    dj_args = argparse.Namespace(
        model_path=model_path,
        host=host,
        port=port,
        environment=environment,
        audio_model=audio_model,
    )

    dj_system = DJSystem.get_instance(dj_args)
    app.state.dj_system = dj_system

    uvicorn.run(app, host=host, port=port)
    print("✅ Server closed.")


if __name__ == "__main__":
    main()
