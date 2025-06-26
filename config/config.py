# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# Copyright (C) 2025 Anthony Charretier

import asyncio
from typing import List

API_KEYS: List[str] = []
ENVIRONMENT: str = "dev"
AUDIO_MODEL: str = "stabilityai/stable-audio-open-1.0"
IS_TEST: bool = False

API_KEY_HEADER = "X-API-Key"

lock = asyncio.Lock()


def init_config_from_args(args):
    global API_KEYS, ENVIRONMENT, AUDIO_MODEL, IS_TEST

    ENVIRONMENT = args.environment or "dev"
    AUDIO_MODEL = args.audio_model or "stabilityai/stable-audio-open-1.0"
    IS_TEST = args.is_test

    if args.use_stored_keys:
        API_KEYS = load_api_keys_from_db()
        print(
            f"Config initialized: {len(API_KEYS)} API keys loaded from database, env={ENVIRONMENT}"
        )
    else:
        API_KEYS = []
        print(
            f"Config initialized: Development bypass (no API keys), env={ENVIRONMENT}"
        )


def load_api_keys_from_db():
    try:
        from core.secure_storage import SecureStorage
        import sqlite3
        from pathlib import Path

        db_path = Path.home() / ".obsidian_neural" / "config.db"
        if not db_path.exists():
            print("Warning: No database found, no API keys loaded")
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
        print(f"Successfully loaded {len(api_keys)} API keys from encrypted database")
        return api_keys

    except Exception as e:
        print(f"Error loading API keys from database: {e}")
        return []
