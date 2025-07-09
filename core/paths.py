import os
import platform
from pathlib import Path


def get_config_db_path() -> Path:
    system = platform.system()

    if system == "Windows":
        config_dir = Path.home() / ".obsidian_neural"
    elif system == "Darwin":
        config_dir = Path.home() / "Library" / "Application Support" / "OBSIDIAN-Neural"
    else:
        xdg_config_home = os.environ.get("XDG_CONFIG_HOME", Path.home() / ".config")
        config_dir = Path(xdg_config_home) / "obsidian-neural"

    config_dir.mkdir(parents=True, exist_ok=True)
    return config_dir
