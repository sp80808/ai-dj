import hashlib
import json
from pathlib import Path


def generate_code_integrity_hash():
    code_files = []

    for py_file in Path("/app").rglob("*.py"):
        if py_file.is_file():
            try:
                with open(py_file, "rb") as f:
                    content = f.read()
                    file_hash = hashlib.sha256(content).hexdigest()
                    code_files.append(f"{py_file.name}:{file_hash}")
            except:
                continue

    combined = "|".join(sorted(code_files))
    return hashlib.sha256(combined.encode()).hexdigest()


def main():
    integrity_hash = generate_code_integrity_hash()

    with open("/app/.worker_identity", "w") as f:
        json.dump(
            {
                "code_integrity_hash": integrity_hash,
                "build_timestamp": str(time.time()),
            },
            f,
        )

    print(f"🔐 Worker integrity hash: {integrity_hash[:16]}...")


if __name__ == "__main__":
    import time

    main()
