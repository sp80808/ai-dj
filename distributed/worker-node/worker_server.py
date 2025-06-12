import argparse
import asyncio
import hashlib
import os
import sys
import time
import uuid
from datetime import datetime
from pathlib import Path
import aiohttp
import psutil
import torch
from fastapi import FastAPI, HTTPException, BackgroundTasks
from fastapi.responses import Response
import uvicorn
from pydantic import BaseModel
from typing import Dict, Any, Optional, List

sys.path.append(str(Path(__file__).parent.parent))

from core.dj_system import DJSystem
from distributed.shared.memory_providers import DistributedMemoryProvider


class WorkerGenerateRequest(BaseModel):
    prompt: str
    bpm: float
    key: Optional[str] = None
    measures: Optional[int] = 4
    preferred_stems: Optional[List[str]] = None
    generation_duration: Optional[float] = 6.0
    sample_rate: Optional[float] = 48000.0
    user_memory: Optional[Dict[str, Any]] = None
    request_id: str
    anonymized_user_id: str


class WorkerStatus(BaseModel):
    worker_id: str
    status: str
    gpu_memory_used: float
    gpu_memory_total: float
    cpu_usage: float
    memory_usage: float
    queue_size: int
    last_heartbeat: datetime


class WorkerServer:
    def __init__(self, central_server_url: str, worker_port: int = 8001):
        self.worker_id = self.generate_worker_id()
        self.central_server_url = central_server_url
        self.worker_port = worker_port
        self.dj_system = None
        self.status = "initializing"
        self.session_token = None
        self.request_queue = asyncio.Queue()
        self.processing_request = False

        self.app = FastAPI(
            title=f"OBSIDIAN Worker {self.worker_id[:8]}",
            description="Distributed GPU Worker for OBSIDIAN Neural Sound Engine",
            version="1.0.0",
        )
        self.setup_routes()

        print(f"🚀 Worker {self.worker_id[:8]} initializing...")

    def generate_worker_id(self) -> str:
        import platform
        import socket

        hostname = socket.gethostname()
        mac_address = ":".join(
            ["{:02x}".format((uuid.getnode() >> i) & 0xFF) for i in range(0, 48, 8)][
                ::-1
            ]
        )
        platform_info = platform.platform()
        timestamp = str(time.time())
        random_component = str(uuid.uuid4())

        unique_string = (
            f"{hostname}-{mac_address}-{platform_info}-{timestamp}-{random_component}"
        )
        return hashlib.sha256(unique_string.encode()).hexdigest()

    def get_docker_image_hash(self) -> str:
        try:
            return os.environ.get("DOCKER_IMAGE_HASH", "development-mode")
        except:
            return "unknown"

    def get_gpu_info(self) -> Dict[str, Any]:
        """Get GPU information"""
        if not torch.cuda.is_available():
            return {"error": "CUDA not available"}

        try:
            gpu_info = {}
            gpu_count = torch.cuda.device_count()
            for i in range(gpu_count):
                props = torch.cuda.get_device_properties(i)
                memory_allocated = torch.cuda.memory_allocated(i)
                memory_reserved = torch.cuda.memory_reserved(i)
                memory_total = props.total_memory

                gpu_info[f"gpu_{i}"] = {
                    "name": props.name,
                    "memory_total": memory_total,
                    "memory_allocated": memory_allocated,
                    "memory_reserved": memory_reserved,
                    "memory_free": memory_total - memory_reserved,
                    "compute_capability": f"{props.major}.{props.minor}",
                }
            return gpu_info
        except Exception as e:
            return {"error": str(e)}

    def get_system_status(self) -> WorkerStatus:
        gpu_info = self.get_gpu_info()

        gpu_memory_used = 0
        gpu_memory_total = 0
        if "gpu_0" in gpu_info and "error" not in gpu_info:
            gpu_memory_used = gpu_info["gpu_0"]["memory_allocated"]
            gpu_memory_total = gpu_info["gpu_0"]["memory_total"]

        return WorkerStatus(
            worker_id=self.worker_id,
            status=self.status,
            gpu_memory_used=gpu_memory_used / (1024**3) if gpu_memory_used else 0,  # GB
            gpu_memory_total=(gpu_memory_total / (1024**3) if gpu_memory_total else 0),
            cpu_usage=psutil.cpu_percent(),
            memory_usage=psutil.virtual_memory().percent,
            queue_size=self.request_queue.qsize(),
            last_heartbeat=datetime.now(),
        )

    def setup_routes(self):
        @self.app.get("/health")
        async def health_check():
            return {"status": "healthy", "worker_id": self.worker_id[:8]}

        @self.app.get("/status")
        async def get_status():
            return self.get_system_status()

        @self.app.post("/generate")
        async def generate_audio(
            request: WorkerGenerateRequest, background_tasks: BackgroundTasks
        ):
            if self.status != "idle":
                raise HTTPException(status_code=503, detail="Worker is busy")

            self.status = "busy"
            try:
                result = await self.process_generation_request(request)
                return result
            except Exception as e:
                print(f"❌ Generation error: {str(e)}")
                raise HTTPException(status_code=500, detail=str(e))
            finally:
                self.status = "idle"

    async def process_generation_request(
        self, request: WorkerGenerateRequest
    ) -> Response:
        print(
            f"🎵 Processing request {request.request_id} for user {request.anonymized_user_id[:8]}..."
        )

        try:
            if not self.dj_system:
                self.init_dj_system()

            memory_provider = DistributedMemoryProvider(request.user_memory or {})

            self.dj_system.dj_brain.memory_provider = memory_provider
            self.dj_system.dj_brain.session_state = {
                "current_tempo": request.bpm,
                "current_key": request.key or "C minor",
                "user_prompt": request.prompt,
                "request_id": request.request_id,
                "user_id": request.anonymized_user_id,
                "last_action_time": time.time(),
            }

            self.dj_system.dj_brain.init_model()
            llm_decision = self.dj_system.dj_brain.get_next_decision()
            self.dj_system.dj_brain.destroy_model()

            self.dj_system.music_gen.init_model()
            sample_details = llm_decision.get("parameters", {}).get(
                "sample_details", {}
            )
            musicgen_prompt = sample_details.get("musicgen_prompt", request.prompt)

            audio, _ = self.dj_system.music_gen.generate_sample(
                musicgen_prompt=musicgen_prompt,
                tempo=request.bpm,
                generation_duration=request.generation_duration or 6,
                sample_rate=int(request.sample_rate),
            )
            self.dj_system.music_gen.destroy_model()

            temp_path = os.path.join(
                self.dj_system.output_dir_base, f"worker_temp_{request.request_id}.wav"
            )
            self.dj_system.music_gen.save_sample(
                audio, temp_path, sample_rate=int(request.sample_rate)
            )

            self.dj_system.layer_manager.master_tempo = request.bpm
            processed_path = self.dj_system.layer_manager._prepare_sample_for_loop(
                original_audio_path=temp_path,
                layer_id=f"worker_loop_{request.request_id}",
                sample_rate=int(request.sample_rate),
            )

            used_stems = None
            if request.preferred_stems and processed_path:
                spectral_profile, separated_path = (
                    self.dj_system.stems_manager._analyze_sample_with_demucs(
                        processed_path,
                        os.path.join(self.dj_system.output_dir_base, "temp"),
                    )
                )

                if spectral_profile and separated_path:
                    final_path, used_stems = (
                        self.dj_system.stems_manager._extract_multiple_stems(
                            spectral_profile,
                            separated_path,
                            f"worker_loop_{request.request_id}",
                            request.preferred_stems,
                            sample_rate=int(request.sample_rate),
                        )
                    )
                    if final_path:
                        if os.path.exists(processed_path):
                            os.remove(processed_path)
                        processed_path = final_path

            if not processed_path or not os.path.exists(processed_path):
                raise Exception("Audio generation failed - no output file")

            with open(processed_path, "rb") as f:
                wav_data = f.read()

            for cleanup_path in [temp_path, processed_path]:
                if cleanup_path and os.path.exists(cleanup_path):
                    os.remove(cleanup_path)

            print(f"✅ Request {request.request_id} completed successfully")

            return Response(
                content=wav_data,
                media_type="audio/wav",
            )

        except Exception as e:
            print(f"❌ Error processing request {request.request_id}: {str(e)}")
            raise

    def init_dj_system(self):
        print("⚡ Initializing DJSystem for worker...")

        args = argparse.Namespace(
            model_path=os.environ.get("LLM_MODEL_PATH"),
            audio_model=os.environ.get(
                "AUDIO_MODEL", "stabilityai/stable-audio-open-1.0"
            ),
        )

        self.dj_system = DJSystem.get_instance(args)
        print("✅ DJSystem initialized for worker")

    async def register_with_central_server(self):
        registration_data = {
            "worker_id": self.worker_id,
            "docker_image_hash": self.get_docker_image_hash(),
            "gpu_info": self.get_gpu_info(),
            "capabilities": ["stable-audio-open", "llm", "stems-extraction"],
            "heartbeat_interval": 30,
            "worker_port": self.worker_port,
            "status": self.get_system_status().dict(),
        }

        try:
            async with aiohttp.ClientSession() as session:
                async with session.post(
                    f"{self.central_server_url}/api/workers/register",
                    json=registration_data,
                    timeout=10,
                ) as response:
                    if response.status == 200:
                        result = await response.json()
                        self.session_token = result.get("session_token")
                        self.status = "idle"
                        print(f"✅ Successfully registered with central server")
                        return True
                    else:
                        print(f"❌ Registration failed: {response.status}")
                        return False
        except Exception as e:
            print(f"❌ Registration error: {str(e)}")
            return False

    async def heartbeat_loop(self):
        while True:
            if self.session_token:
                try:
                    status_data = self.get_system_status().model_dump()
                    async with aiohttp.ClientSession() as session:
                        async with session.post(
                            f"{self.central_server_url}/api/workers/heartbeat",
                            json=status_data,
                            headers={"Authorization": f"Bearer {self.session_token}"},
                            timeout=5,
                        ) as response:
                            if response.status != 200:
                                print(f"⚠️ Heartbeat failed: {response.status}")
                except Exception as e:
                    print(f"⚠️ Heartbeat error: {str(e)}")

            await asyncio.sleep(30)


async def main():
    parser = argparse.ArgumentParser(description="OBSIDIAN Distributed Worker Node")
    parser.add_argument("--host", default="0.0.0.0", help="Host to bind to")
    parser.add_argument("--port", type=int, default=8001, help="Port to bind to")
    parser.add_argument(
        "--central-server",
        default=os.environ.get("CENTRAL_SERVER_URL", "http://central-server:8080"),
        help="Central server URL",
    )

    args = parser.parse_args()

    worker = WorkerServer(args.central_server, args.port)

    print("🔗 Attempting to register with central server...")
    registration_success = await worker.register_with_central_server()

    if not registration_success:
        print(
            "❌ Failed to register with central server. Starting in standalone mode..."
        )
        worker.status = "standalone"

    heartbeat_task = asyncio.create_task(worker.heartbeat_loop())

    config = uvicorn.Config(
        worker.app, host=args.host, port=args.port, log_level="info", access_log=False
    )
    server = uvicorn.Server(config)

    print(f"🚀 Starting worker server on {args.host}:{args.port}")
    print(f"🆔 Worker ID: {worker.worker_id[:8]}")
    print(f"🔗 Central Server: {args.central_server}")

    try:
        await server.serve()
    except KeyboardInterrupt:
        print("🛑 Shutting down worker...")
    finally:
        heartbeat_task.cancel()
        print("✅ Worker shutdown complete")


if __name__ == "__main__":
    asyncio.run(main())
