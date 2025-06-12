FROM nvidia/cuda:12.1-devel-ubuntu22.04

ENV DEBIAN_FRONTEND=noninteractive
ENV PYTHONUNBUFFERED=1

RUN apt-get update && apt-get install -y \
    python3.10 \
    python3.10-venv \
    python3-pip \
    git \
    curl \
    ca-certificates \
    gnupg \
    ffmpeg \
    build-essential \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

RUN useradd -m -u 1000 obsidian && \
    chown -R obsidian:obsidian /app

COPY --chown=obsidian:obsidian requirements.txt .

RUN pip3 install --no-cache-dir --extra-index-url https://download.pytorch.org/whl/cu121 -r requirements.txt

COPY --chown=obsidian:obsidian . .

RUN mkdir -p /app/output /app/temp && \
    chown -R obsidian:obsidian /app

RUN python3 generate_worker_identity.py

HEALTHCHECK --interval=30s --timeout=10s --start-period=60s --retries=3 \
    CMD curl -f http://localhost:8001/health || exit 1

USER obsidian

EXPOSE 8001

ENV WORKER_MODE=distributed
ENV NVIDIA_VISIBLE_DEVICES=all
ENV NVIDIA_DRIVER_CAPABILITIES=compute,utility

CMD ["python3", "worker_server.py", "--host", "0.0.0.0", "--port", "8001"]