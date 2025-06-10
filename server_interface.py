#!/usr/bin/env python3

import tkinter as tk
from tkinter import ttk, messagebox, scrolledtext, filedialog
import subprocess
import threading
import os
import sys
import time
from pathlib import Path
import psutil
import sqlite3


try:
    from PIL import Image, ImageTk

    PIL_AVAILABLE = True
except ImportError:
    PIL_AVAILABLE = False
from core.secure_storage import SecureStorage


class ObsidianNeuralLauncher:
    def __init__(self):
        self.root = tk.Tk()
        self.root.title("OBSIDIAN-Neural - Launcher & Control Panel")
        self.root.geometry("900x700")
        self.root.minsize(850, 650)
        self.root.resizable(True, True)
        self.api_keys_list = []
        self.root.update_idletasks()
        x = (self.root.winfo_screenwidth() // 2) - (900 // 2)
        y = (self.root.winfo_screenheight() // 2) - (700 // 2)
        self.root.geometry(f"900x700+{x}+{y}")

        self.server_process = None
        self.is_server_running = False

        self.log_text = None

        self.init_database()
        self.setup_variables()

        self.setup_ui()

        self.installation_dir = self.detect_installation_dir()
        if self.installation_dir != Path.cwd():
            os.chdir(self.installation_dir)
        self.load_config()
        self.load_api_keys()

        if self.check_first_launch():
            self.load_config()
            self.load_api_keys()

        self.enable_auto_save()
        self.monitor_server()

    def handle_admin_install(self):
        install_dir = self.detect_installation_dir()

        if str(install_dir).startswith("C:\\ProgramData"):
            self.log("Admin installation detected - using local config copy")

            admin_env = install_dir / ".env"
            local_env = Path(".env")

            if admin_env.exists() and not local_env.exists():
                try:
                    import shutil

                    shutil.copy2(admin_env, local_env)
                    self.log("Configuration copied to local directory")
                except Exception as e:
                    self.log(f"Could not copy config: {e}", "WARNING")

            info_msg = f"""Installation Mode: Administrator
    Install Location: {install_dir}
    Config Location: {local_env.absolute()}

    Note: Configuration is stored locally for security.
    Changes will apply to the local server instance."""

            messagebox.showinfo("Admin Installation Detected", info_msg)

            return local_env
        else:
            return install_dir / ".env"

    def get_env_path(self):
        return self.handle_admin_install()

    def detect_installation_dir(self):
        possible_paths = [
            Path("C:/ProgramData/OBSIDIAN-Neural"),
            Path.home() / "OBSIDIAN-Neural",
            Path.cwd(),
        ]

        for path in possible_paths:
            if (path / "main.py").exists():
                self.log(f"Found OBSIDIAN-Neural installation at: {path}")
                return path

        self.log("Using current directory as installation path")
        return Path.cwd()

    def setup_variables(self):
        self.api_keys_list = []
        self.model_path = tk.StringVar(value="")
        self.environment = tk.StringVar(value="dev")
        self.host = tk.StringVar(value="127.0.0.1")
        self.port = tk.StringVar(value="8000")
        self.audio_model = tk.StringVar(value="stabilityai/stable-audio-open-1.0")
        self.server_status = tk.StringVar(value="Server Stopped")
        self.server_url = tk.StringVar(value="")
        self.auto_save_enabled = False
        self.hf_token_var = tk.StringVar(value="")

    def enable_auto_save(self):
        if self.auto_save_enabled:
            return

        self.auto_save_enabled = True
        variables = [
            self.model_path,
            self.environment,
            self.host,
            self.port,
            self.audio_model,
            self.hf_token_var,
        ]

        for var in variables:
            var.trace_add("write", lambda *args: self.auto_save_config())

    def auto_save_config(self):
        if hasattr(self, "db_path") and self.auto_save_enabled:
            if hasattr(self, "_save_timer"):
                self.root.after_cancel(self._save_timer)
            self._save_timer = self.root.after(1000, self.save_config)

    def setup_ui(self):
        style = ttk.Style()
        if "clam" in style.theme_names():
            style.theme_use("clam")
        style.configure("Success.TLabel", foreground="dark green")
        style.configure("Warning.TLabel", foreground="orange")
        style.configure("Error.TLabel", foreground="red")
        style.configure("Running.TButton", background="light green")
        style.configure("Stopped.TButton", background="light coral")

        main_frame = ttk.Frame(self.root, padding="20")
        main_frame.pack(fill="both", expand=True)

        self.create_header(main_frame)
        self.notebook = ttk.Notebook(main_frame)
        self.notebook.pack(fill="both", expand=True, pady=(20, 0))

        self.create_control_tab()
        self.create_config_tab()

        self.create_logs_tab()

    def create_header(self, parent):
        header_frame = ttk.Frame(parent)
        header_frame.pack(fill="x", pady=(0, 20))

        logo_path = Path("logo.png")
        if logo_path.exists() and PIL_AVAILABLE:
            try:
                image = Image.open(logo_path)
                image = image.resize((64, 64), Image.Resampling.LANCZOS)
                self.logo_photo = ImageTk.PhotoImage(image)

                logo_title_frame = ttk.Frame(header_frame)
                logo_title_frame.pack()

                logo_label = ttk.Label(logo_title_frame, image=self.logo_photo)
                logo_label.pack(side="left", padx=(0, 15))

                titles_frame = ttk.Frame(logo_title_frame)
                titles_frame.pack(side="left")

                title_label = ttk.Label(
                    titles_frame, text="OBSIDIAN-Neural", font=("Arial", 20, "bold")
                )
                title_label.pack(anchor="w")

                subtitle_label = ttk.Label(
                    titles_frame,
                    text="Real-time AI Music Generation Control Panel",
                    font=("Arial", 12),
                )
                subtitle_label.pack(anchor="w")
            except Exception:
                self.create_title_without_logo(header_frame)
        else:
            self.create_title_without_logo(header_frame)

    def create_title_without_logo(self, parent):
        title_label = ttk.Label(
            parent, text="OBSIDIAN-Neural", font=("Arial", 20, "bold")
        )
        title_label.pack()

        subtitle_label = ttk.Label(
            parent,
            text="Real-time AI Music Generation Control Panel",
            font=("Arial", 12),
        )
        subtitle_label.pack()

    def create_control_tab(self):
        control_frame = ttk.Frame(self.notebook, padding="20")
        self.notebook.add(control_frame, text="🚀 Server Control")

        status_frame = ttk.LabelFrame(control_frame, text="Server Status", padding="15")
        status_frame.pack(fill="x", pady=(0, 20))

        status_display_frame = ttk.Frame(status_frame)
        status_display_frame.pack(fill="x", pady=(0, 15))

        self.status_label = ttk.Label(
            status_display_frame,
            textvariable=self.server_status,
            font=("Arial", 14, "bold"),
        )
        self.status_label.pack(side="left")

        self.url_label = ttk.Label(
            status_display_frame,
            textvariable=self.server_url,
            font=("Arial", 10),
            foreground="blue",
        )
        self.url_label.pack(side="left", padx=(20, 0))
        button_frame = ttk.Frame(status_frame)
        button_frame.pack(fill="x")

        self.start_button = ttk.Button(
            button_frame, text="🎵 Start Server", command=self.start_server
        )
        self.start_button.pack(side="left", padx=(0, 10))

        self.stop_button = ttk.Button(
            button_frame,
            text="⏹ Stop Server",
            command=self.stop_server,
            state="disabled",
        )
        self.stop_button.pack(side="left", padx=(0, 10))

        self.restart_button = ttk.Button(
            button_frame,
            text="🔄 Restart Server",
            command=self.restart_server,
            state="disabled",
        )
        self.restart_button.pack(side="left", padx=(0, 10))
        actions_frame = ttk.LabelFrame(
            control_frame, text="Quick Actions", padding="15"
        )
        actions_frame.pack(fill="x", pady=(0, 20))

        actions_button_frame = ttk.Frame(actions_frame)
        actions_button_frame.pack()

        ttk.Button(
            actions_button_frame,
            text="📁 Open Project Folder",
            command=self.open_project_folder,
        ).pack(side="left", padx=(0, 10))

        ttk.Button(
            actions_button_frame, text="📊 System Info", command=self.show_system_info
        ).pack(side="left")

    def save_hf_token(self):
        try:
            conn = sqlite3.connect(self.db_path)
            cursor = conn.cursor()

            token = self.hf_token_var.get().strip()
            if token:
                encrypted_token = self.secure_storage.encrypt(token)
                cursor.execute(
                    "INSERT OR REPLACE INTO config (key, value, is_encrypted) VALUES (?, ?, ?)",
                    ("hf_token", encrypted_token, 1),
                )

                os.environ["HF_TOKEN"] = token
                self.log("Hugging Face token saved and set in environment", "SUCCESS")
            else:
                cursor.execute("DELETE FROM config WHERE key = ?", ("hf_token",))
                os.environ.pop("HF_TOKEN", None)
                self.log("Hugging Face token removed", "SUCCESS")

            conn.commit()
            conn.close()
            return True

        except Exception as e:
            self.log(f"Error saving HF token: {e}", "ERROR")
            return False

    def verify_hf_token_config(self):
        token = self.hf_token_var.get().strip()
        if not token:
            messagebox.showerror("Error", "Please enter your Hugging Face token")
            return

        if not token.startswith("hf_") or len(token) < 30:
            messagebox.showerror(
                "Invalid Format",
                "Hugging Face tokens should start with 'hf_' and be ~37 characters long.\n\n"
                "Create a token at: https://huggingface.co/settings/tokens",
            )
            return

        try:
            import requests

            self.log("Verifying Hugging Face token...", "INFO")

            headers = {"Authorization": f"Bearer {token}"}

            response = requests.get(
                "https://huggingface.co/api/whoami-v2", headers=headers, timeout=15
            )

            if response.status_code != 200:
                if response.status_code == 401:
                    messagebox.showerror(
                        "Invalid Token",
                        "❌ Token is invalid or expired\n\n"
                        "Please check your token at:\n"
                        "https://huggingface.co/settings/tokens",
                    )
                else:
                    messagebox.showerror(
                        "Verification Failed",
                        f"❌ Could not verify token\n"
                        f"HTTP {response.status_code}: {response.text[:200]}",
                    )
                return

            user_info = response.json()
            username = user_info.get("name", user_info.get("fullname", "Unknown"))
            self.log(f"Token valid for user: {username}", "SUCCESS")

            model_tests = [
                (
                    "HEAD request",
                    "HEAD",
                    "https://huggingface.co/stabilityai/stable-audio-open-1.0",
                ),
                (
                    "Model info API",
                    "GET",
                    "https://huggingface.co/api/models/stabilityai/stable-audio-open-1.0",
                ),
                (
                    "Files API",
                    "GET",
                    "https://huggingface.co/api/models/stabilityai/stable-audio-open-1.0/tree/main",
                ),
            ]

            access_results = []
            for test_name, method, url in model_tests:
                try:
                    if method == "HEAD":
                        test_response = requests.head(url, headers=headers, timeout=10)
                    else:
                        test_response = requests.get(url, headers=headers, timeout=10)

                    access_results.append(f"{test_name}: {test_response.status_code}")
                    self.log(f"{test_name}: HTTP {test_response.status_code}")

                    if test_response.status_code == 200:
                        messagebox.showinfo(
                            "Token Valid",
                            f"✅ Token verified!\n"
                            f"User: {username}\n"
                            f"✅ Stable Audio access confirmed via {test_name}",
                        )
                        self.save_hf_token()
                        return

                except Exception as e:
                    access_results.append(f"{test_name}: Error - {str(e)[:50]}")
                    self.log(f"{test_name}: Error - {e}")

            if any("403" in result for result in access_results):
                messagebox.showwarning(
                    "Access Required",
                    f"✅ Token is valid for user: {username}\n"
                    f"❌ Access to Stable Audio model required\n\n"
                    f"Steps to fix:\n"
                    f"1. Go to: https://huggingface.co/stabilityai/stable-audio-open-1.0\n"
                    f"2. Click 'Request access'\n"
                    f"3. Wait for approval (usually instant)\n"
                    f"4. Create a NEW token with 'read' permissions\n\n"
                    f"Debug info:\n" + "\n".join(access_results),
                )
            else:
                messagebox.showerror(
                    "Access Issues",
                    f"✅ Token is valid for user: {username}\n"
                    f"❌ Cannot access Stable Audio model\n\n"
                    f"Debug info:\n" + "\n".join(access_results),
                )

        except requests.exceptions.Timeout:
            messagebox.showerror(
                "Timeout", "❌ Request timed out. Check your internet connection."
            )
        except requests.exceptions.ConnectionError:
            messagebox.showerror(
                "Connection Error", "❌ Could not connect to Hugging Face."
            )
        except Exception as e:
            messagebox.showerror("Verification Error", f"Could not verify token:\n{e}")

    def create_config_tab(self):
        config_frame = ttk.Frame(self.notebook, padding="20")
        self.notebook.add(config_frame, text="⚙️ Configuration")

        canvas = tk.Canvas(config_frame, highlightthickness=0, bd=0)
        scrollbar = ttk.Scrollbar(config_frame, orient="vertical", command=canvas.yview)
        scrollable_frame = ttk.Frame(canvas)

        scrollable_frame.bind(
            "<Configure>", lambda e: canvas.configure(scrollregion=canvas.bbox("all"))
        )

        canvas.pack(side="left", fill="both", expand=True)
        scrollbar.pack(side="right", fill="y")

        canvas_window = canvas.create_window(
            (0, 0), window=scrollable_frame, anchor="nw"
        )

        def configure_scroll_region(event=None):
            canvas.configure(scrollregion=canvas.bbox("all"))
            canvas.itemconfig(canvas_window, width=canvas.winfo_width())

        canvas.bind("<Configure>", configure_scroll_region)
        canvas.configure(yscrollcommand=scrollbar.set)

        api_frame = ttk.LabelFrame(
            scrollable_frame, text="API Keys Management", padding="15"
        )
        api_frame.pack(fill="x", pady=(0, 15), padx=10)

        ttk.Label(api_frame, text="API Keys:").pack(anchor="w")

        list_frame = ttk.Frame(api_frame)
        list_frame.pack(fill="x", pady=(5, 10))

        list_scroll = ttk.Scrollbar(list_frame, orient="vertical")
        self.api_keys_listbox = tk.Listbox(
            list_frame, height=4, yscrollcommand=list_scroll.set, font=("Consolas", 9)
        )
        list_scroll.config(command=self.api_keys_listbox.yview)

        self.api_keys_listbox.pack(side="left", fill="both", expand=True)
        list_scroll.pack(side="right", fill="y")

        api_buttons_frame = ttk.Frame(api_frame)
        api_buttons_frame.pack(fill="x", pady=(0, 10))

        ttk.Button(
            api_buttons_frame, text="🔑 Generate New Key", command=self.generate_api_key
        ).pack(side="left", padx=(0, 5))
        ttk.Button(
            api_buttons_frame, text="📋 Add Custom Key", command=self.add_custom_api_key
        ).pack(side="left", padx=(0, 5))
        ttk.Button(
            api_buttons_frame, text="✏️ Rename Selected", command=self.rename_api_key
        ).pack(side="left", padx=(0, 5))
        ttk.Button(
            api_buttons_frame, text="🗑 Remove Selected", command=self.remove_api_key
        ).pack(side="left", padx=(0, 5))
        ttk.Button(
            api_buttons_frame, text="📋 Copy Selected", command=self.copy_api_key
        ).pack(side="left")

        hf_frame = ttk.LabelFrame(
            scrollable_frame, text="Hugging Face Configuration", padding="15"
        )
        hf_frame.pack(fill="x", pady=(0, 15), padx=10)

        ttk.Label(hf_frame, text="Access Token (for Stable Audio model):").pack(
            anchor="w"
        )

        hf_token_frame = ttk.Frame(hf_frame)
        hf_token_frame.pack(fill="x", pady=(5, 10))

        self.hf_show_token = tk.BooleanVar(value=False)

        def update_hf_display():
            if self.hf_show_token.get():
                hf_entry.config(show="")
                hf_show_btn.config(text="🙈 Hide")
            else:
                hf_entry.config(show="*")
                hf_show_btn.config(text="👁 Show")

        hf_entry = ttk.Entry(
            hf_token_frame,
            textvariable=self.hf_token_var,
            font=("Consolas", 10),
            show="*",
            width=50,
        )
        hf_entry.pack(side="left", fill="x", expand=True)

        def on_hf_token_change(*args):
            if hasattr(self, "auto_save_enabled") and self.auto_save_enabled:
                if hasattr(self, "_hf_save_timer"):
                    self.root.after_cancel(self._hf_save_timer)
                self._hf_save_timer = self.root.after(2000, self.save_hf_token)

        self.hf_token_var.trace_add("write", on_hf_token_change)
        hf_show_btn = ttk.Button(
            hf_token_frame,
            text="👁 Show",
            width=8,
            command=lambda: [
                self.hf_show_token.set(not self.hf_show_token.get()),
                update_hf_display(),
            ],
        )
        hf_show_btn.pack(side="left", padx=(5, 5))
        ttk.Button(
            hf_token_frame, text="🔍 Verify", command=self.verify_hf_token_config
        ).pack(side="left")

        hf_help_text = "Get your token at: https://huggingface.co/settings/tokens\nRequest access to: https://huggingface.co/stabilityai/stable-audio-open-1.0"
        ttk.Label(
            hf_frame, text=hf_help_text, font=("Arial", 9), foreground="gray"
        ).pack(anchor="w", pady=(5, 0))

        model_frame = ttk.LabelFrame(
            scrollable_frame, text="Model Configuration", padding="15"
        )
        model_frame.pack(fill="x", pady=(0, 15), padx=10)

        ttk.Label(model_frame, text="LLM Model Path:").pack(anchor="w")
        model_path_frame = ttk.Frame(model_frame)
        model_path_frame.pack(fill="x", pady=(5, 10))

        ttk.Entry(model_path_frame, textvariable=self.model_path, width=50).pack(
            side="left", fill="x", expand=True
        )
        ttk.Button(
            model_path_frame, text="Browse", command=self.browse_model_path
        ).pack(side="right", padx=(5, 0))

        ttk.Label(model_frame, text="Audio Model:").pack(anchor="w")
        audio_model_combo = ttk.Combobox(
            model_frame,
            textvariable=self.audio_model,
            values=[
                "stabilityai/stable-audio-open-1.0",
                "stabilityai/stable-audio-open-small",
            ],
            state="readonly",
        )
        audio_model_combo.pack(fill="x", pady=(5, 0))

        server_frame = ttk.LabelFrame(
            scrollable_frame, text="Server Configuration", padding="15"
        )
        server_frame.pack(fill="x", pady=(0, 15), padx=10)

        host_port_frame = ttk.Frame(server_frame)
        host_port_frame.pack(fill="x", pady=(0, 10))

        ttk.Label(host_port_frame, text="Host:").pack(side="left")
        ttk.Entry(host_port_frame, textvariable=self.host, width=15).pack(
            side="left", padx=(5, 20)
        )

        ttk.Label(host_port_frame, text="Port:").pack(side="left")
        ttk.Entry(host_port_frame, textvariable=self.port, width=10).pack(
            side="left", padx=(5, 0)
        )

        ttk.Label(server_frame, text="Environment:").pack(anchor="w")
        env_combo = ttk.Combobox(
            server_frame,
            textvariable=self.environment,
            values=["dev", "prod"],
            state="readonly",
        )
        env_combo.pack(fill="x", pady=(5, 0))

        button_frame = ttk.Frame(scrollable_frame)
        button_frame.pack(fill="x", padx=10, pady=15)

        ttk.Button(
            button_frame, text="💾 Save Configuration", command=self.manual_save_config
        ).pack(side="left", padx=(0, 10))

        ttk.Button(
            button_frame, text="🔄 Reload Configuration", command=self.reload_config
        ).pack(side="left", padx=(0, 10))

        ttk.Button(
            button_frame, text="🗑 Clear All Data", command=self.clear_all_data
        ).pack(side="left", padx=(0, 10))

        ttk.Button(
            button_frame, text="🔧 Reset to Defaults", command=self.reset_to_defaults
        ).pack(side="left")

        def _on_mousewheel(event):
            canvas.yview_scroll(int(-1 * (event.delta / 120)), "units")

        canvas.bind_all("<MouseWheel>", _on_mousewheel)

    def rename_api_key(self):
        selection = self.api_keys_listbox.curselection()
        if not selection:
            messagebox.showwarning("No Selection", "Please select an API key to rename")
            return

        index = selection[0]

        try:
            conn = sqlite3.connect(self.db_path)
            cursor = conn.cursor()

            cursor.execute(
                "SELECT id, key_value_encrypted, name FROM api_keys ORDER BY created_at"
            )
            rows = cursor.fetchall()

            if index >= len(rows):
                conn.close()
                messagebox.showerror("Error", "Invalid selection")
                return

            db_id, encrypted_key, current_name = rows[index]
            decrypted_key = self.secure_storage.decrypt(encrypted_key)

            new_name = tk.simpledialog.askstring(
                "Rename API Key",
                f"Enter new name for key {decrypted_key[:8]}...:",
                initialvalue=current_name or f"Key {index + 1}",
            )

            if new_name and new_name.strip():
                cursor.execute(
                    "UPDATE api_keys SET name = ? WHERE id = ?",
                    (new_name.strip(), db_id),
                )
                conn.commit()
                conn.close()

                self.update_api_keys_listbox()
                self.log(f"API key renamed to '{new_name.strip()}'", "SUCCESS")
            else:
                conn.close()

        except Exception as e:
            self.log(f"Error renaming API key: {e}", "ERROR")
            messagebox.showerror("Error", f"Could not rename API key: {e}")

    def manual_save_config(self):
        if self.save_config():
            messagebox.showinfo("Saved", "Configuration saved successfully!")

    def reload_config(self):
        self.load_config()
        self.load_api_keys()
        messagebox.showinfo("Reloaded", "Configuration reloaded from database!")

    def clear_all_data(self):
        if messagebox.askyesno(
            "Clear All Data",
            "This will delete all API keys and configuration.\nAre you sure?",
        ):
            try:
                conn = sqlite3.connect(self.db_path)
                cursor = conn.cursor()

                cursor.execute("DELETE FROM api_keys")
                cursor.execute("DELETE FROM config")

                conn.commit()
                conn.close()

                self.api_keys_list = []
                self.update_api_keys_listbox()
                self.reset_to_defaults()

                self.log("All data cleared from database", "SUCCESS")
                messagebox.showinfo("Cleared", "All data has been cleared!")

            except Exception as e:
                self.log(f"Error clearing data: {e}", "ERROR")
                messagebox.showerror("Error", f"Could not clear data: {e}")

    def init_database(self):
        self.db_path = Path.home() / ".obsidian_neural" / "config.db"
        self.db_path.parent.mkdir(exist_ok=True)

        self.secure_storage = SecureStorage(self.db_path)

        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()

        cursor.execute(
            """
            CREATE TABLE IF NOT EXISTS api_keys (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                key_value_encrypted TEXT UNIQUE NOT NULL,
                name TEXT,
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                last_used TIMESTAMP
            )
        """
        )

        cursor.execute(
            """
            CREATE TABLE IF NOT EXISTS config (
                key TEXT PRIMARY KEY,
                value TEXT,
                is_encrypted INTEGER DEFAULT 0
            )
        """
        )

        cursor.execute("PRAGMA table_info(api_keys)")
        api_columns = [column[1] for column in cursor.fetchall()]

        if "key_value" in api_columns:
            self.log("Migrating API keys to encrypted storage...", "WARNING")

            cursor.execute("SELECT key_value, name FROM api_keys")
            old_data = cursor.fetchall()

            cursor.execute("DROP TABLE api_keys")
            cursor.execute(
                """
                CREATE TABLE api_keys (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    key_value_encrypted TEXT UNIQUE NOT NULL,
                    name TEXT,
                    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                    last_used TIMESTAMP
                )
            """
            )

            for key_value, name in old_data:
                encrypted_key = self.secure_storage.encrypt(key_value)
                cursor.execute(
                    "INSERT INTO api_keys (key_value_encrypted, name) VALUES (?, ?)",
                    (encrypted_key, name),
                )

            self.log(
                f"Migrated {len(old_data)} API keys to encrypted storage", "SUCCESS"
            )

        cursor.execute("PRAGMA table_info(config)")
        config_columns = [column[1] for column in cursor.fetchall()]

        if "is_encrypted" not in config_columns:
            self.log("Adding encryption support to config table...", "WARNING")
            cursor.execute(
                "ALTER TABLE config ADD COLUMN is_encrypted INTEGER DEFAULT 0"
            )
            self.log("Config table updated with encryption support", "SUCCESS")

        conn.commit()
        conn.close()

        self.log(f"Secure database initialized at: {self.db_path}")

    def create_logs_tab(self):
        logs_frame = ttk.Frame(self.notebook, padding="20")
        self.notebook.add(logs_frame, text="📝 Logs")

        controls_frame = ttk.Frame(logs_frame)
        controls_frame.pack(fill="x", pady=(0, 10))

        ttk.Button(controls_frame, text="🗑 Clear Logs", command=self.clear_logs).pack(
            side="left", padx=(0, 10)
        )

        ttk.Button(controls_frame, text="💾 Save Logs", command=self.save_logs).pack(
            side="left", padx=(0, 10)
        )

        self.auto_scroll = tk.BooleanVar(value=True)
        ttk.Checkbutton(
            controls_frame, text="Auto-scroll", variable=self.auto_scroll
        ).pack(side="left", padx=(10, 0))

        self.log_text = scrolledtext.ScrolledText(
            logs_frame, height=20, font=("Consolas", 9)
        )
        self.log_text.pack(fill="both", expand=True)

        self.log_text.tag_configure("INFO", foreground="black")
        self.log_text.tag_configure("SUCCESS", foreground="dark green")
        self.log_text.tag_configure("WARNING", foreground="orange")
        self.log_text.tag_configure("ERROR", foreground="red")

    def log(self, message, level="INFO"):
        timestamp = time.strftime("%H:%M:%S")
        emoji = {"INFO": "ℹ️ ", "SUCCESS": "✅", "WARNING": "⚠️ ", "ERROR": "❌"}.get(
            level, "ℹ️"
        )

        formatted_msg = f"[{timestamp}] {emoji} {message}\n"

        if self.log_text is not None:
            self.log_text.insert(tk.END, formatted_msg, level)

            if hasattr(self, "auto_scroll") and self.auto_scroll.get():
                self.log_text.see(tk.END)

            self.root.update_idletasks()
        else:
            print(f"{formatted_msg.strip()}")

    def load_api_keys(self):
        try:
            conn = sqlite3.connect(self.db_path)
            cursor = conn.cursor()

            cursor.execute(
                "SELECT key_value_encrypted FROM api_keys ORDER BY created_at"
            )
            rows = cursor.fetchall()

            self.api_keys_list = []
            for row in rows:
                decrypted_key = self.secure_storage.decrypt(row[0])
                if decrypted_key:
                    self.api_keys_list.append(decrypted_key)

            self.update_api_keys_listbox()
            conn.close()

            if self.api_keys_list:
                self.log(f"Loaded {len(self.api_keys_list)} encrypted API key(s)")

        except Exception as e:
            self.log(f"Error loading API keys: {e}", "WARNING")
            self.api_keys_list = []

    def save_api_key(self, api_key, name=None):
        try:
            encrypted_key = self.secure_storage.encrypt(api_key)

            conn = sqlite3.connect(self.db_path)
            cursor = conn.cursor()

            cursor.execute(
                "INSERT OR IGNORE INTO api_keys (key_value_encrypted, name) VALUES (?, ?)",
                (encrypted_key, name),
            )

            conn.commit()
            conn.close()

            return True
        except Exception as e:
            self.log(f"Error saving API key: {e}", "ERROR")
            return False

    def remove_api_key(self):
        selection = self.api_keys_listbox.curselection()
        if not selection:
            messagebox.showwarning("No Selection", "Please select an API key to remove")
            return

        index = selection[0]

        try:
            conn = sqlite3.connect(self.db_path)
            cursor = conn.cursor()

            cursor.execute(
                "SELECT id, key_value_encrypted, name FROM api_keys ORDER BY created_at"
            )
            rows = cursor.fetchall()

            if index >= len(rows):
                conn.close()
                messagebox.showerror("Error", "Invalid selection")
                return

            db_id, encrypted_key, name = rows[index]

            decrypted_key = self.secure_storage.decrypt(encrypted_key)
            display_name = name if name else f"Key {index + 1}"

            if not messagebox.askyesno(
                "Confirm Deletion",
                f"Are you sure you want to delete:\n'{display_name}'\n{decrypted_key[:8]}...{decrypted_key[-4:]}?",
            ):
                conn.close()
                return

            cursor.execute("DELETE FROM api_keys WHERE id = ?", (db_id,))
            deleted_count = cursor.rowcount

            conn.commit()
            conn.close()

            if deleted_count > 0:
                self.load_api_keys()
                self.log(f"API key '{display_name}' removed successfully", "SUCCESS")
            else:
                messagebox.showerror("Error", "Could not remove API key from database")

        except Exception as e:
            self.log(f"Error removing API key: {e}", "ERROR")
            messagebox.showerror("Error", f"Could not remove API key: {e}")

    def remove_api_key_from_db(self, api_key):
        try:
            encrypted_key = self.secure_storage.encrypt(api_key)

            conn = sqlite3.connect(self.db_path)
            cursor = conn.cursor()

            cursor.execute(
                "DELETE FROM api_keys WHERE key_value_encrypted = ?", (encrypted_key,)
            )
            deleted_count = cursor.rowcount

            conn.commit()
            conn.close()

            if deleted_count > 0:
                self.log(f"Removed {deleted_count} API key from database")
                return True
            else:
                self.log("No matching API key found in database", "WARNING")
                return False

        except Exception as e:
            self.log(f"Error removing API key: {e}", "ERROR")
            return False

    def save_config(self):
        try:
            conn = sqlite3.connect(self.db_path)
            cursor = conn.cursor()

            config_items = [
                ("model_path", self.model_path.get(), 0),
                ("environment", self.environment.get(), 0),
                ("host", self.host.get(), 0),
                ("port", self.port.get(), 0),
                ("audio_model", self.audio_model.get(), 0),
            ]

            if hasattr(self, "hf_token_var") and self.hf_token_var.get().strip():
                self.save_hf_token()

            for key, value, is_encrypted in config_items:
                cursor.execute(
                    "INSERT OR REPLACE INTO config (key, value, is_encrypted) VALUES (?, ?, ?)",
                    (key, value, is_encrypted),
                )

            conn.commit()
            conn.close()

            return True
        except Exception as e:
            self.log(f"Error saving configuration: {e}", "ERROR")
            return False

    def load_config(self):
        try:
            conn = sqlite3.connect(self.db_path)
            cursor = conn.cursor()

            cursor.execute("SELECT key, value, is_encrypted FROM config")
            rows = cursor.fetchall()

            for key, value, is_encrypted in rows:
                if is_encrypted:
                    decrypted_value = self.secure_storage.decrypt(value)
                    if key == "hf_token" and hasattr(self, "hf_token_var"):
                        self.hf_token_var.set(decrypted_value)
                else:
                    if key == "model_path":
                        self.model_path.set(value)
                    elif key == "environment":
                        self.environment.set(value)
                    elif key == "host":
                        self.host.set(value)
                    elif key == "port":
                        self.port.set(value)
                    elif key == "audio_model":
                        self.audio_model.set(value)

            conn.close()
            self.log(f"Configuration loaded with encryption support")

        except Exception as e:
            self.log(f"Error loading configuration: {e}", "WARNING")

    def generate_api_key(self):
        name_dialog = tk.Toplevel(self.root)
        name_dialog.title("Name Your API Key")
        name_dialog.geometry("400x150")
        name_dialog.transient(self.root)
        name_dialog.grab_set()

        name_dialog.update_idletasks()
        x = (name_dialog.winfo_screenwidth() // 2) - (400 // 2)
        y = (name_dialog.winfo_screenheight() // 2) - (150 // 2)
        name_dialog.geometry(f"400x150+{x}+{y}")

        frame = ttk.Frame(name_dialog, padding="20")
        frame.pack(fill="both", expand=True)

        ttk.Label(frame, text="Enter a name for this API key:").pack(anchor="w")

        name_var = tk.StringVar(value=f"API Key {len(self.api_keys_list) + 1}")
        name_entry = ttk.Entry(frame, textvariable=name_var, width=40)
        name_entry.pack(fill="x", pady=(10, 20))
        name_entry.select_range(0, tk.END)
        name_entry.focus()

        result = {"name": None}

        def confirm_name():
            name = name_var.get().strip()
            if name:
                result["name"] = name
                name_dialog.destroy()
            else:
                messagebox.showerror("Error", "Please enter a key name")

        def cancel_name():
            name_dialog.destroy()

        button_frame = ttk.Frame(frame)
        button_frame.pack(fill="x")

        ttk.Button(button_frame, text="Generate Key", command=confirm_name).pack(
            side="right"
        )
        ttk.Button(button_frame, text="Cancel", command=cancel_name).pack(
            side="right", padx=(0, 10)
        )

        name_entry.bind("<Return>", lambda e: confirm_name())

        name_dialog.wait_window()

        if result["name"]:
            api_key = self.generate_unique_api_key()

            if self.save_api_key(api_key, result["name"]):
                self.api_keys_list.append(api_key)
                self.update_api_keys_listbox()

                self.show_api_key_dialog(
                    api_key, f"Generated API Key: {result['name']}"
                )
                self.log(
                    f"New API key '{result['name']}' generated: {api_key[:8]}...",
                    "SUCCESS",
                )
            else:
                messagebox.showerror("Error", "Could not save API key to database")

    def add_custom_api_key(self):
        dialog = tk.Toplevel(self.root)
        dialog.title("Add Custom API Key")
        dialog.geometry("450x200")
        dialog.transient(self.root)
        dialog.grab_set()

        dialog.update_idletasks()
        x = (dialog.winfo_screenwidth() // 2) - (450 // 2)
        y = (dialog.winfo_screenheight() // 2) - (200 // 2)
        dialog.geometry(f"450x200+{x}+{y}")

        frame = ttk.Frame(dialog, padding="20")
        frame.pack(fill="both", expand=True)

        ttk.Label(frame, text="Key Name:").pack(anchor="w")
        key_name_var = tk.StringVar(value="Custom Key")
        name_entry = ttk.Entry(frame, textvariable=key_name_var, width=50)
        name_entry.pack(fill="x", pady=(5, 15))

        ttk.Label(frame, text="API Key:").pack(anchor="w")
        key_var = tk.StringVar()
        entry = ttk.Entry(frame, textvariable=key_var, width=50, show="*")
        entry.pack(fill="x", pady=(5, 15))
        entry.focus()

        show_frame = ttk.Frame(frame)
        show_frame.pack(fill="x", pady=(0, 15))

        show_custom = tk.BooleanVar(value=False)

        def toggle_show():
            if show_custom.get():
                entry.config(show="")
                show_btn.config(text="🙈 Hide")
            else:
                entry.config(show="*")
                show_btn.config(text="👁 Show")

        show_btn = ttk.Button(
            show_frame,
            text="👁 Show",
            command=lambda: [show_custom.set(not show_custom.get()), toggle_show()],
        )
        show_btn.pack(side="left")

        button_frame = ttk.Frame(frame)
        button_frame.pack(fill="x")

        def add_key():
            key = key_var.get().strip()
            name = key_name_var.get().strip()

            if not name:
                messagebox.showerror("Error", "Please enter a key name")
                return

            if key:
                if key not in self.api_keys_list:
                    if self.save_api_key(key, name):
                        self.api_keys_list.append(key)
                        self.update_api_keys_listbox()
                        self.log(
                            f"Custom API key '{name}' added: {key[:8]}...", "SUCCESS"
                        )
                        dialog.destroy()
                    else:
                        messagebox.showerror(
                            "Error", "Could not save API key to database"
                        )
                else:
                    messagebox.showwarning("Duplicate", "This API key already exists")
            else:
                messagebox.showerror("Error", "Please enter a valid API key")

        ttk.Button(button_frame, text="Add Key", command=add_key).pack(
            side="left", padx=(0, 10)
        )
        ttk.Button(button_frame, text="Cancel", command=dialog.destroy).pack(
            side="left"
        )

        entry.bind("<Return>", lambda e: add_key())

    def copy_api_key(self):
        selection = self.api_keys_listbox.curselection()
        if selection:
            index = selection[0]
            api_key = self.api_keys_list[index]
            self.root.clipboard_clear()
            self.root.clipboard_append(api_key)
            self.log(f"API key copied to clipboard: {api_key[:8]}...", "SUCCESS")
            messagebox.showinfo(
                "Copied", f"API key copied to clipboard:\n{api_key[:16]}..."
            )
        else:
            messagebox.showwarning("No Selection", "Please select an API key to copy")

    def show_api_key_dialog(self, api_key, title):
        dialog = tk.Toplevel(self.root)
        dialog.title(title)
        dialog.geometry("600x300")
        dialog.transient(self.root)
        dialog.grab_set()

        dialog.update_idletasks()
        x = (dialog.winfo_screenwidth() // 2) - (600 // 2)
        y = (dialog.winfo_screenheight() // 2) - (300 // 2)
        dialog.geometry(f"600x300+{x}+{y}")

        frame = ttk.Frame(dialog, padding="20")
        frame.pack(fill="both", expand=True)

        ttk.Label(frame, text="Your API Key:", font=("Arial", 12, "bold")).pack(
            anchor="w"
        )

        warning_label = ttk.Label(
            frame,
            text="⚠️ Save this key securely - it won't be shown again in full!",
            foreground="red",
            font=("Arial", 10),
        )
        warning_label.pack(anchor="w", pady=(5, 15))

        key_frame = ttk.Frame(frame)
        key_frame.pack(fill="x", pady=(0, 20))

        key_entry = ttk.Entry(
            key_frame,
            font=("Consolas", 12),
            width=60,
        )
        key_entry.pack(fill="x", pady=(0, 10))

        key_entry.insert(0, api_key)

        def select_all():
            key_entry.select_range(0, tk.END)
            key_entry.focus()

        dialog.after(200, select_all)

        button_frame = ttk.Frame(frame)
        button_frame.pack(fill="x")

        def copy_and_close():
            self.root.clipboard_clear()
            self.root.clipboard_append(api_key)
            self.log("API key copied to clipboard", "SUCCESS")
            messagebox.showinfo("Copied!", "API key copied to clipboard!")
            dialog.destroy()

        def select_all_key():
            key_entry.select_range(0, tk.END)
            key_entry.focus()

        ttk.Button(button_frame, text="📋 Copy & Close", command=copy_and_close).pack(
            side="left", padx=(0, 10)
        )
        ttk.Button(button_frame, text="🔍 Select All", command=select_all_key).pack(
            side="left", padx=(0, 10)
        )
        ttk.Button(button_frame, text="Close", command=dialog.destroy).pack(side="left")

        key_entry.focus()

    def update_api_keys_listbox(self):
        self.api_keys_listbox.delete(0, tk.END)

        try:
            conn = sqlite3.connect(self.db_path)
            cursor = conn.cursor()

            cursor.execute(
                "SELECT key_value_encrypted, name FROM api_keys ORDER BY created_at"
            )
            rows = cursor.fetchall()

            self.api_keys_list = []

            for i, (encrypted_key, name) in enumerate(rows):
                decrypted_key = self.secure_storage.decrypt(encrypted_key)
                if decrypted_key:
                    self.api_keys_list.append(decrypted_key)

                    display_name = name if name else f"Unnamed Key {i+1}"
                    display_key = (
                        f"[{display_name}] {decrypted_key[:8]}...{decrypted_key[-4:]}"
                    )
                    self.api_keys_listbox.insert(tk.END, display_key)

            conn.close()

        except Exception as e:
            self.log(f"Error updating listbox: {e}", "ERROR")

    def generate_unique_api_key(self, length=32):
        import secrets
        import string

        alphabet = string.ascii_letters + string.digits

        while True:
            api_key = "".join(secrets.choice(alphabet) for _ in range(length))
            if not self.api_key_exists(api_key):
                return api_key
            self.log(
                f"Generated key collision (rare!), generating new one...", "WARNING"
            )

    def api_key_exists(self, api_key):
        return api_key in self.api_keys_list

    def browse_model_path(self):
        filename = filedialog.askopenfilename(
            title="Select LLM Model File",
            filetypes=[("GGUF files", "*.gguf"), ("All files", "*.*")],
            initialdir=self.model_path.get() if self.model_path.get() else ".",
        )
        if filename:
            self.model_path.set(filename)

    def reset_to_defaults(self):
        if messagebox.askyesno(
            "Reset Configuration",
            "Reset all settings to defaults? (API keys will be kept)",
        ):
            self.model_path.set("")
            self.environment.set("dev")
            self.host.set("127.0.0.1")
            self.port.set("8000")
            self.audio_model.set("stabilityai/stable-audio-open-1.0")
            self.log("Configuration reset to defaults")

            self.save_config()

    def start_server(self):
        if self.is_server_running:
            self.log("Server is already running", "WARNING")
            return

        if not self.model_path.get() or not Path(self.model_path.get()).exists():
            messagebox.showerror("Error", "Please specify a valid model path")
            return

        try:
            install_dir = self.detect_installation_dir()
            main_py = install_dir / "main.py"

            env = os.environ.copy()
            env["PYTHONIOENCODING"] = "utf-8"

            if hasattr(self, "hf_token_var") and self.hf_token_var.get().strip():
                env["HF_TOKEN"] = self.hf_token_var.get().strip()
                self.log("Hugging Face token added to environment")
            elif hasattr(self, "secure_storage"):
                try:
                    conn = sqlite3.connect(self.db_path)
                    cursor = conn.cursor()
                    cursor.execute(
                        "SELECT value FROM config WHERE key = ? AND is_encrypted = 1",
                        ("hf_token",),
                    )
                    result = cursor.fetchone()
                    conn.close()

                    if result:
                        decrypted_token = self.secure_storage.decrypt(result[0])
                        if decrypted_token:
                            env["HF_TOKEN"] = decrypted_token
                            self.log("Hugging Face token loaded from database")
                except Exception as e:
                    self.log(f"Could not load HF token: {e}", "WARNING")

            if not main_py.exists():
                messagebox.showerror("Error", f"main.py not found at {install_dir}")
                return

            python_exe = install_dir / "env" / "Scripts" / "python.exe"
            if not python_exe.exists():
                python_exe = sys.executable
                self.log("Using system Python (venv not found)", "WARNING")

            self.log(f"Using Python: {python_exe}")
            self.log(f"Starting server from: {install_dir}")

            cmd = [
                str(python_exe),
                str(main_py),
                "--host",
                self.host.get(),
                "--port",
                self.port.get(),
                "--model-path",
                self.model_path.get(),
                "--environment",
                self.environment.get(),
                "--audio-model",
                self.audio_model.get(),
            ]

            environment = self.environment.get()
            has_api_keys = len(self.api_keys_list) > 0

            if environment == "prod":
                cmd.append("--use-stored-keys")
                self.log("Production mode: using stored API keys")
            elif environment == "dev" and has_api_keys:
                use_keys = messagebox.askyesno(
                    "Development Mode",
                    "You have API keys configured.\n\n"
                    "Use stored API keys for authentication?\n\n"
                    "• Yes: Use API authentication\n"
                    "• No: Development bypass (no auth)",
                )
                if use_keys:
                    cmd.append("--use-stored-keys")
                    self.log("Development mode: using stored API keys")
                else:
                    self.log("Development mode: authentication bypass enabled")
            else:
                self.log("Development mode: no API keys configured, bypass enabled")

            self.log(f"Command: {' '.join(cmd)}")

            self.server_process = subprocess.Popen(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                universal_newlines=True,
                bufsize=1,
                cwd=str(install_dir),
                env=env,
                encoding="utf-8",
                errors="replace",
            )

            self.is_server_running = True
            self.update_ui_state()

            threading.Thread(target=self.monitor_output, daemon=True).start()

            self.log("Server started successfully", "SUCCESS")

        except Exception as e:
            self.log(f"Failed to start server: {e}", "ERROR")
            messagebox.showerror("Error", f"Failed to start server:\n{e}")

    def stop_server(self):
        if not self.is_server_running or not self.server_process:
            self.log("No server process to stop", "WARNING")
            return

        try:
            self.log("Stopping server...")

            self.server_process.terminate()

            try:
                self.server_process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.server_process.kill()
                self.log("Server force-killed", "WARNING")

            self.server_process = None
            self.is_server_running = False
            self.update_ui_state()

            self.log("Server stopped", "SUCCESS")

        except Exception as e:
            self.log(f"Error stopping server: {e}", "ERROR")

    def restart_server(self):
        self.log("Restarting server...")
        self.stop_server()
        time.sleep(1)
        self.start_server()

    def monitor_output(self):
        if not self.server_process:
            return

        try:
            for line in iter(self.server_process.stdout.readline, ""):
                if line:
                    line = line.rstrip()
                    if line:
                        self.log(f"[SERVER] {line}")

                if self.server_process.poll() is not None:
                    break

        except Exception as e:
            self.log(f"Error monitoring output: {e}", "ERROR")

        if self.is_server_running:
            self.is_server_running = False
            self.server_process = None
            self.root.after(0, self.update_ui_state)
            self.log("Server process ended", "WARNING")

    def monitor_server(self):
        if self.is_server_running and self.server_process:
            if self.server_process.poll() is not None:
                self.is_server_running = False
                self.server_process = None
                self.update_ui_state()
                self.log("Server stopped unexpectedly", "ERROR")

        self.update_ui_state()

        self.root.after(2000, self.monitor_server)

    def update_ui_state(self):
        if self.is_server_running:
            self.server_status.set("🟢 Server Running")
            self.server_url.set(f"http://{self.host.get()}:{self.port.get()}")
            self.status_label.configure(style="Success.TLabel")

            self.start_button.configure(state="disabled")
            self.stop_button.configure(state="normal")
            self.restart_button.configure(state="normal")
        else:
            self.server_status.set("🔴 Server Stopped")
            self.server_url.set("")
            self.status_label.configure(style="Error.TLabel")

            self.start_button.configure(state="normal")
            self.stop_button.configure(state="disabled")
            self.restart_button.configure(state="disabled")

    def check_first_launch(self):
        try:
            conn = sqlite3.connect(self.db_path)
            cursor = conn.cursor()

            cursor.execute("SELECT COUNT(*) FROM config")
            config_count = cursor.fetchone()[0]

            cursor.execute("SELECT COUNT(*) FROM api_keys")
            api_count = cursor.fetchone()[0]

            conn.close()

            if config_count == 0 and api_count == 0:
                self.show_setup_wizard()
                return True

            return False

        except Exception as e:
            self.log(f"Error checking first launch: {e}", "WARNING")
            return False

    def show_setup_wizard(self):
        wizard = tk.Toplevel(self.root)
        wizard.title("OBSIDIAN-Neural - First Time Setup")
        wizard.geometry("650x800")
        wizard.transient(self.root)
        wizard.grab_set()
        wizard.resizable(True, True)

        wizard.update_idletasks()
        x = (wizard.winfo_screenwidth() // 2) - (650 // 2)
        y = (wizard.winfo_screenheight() // 2) - (800 // 2)
        wizard.geometry(f"650x800+{x}+{y}")

        wizard_model_path = tk.StringVar()
        wizard_api_key = tk.StringVar()
        wizard_environment = tk.StringVar(value="dev")
        wizard_host = tk.StringVar(value="127.0.0.1")
        wizard_port = tk.StringVar(value="8000")
        wizard_audio_model = tk.StringVar(value="stabilityai/stable-audio-open-1.0")

        main_frame = ttk.Frame(wizard, padding="30")
        main_frame.pack(fill="both", expand=True)

        header_label = ttk.Label(
            main_frame,
            text="🎵 Welcome to OBSIDIAN-Neural!",
            font=("Arial", 18, "bold"),
        )
        header_label.pack(pady=(0, 10))

        subtitle_label = ttk.Label(
            main_frame,
            text="Let's set up your AI music generation system",
            font=("Arial", 12),
        )
        subtitle_label.pack(pady=(0, 30))

        step1_frame = ttk.LabelFrame(
            main_frame, text="Step 1: Select Your AI Model", padding="15"
        )
        step1_frame.pack(fill="x", pady=(0, 15))

        ttk.Label(step1_frame, text="Choose your LLM model file (.gguf):").pack(
            anchor="w"
        )

        model_frame = ttk.Frame(step1_frame)
        model_frame.pack(fill="x", pady=(5, 0))

        model_entry = ttk.Entry(model_frame, textvariable=wizard_model_path, width=50)
        model_entry.pack(side="left", fill="x", expand=True)

        def browse_wizard_model():
            filename = filedialog.askopenfilename(
                title="Select LLM Model File",
                filetypes=[("GGUF files", "*.gguf"), ("All files", "*.*")],
                initialdir=".",
            )
            if filename:
                wizard_model_path.set(filename)

        ttk.Button(model_frame, text="Browse", command=browse_wizard_model).pack(
            side="right", padx=(5, 0)
        )

        hf_step_frame = ttk.LabelFrame(
            main_frame, text="Step 2: Hugging Face Access Token", padding="15"
        )
        hf_step_frame.pack(fill="x", pady=(0, 20))

        hf_info_frame = ttk.Frame(hf_step_frame)
        hf_info_frame.pack(fill="x", pady=(0, 10))

        ttk.Label(
            hf_info_frame,
            text="⚠️ Required for Stable Audio model download",
            foreground="orange",
            font=("Arial", 10, "bold"),
        ).pack(anchor="w")

        instructions_text = """1. Go to https://huggingface.co/stabilityai/stable-audio-open-1.0
        2. Click "Request access to this model" 
        3. Wait for approval (usually instant)
        4. Generate a token at https://huggingface.co/settings/tokens"""

        instructions_label = ttk.Label(
            hf_step_frame, text=instructions_text, font=("Arial", 9)
        )
        instructions_label.pack(anchor="w", pady=(0, 10))

        wizard_hf_token = tk.StringVar()
        token_frame = ttk.Frame(hf_step_frame)
        token_frame.pack(fill="x", pady=(5, 10))

        show_token = tk.BooleanVar(value=False)

        def update_token_display():
            if show_token.get():
                token_entry.config(show="")
                token_show_button.config(text="🙈 Hide")
            else:
                token_entry.config(show="*")
                token_show_button.config(text="👁 Show")

        ttk.Label(token_frame, text="HF Token:").pack(side="left")
        token_entry = ttk.Entry(
            token_frame,
            textvariable=wizard_hf_token,
            width=35,
            font=("Consolas", 10),
            show="*",
        )
        token_entry.pack(side="left", padx=(10, 5), fill="x", expand=True)

        token_show_button = ttk.Button(
            token_frame,
            text="👁 Show",
            width=8,
            command=lambda: [
                show_token.set(not show_token.get()),
                update_token_display(),
            ],
        )
        token_show_button.pack(side="left", padx=(0, 5))

        def verify_hf_token():
            token = wizard_hf_token.get().strip()
            if not token:
                messagebox.showerror("Error", "Please enter your Hugging Face token")
                return

            try:
                import requests

                headers = {"Authorization": f"Bearer {token}"}
                response = requests.get(
                    "https://huggingface.co/api/whoami", headers=headers, timeout=10
                )

                if response.status_code == 200:
                    user_info = response.json()
                    username = user_info.get("name", "Unknown")
                    messagebox.showinfo(
                        "Token Valid", f"✅ Token verified!\nUser: {username}"
                    )
                    return True
                else:
                    messagebox.showerror(
                        "Invalid Token", "❌ Token is invalid or expired"
                    )
                    return False

            except Exception as e:
                messagebox.showerror(
                    "Verification Error", f"Could not verify token:\n{e}"
                )
                return False

        ttk.Button(token_frame, text="🔍 Verify", command=verify_hf_token).pack(
            side="left"
        )

        step2_frame = ttk.LabelFrame(
            main_frame, text="Step 3: Create Your First API Key", padding="15"
        )
        step2_frame.pack(fill="x", pady=(0, 20))

        ttk.Label(
            step2_frame, text="Generate a secure API key for authentication:"
        ).pack(anchor="w")

        name_frame = ttk.Frame(step2_frame)
        name_frame.pack(fill="x", pady=(5, 10))

        ttk.Label(name_frame, text="Key Name:").pack(side="left")
        wizard_api_name = tk.StringVar(value="Main API Key")
        name_entry = ttk.Entry(name_frame, textvariable=wizard_api_name, width=25)
        name_entry.pack(side="left", padx=(10, 0))

        api_frame = ttk.Frame(step2_frame)
        api_frame.pack(fill="x", pady=(8, 0))

        show_key = tk.BooleanVar(value=False)

        def update_api_display():
            if show_key.get():
                api_entry.config(show="")
                show_button.config(text="🙈 Hide")
            else:
                api_entry.config(show="*")
                show_button.config(text="👁 Show")

        api_entry = ttk.Entry(
            api_frame,
            textvariable=wizard_api_key,
            width=35,
            font=("Consolas", 10),
            show="*",
        )
        api_entry.pack(side="left", fill="x", expand=True)

        show_button = ttk.Button(
            api_frame,
            text="👁 Show",
            width=8,
            command=lambda: [show_key.set(not show_key.get()), update_api_display()],
        )
        show_button.pack(side="left", padx=(5, 5))

        def generate_wizard_key():
            import secrets
            import string

            alphabet = string.ascii_letters + string.digits
            api_key = "".join(secrets.choice(alphabet) for _ in range(32))
            wizard_api_key.set(api_key)

        ttk.Button(api_frame, text="🔑 Generate", command=generate_wizard_key).pack(
            side="left"
        )

        step3_frame = ttk.LabelFrame(
            main_frame, text="Step 3: Server Configuration", padding="15"
        )
        step3_frame.pack(fill="x", pady=(0, 15))

        server_frame = ttk.Frame(step3_frame)
        server_frame.pack(fill="x", pady=(0, 10))

        ttk.Label(server_frame, text="Host:").pack(side="left")
        ttk.Entry(server_frame, textvariable=wizard_host, width=15).pack(
            side="left", padx=(5, 20)
        )

        ttk.Label(server_frame, text="Port:").pack(side="left")
        ttk.Entry(server_frame, textvariable=wizard_port, width=8).pack(
            side="left", padx=(5, 20)
        )

        ttk.Label(server_frame, text="Environment:").pack(side="left")
        env_combo = ttk.Combobox(
            server_frame,
            textvariable=wizard_environment,
            values=["dev", "prod"],
            width=8,
            state="readonly",
        )
        env_combo.pack(side="left", padx=(5, 0))

        audio_frame = ttk.Frame(step3_frame)
        audio_frame.pack(fill="x", pady=(10, 0))

        ttk.Label(audio_frame, text="Audio Model:").pack(anchor="w")
        audio_combo = ttk.Combobox(
            audio_frame,
            textvariable=wizard_audio_model,
            values=[
                "stabilityai/stable-audio-open-1.0",
                "stabilityai/stable-audio-open-small",
            ],
            state="readonly",
            width=40,
        )
        audio_combo.pack(anchor="w", pady=(5, 0))

        button_frame = ttk.Frame(main_frame)
        button_frame.pack(fill="x", pady=(20, 0))

        def validate_and_save():
            errors = []

            if not wizard_model_path.get():
                errors.append("Please select a model file")
            elif not Path(wizard_model_path.get()).exists():
                errors.append("Model file does not exist")

            if not wizard_hf_token.get().strip():
                errors.append("Please enter your Hugging Face token")
            elif len(wizard_hf_token.get().strip()) < 10:
                errors.append("Hugging Face token seems too short")

            if not wizard_api_name.get().strip():
                errors.append("Please enter a name for your API key")

            if not wizard_api_key.get():
                errors.append("Please generate an API key")
            elif len(wizard_api_key.get()) < 16:
                errors.append("API key must be at least 16 characters")

            try:
                port = int(wizard_port.get())
                if port < 1 or port > 65535:
                    errors.append("Port must be between 1 and 65535")
            except ValueError:
                errors.append("Port must be a valid number")

            if errors:
                messagebox.showerror("Configuration Error", "\n".join(errors))
                return

            try:
                self.model_path.set(wizard_model_path.get())
                self.host.set(wizard_host.get())
                self.port.set(wizard_port.get())
                self.environment.set(wizard_environment.get())
                self.audio_model.set(wizard_audio_model.get())

                conn = sqlite3.connect(self.db_path)
                cursor = conn.cursor()
                cursor.execute(
                    "INSERT OR REPLACE INTO config (key, value) VALUES (?, ?)",
                    ("hf_token", wizard_hf_token.get().strip()),
                )
                conn.commit()
                conn.close()

                if self.save_api_key(wizard_api_key.get(), wizard_api_name.get()):
                    self.api_keys_list.append(wizard_api_key.get())
                    self.update_api_keys_listbox()

                if self.save_config():
                    self.log("Initial configuration completed successfully!", "SUCCESS")

                    messagebox.showinfo(
                        "Setup Complete",
                        "🎉 Configuration saved successfully!\n\n"
                        "✅ Model configured\n"
                        "✅ Hugging Face token saved\n"
                        "✅ API key generated and saved\n"
                        "✅ Server settings configured\n\n"
                        "You can now start the OBSIDIAN-Neural server!",
                    )

                    wizard.destroy()
                else:
                    messagebox.showerror("Error", "Failed to save configuration")

            except Exception as e:
                messagebox.showerror("Error", f"Failed to save configuration:\n{e}")

        def skip_setup():
            if messagebox.askyesno(
                "Skip Setup",
                "Are you sure you want to skip the setup?\n\n"
                "You'll need to configure everything manually.",
            ):
                wizard.destroy()

        ttk.Button(
            button_frame, text="💾 Save Configuration", command=validate_and_save
        ).pack(side="right")
        ttk.Button(button_frame, text="Skip", command=skip_setup).pack(
            side="right", padx=(0, 10)
        )

        generate_wizard_key()

        wizard.protocol("WM_DELETE_WINDOW", skip_setup)
        wizard.focus_set()

    def open_project_folder(self):
        import os
        import platform

        try:
            if platform.system() == "Windows":
                os.startfile(".")
            elif platform.system() == "Darwin":
                subprocess.run(["open", "."])
            else:
                subprocess.run(["xdg-open", "."])
            self.log("Project folder opened")
        except Exception as e:
            self.log(f"Error opening folder: {e}", "ERROR")

    def show_system_info(self):
        try:
            import platform

            cpu_count = psutil.cpu_count(logical=False)
            cpu_threads = psutil.cpu_count(logical=True)
            memory = psutil.virtual_memory()
            disk = psutil.disk_usage(".")

            info = f"""System Information:
            
OS: {platform.system()} {platform.release()}
Python: {platform.python_version()}
CPU Cores: {cpu_count} ({cpu_threads} threads)
Memory: {memory.total // (1024**3)} GB total, {memory.available // (1024**3)} GB available
Disk Space: {disk.free // (1024**3)} GB free / {disk.total // (1024**3)} GB total

Project Directory: {Path.cwd()}
"""
            messagebox.showinfo("System Information", info)

        except Exception as e:
            messagebox.showerror("Error", f"Could not get system info: {e}")

    def clear_logs(self):
        self.log_text.delete(1.0, tk.END)
        self.log("Logs cleared")

    def save_logs(self):
        filename = filedialog.asksaveasfilename(
            defaultextension=".log",
            filetypes=[("Log files", "*.log"), ("Text files", "*.txt")],
            title="Save Logs",
        )

        if filename:
            try:
                with open(filename, "w", encoding="utf-8") as f:
                    f.write(self.log_text.get(1.0, tk.END))
                self.log(f"Logs saved to {filename}", "SUCCESS")
            except Exception as e:
                self.log(f"Error saving logs: {e}", "ERROR")

    def on_closing(self):
        if self.is_server_running:
            if messagebox.askokcancel("Quit", "Server is running. Stop it and quit?"):
                self.stop_server()
                time.sleep(1)
                self.root.quit()
        else:
            self.root.quit()

    def run(self):
        self.root.protocol("WM_DELETE_WINDOW", self.on_closing)
        self.log("OBSIDIAN-Neural Launcher started")
        self.log("Configure your settings and click 'Start Server' to begin")
        self.root.mainloop()


if __name__ == "__main__":
    app = ObsidianNeuralLauncher()
    app.run()
