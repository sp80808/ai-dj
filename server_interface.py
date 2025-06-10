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


class ObsidianNeuralLauncher:
    def __init__(self):
        self.root = tk.Tk()
        self.root.title("OBSIDIAN-Neural - Launcher & Control Panel")
        self.root.geometry("900x700")
        self.root.minsize(850, 650)
        self.root.resizable(True, True)
        self.api_keys_list = []
        # Center window
        self.root.update_idletasks()
        x = (self.root.winfo_screenwidth() // 2) - (900 // 2)
        y = (self.root.winfo_screenheight() // 2) - (700 // 2)
        self.root.geometry(f"900x700+{x}+{y}")

        # Server process
        self.server_process = None
        self.is_server_running = False

        # Initialize log_text to None first
        self.log_text = None

        self.init_database()
        # Configuration variables
        self.setup_variables()

        # Setup UI first (creates log_text)
        self.setup_ui()

        self.installation_dir = self.detect_installation_dir()
        if self.installation_dir != Path.cwd():
            os.chdir(self.installation_dir)
        self.load_config()
        self.load_api_keys()

        if self.check_first_launch():
            # After wizard is complete, reload everything
            self.load_config()
            self.load_api_keys()

        # Enable auto-save after loading
        self.enable_auto_save()
        # Start monitoring
        self.monitor_server()

    def handle_admin_install(self):
        """Handle admin installation with local config copy"""
        install_dir = self.detect_installation_dir()

        if str(install_dir).startswith("C:\\ProgramData"):
            self.log("Admin installation detected - using local config copy")

            # Paths
            admin_env = install_dir / ".env"
            local_env = Path(".env")

            # Copy admin .env to local if it doesn't exist
            if admin_env.exists() and not local_env.exists():
                try:
                    import shutil

                    shutil.copy2(admin_env, local_env)
                    self.log("Configuration copied to local directory")
                except Exception as e:
                    self.log(f"Could not copy config: {e}", "WARNING")

            # Show info to user
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
        """Get the correct .env path"""
        return self.handle_admin_install()

    def detect_installation_dir(self):
        """Detect the correct OBSIDIAN-Neural installation directory"""
        possible_paths = [
            Path("C:/ProgramData/OBSIDIAN-Neural"),  # Admin install
            Path.home() / "OBSIDIAN-Neural",  # User install
            Path.cwd(),  # Current directory
        ]

        for path in possible_paths:
            if (path / "main.py").exists():
                self.log(f"Found OBSIDIAN-Neural installation at: {path}")
                return path

        # Fallback to current directory
        self.log("Using current directory as installation path")
        return Path.cwd()

    def setup_variables(self):
        """Initialize configuration variables"""
        self.api_keys_list = []
        self.model_path = tk.StringVar(value="")
        self.environment = tk.StringVar(value="dev")
        self.host = tk.StringVar(value="127.0.0.1")
        self.port = tk.StringVar(value="8000")
        self.audio_model = tk.StringVar(value="stabilityai/stable-audio-open-1.0")

        # Status variables
        self.server_status = tk.StringVar(value="Server Stopped")
        self.server_url = tk.StringVar(value="")

        # Add auto-save on variable change (after UI is loaded)
        self.auto_save_enabled = False

    def enable_auto_save(self):
        """Enable auto-save when variables change"""
        if self.auto_save_enabled:
            return

        self.auto_save_enabled = True

        # Bind all config variables to auto-save
        variables = [
            self.model_path,
            self.environment,
            self.host,
            self.port,
            self.audio_model,
        ]

        for var in variables:
            var.trace_add("write", lambda *args: self.auto_save_config())

    def auto_save_config(self):
        """Auto-save configuration when variables change"""
        if hasattr(self, "db_path") and self.auto_save_enabled:
            # Delay save to avoid too many writes
            if hasattr(self, "_save_timer"):
                self.root.after_cancel(self._save_timer)
            self._save_timer = self.root.after(1000, self.save_config)

    def setup_ui(self):
        """Setup the user interface"""
        style = ttk.Style()
        if "clam" in style.theme_names():
            style.theme_use("clam")

        # Configure colors
        style.configure("Success.TLabel", foreground="dark green")
        style.configure("Warning.TLabel", foreground="orange")
        style.configure("Error.TLabel", foreground="red")
        style.configure("Running.TButton", background="light green")
        style.configure("Stopped.TButton", background="light coral")

        main_frame = ttk.Frame(self.root, padding="20")
        main_frame.pack(fill="both", expand=True)

        # Header
        self.create_header(main_frame)

        # Create notebook for tabs
        self.notebook = ttk.Notebook(main_frame)
        self.notebook.pack(fill="both", expand=True, pady=(20, 0))

        # Control tab
        self.create_control_tab()

        # Configuration tab
        self.create_config_tab()

        # Logs tab
        self.create_logs_tab()

    def create_header(self, parent):
        """Create header with logo and title"""
        header_frame = ttk.Frame(parent)
        header_frame.pack(fill="x", pady=(0, 20))

        # Try to load logo
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
        """Create title without logo"""
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
        """Create server control tab"""
        control_frame = ttk.Frame(self.notebook, padding="20")
        self.notebook.add(control_frame, text="🚀 Server Control")

        # Server status section
        status_frame = ttk.LabelFrame(control_frame, text="Server Status", padding="15")
        status_frame.pack(fill="x", pady=(0, 20))

        # Status display
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

        # Control buttons
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

        # Quick actions
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

    def create_config_tab(self):
        """Create configuration tab"""
        config_frame = ttk.Frame(self.notebook, padding="20")
        self.notebook.add(config_frame, text="⚙️ Configuration")

        # Create scrollable frame
        canvas = tk.Canvas(config_frame, highlightthickness=0)
        scrollbar = ttk.Scrollbar(config_frame, orient="vertical", command=canvas.yview)
        scrollable_frame = ttk.Frame(canvas)

        scrollable_frame.bind(
            "<Configure>", lambda e: canvas.configure(scrollregion=canvas.bbox("all"))
        )

        canvas.create_window((0, 0), window=scrollable_frame, anchor="nw")
        canvas.configure(yscrollcommand=scrollbar.set)

        canvas.pack(side="left", fill="both", expand=True)
        scrollbar.pack(side="right", fill="y")

        # API Configuration
        api_frame = ttk.LabelFrame(
            scrollable_frame, text="API Keys Management", padding="15"
        )
        api_frame.pack(fill="x", pady=(0, 15), padx=10)

        # API Keys list
        ttk.Label(api_frame, text="API Keys:").pack(anchor="w")

        # Frame for listbox and scrollbar
        list_frame = ttk.Frame(api_frame)
        list_frame.pack(fill="x", pady=(5, 10))

        # Listbox with scrollbar
        list_scroll = ttk.Scrollbar(list_frame, orient="vertical")
        self.api_keys_listbox = tk.Listbox(
            list_frame, height=4, yscrollcommand=list_scroll.set
        )
        list_scroll.config(command=self.api_keys_listbox.yview)

        self.api_keys_listbox.pack(side="left", fill="both", expand=True)
        list_scroll.pack(side="right", fill="y")

        # API Keys buttons
        api_buttons_frame = ttk.Frame(api_frame)
        api_buttons_frame.pack(fill="x", pady=(0, 10))

        ttk.Button(
            api_buttons_frame, text="🔑 Generate New Key", command=self.generate_api_key
        ).pack(side="left", padx=(0, 5))
        ttk.Button(
            api_buttons_frame, text="📋 Add Custom Key", command=self.add_custom_api_key
        ).pack(side="left", padx=(0, 5))
        ttk.Button(
            api_buttons_frame, text="🗑 Remove Selected", command=self.remove_api_key
        ).pack(side="left", padx=(0, 5))
        ttk.Button(
            api_buttons_frame, text="📋 Copy Selected", command=self.copy_api_key
        ).pack(side="left")

        # Model Configuration
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

        # Server Configuration
        server_frame = ttk.LabelFrame(
            scrollable_frame, text="Server Configuration", padding="15"
        )
        server_frame.pack(fill="x", pady=(0, 15), padx=10)

        # Host and Port
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

        # Save/Load buttons
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

        # Bind mousewheel to canvas
        def _on_mousewheel(event):
            canvas.yview_scroll(int(-1 * (event.delta / 120)), "units")

        canvas.bind_all("<MouseWheel>", _on_mousewheel)

    def manual_save_config(self):
        """Manual save configuration"""
        if self.save_config():
            messagebox.showinfo("Saved", "Configuration saved successfully!")

    def reload_config(self):
        """Reload configuration from database"""
        self.load_config()
        self.load_api_keys()
        messagebox.showinfo("Reloaded", "Configuration reloaded from database!")

    def clear_all_data(self):
        """Clear all data from database"""
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

                # Reset UI
                self.api_keys_list = []
                self.update_api_keys_listbox()
                self.reset_to_defaults()

                self.log("All data cleared from database", "SUCCESS")
                messagebox.showinfo("Cleared", "All data has been cleared!")

            except Exception as e:
                self.log(f"Error clearing data: {e}", "ERROR")
                messagebox.showerror("Error", f"Could not clear data: {e}")

    def init_database(self):
        """Initialize SQLite database for API keys storage"""
        # Store database in user directory
        self.db_path = Path.home() / ".obsidian_neural" / "config.db"
        self.db_path.parent.mkdir(exist_ok=True)

        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()

        # Create tables
        cursor.execute(
            """
            CREATE TABLE IF NOT EXISTS api_keys (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                key_value TEXT UNIQUE NOT NULL,
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
                value TEXT
            )
        """
        )

        conn.commit()
        conn.close()

        self.log(f"Database initialized at: {self.db_path}")

    def create_logs_tab(self):
        """Create logs tab"""
        logs_frame = ttk.Frame(self.notebook, padding="20")
        self.notebook.add(logs_frame, text="📝 Logs")

        # Log controls
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

        # Log display
        self.log_text = scrolledtext.ScrolledText(
            logs_frame, height=20, font=("Consolas", 9)
        )
        self.log_text.pack(fill="both", expand=True)

        # Configure log colors
        self.log_text.tag_configure("INFO", foreground="black")
        self.log_text.tag_configure("SUCCESS", foreground="dark green")
        self.log_text.tag_configure("WARNING", foreground="orange")
        self.log_text.tag_configure("ERROR", foreground="red")

    def log(self, message, level="INFO"):
        """Add message to log with timestamp"""
        timestamp = time.strftime("%H:%M:%S")
        emoji = {"INFO": "ℹ️ ", "SUCCESS": "✅", "WARNING": "⚠️", "ERROR": "❌"}.get(
            level, "ℹ️"
        )

        formatted_msg = f"[{timestamp}] {emoji} {message}\n"

        if self.log_text is not None:
            self.log_text.insert(tk.END, formatted_msg, level)

            if hasattr(self, "auto_scroll") and self.auto_scroll.get():
                self.log_text.see(tk.END)

            self.root.update_idletasks()
        else:
            # Fallback to console if UI not ready
            print(f"{formatted_msg.strip()}")

    def load_api_keys(self):
        """Load API keys from database"""
        try:
            conn = sqlite3.connect(self.db_path)
            cursor = conn.cursor()

            cursor.execute("SELECT key_value FROM api_keys ORDER BY created_at")
            rows = cursor.fetchall()

            self.api_keys_list = [row[0] for row in rows]
            self.update_api_keys_listbox()

            conn.close()

            if self.api_keys_list:
                self.log(f"Loaded {len(self.api_keys_list)} API key(s) from database")

        except Exception as e:
            self.log(f"Error loading API keys: {e}", "WARNING")
            self.api_keys_list = []

    def save_api_key(self, api_key, name=None):
        """Save API key to database"""
        try:
            conn = sqlite3.connect(self.db_path)
            cursor = conn.cursor()

            cursor.execute(
                "INSERT OR IGNORE INTO api_keys (key_value, name) VALUES (?, ?)",
                (api_key, name),
            )

            conn.commit()
            conn.close()

            return True
        except Exception as e:
            self.log(f"Error saving API key: {e}", "ERROR")
            return False

    def remove_api_key_from_db(self, api_key):
        """Remove API key from database"""
        try:
            conn = sqlite3.connect(self.db_path)
            cursor = conn.cursor()

            cursor.execute("DELETE FROM api_keys WHERE key_value = ?", (api_key,))

            conn.commit()
            conn.close()

            return True
        except Exception as e:
            self.log(f"Error removing API key: {e}", "ERROR")
            return False

    def save_config(self):
        """Save current configuration to database"""
        try:
            conn = sqlite3.connect(self.db_path)
            cursor = conn.cursor()

            config_items = [
                ("model_path", self.model_path.get()),
                ("environment", self.environment.get()),
                ("host", self.host.get()),
                ("port", self.port.get()),
                ("audio_model", self.audio_model.get()),
            ]

            for key, value in config_items:
                cursor.execute(
                    "INSERT OR REPLACE INTO config (key, value) VALUES (?, ?)",
                    (key, value),
                )

            conn.commit()
            conn.close()

            return True
        except Exception as e:
            self.log(f"Error saving configuration: {e}", "ERROR")
            return False

    def load_config(self):
        """Load configuration from database with fallback to defaults"""
        try:
            conn = sqlite3.connect(self.db_path)
            cursor = conn.cursor()

            cursor.execute("SELECT key, value FROM config")
            rows = cursor.fetchall()

            config_dict = dict(rows)

            # Load with fallback to current values (defaults)
            self.model_path.set(config_dict.get("model_path", self.model_path.get()))
            self.environment.set(config_dict.get("environment", self.environment.get()))
            self.host.set(config_dict.get("host", self.host.get()))
            self.port.set(config_dict.get("port", self.port.get()))
            self.audio_model.set(config_dict.get("audio_model", self.audio_model.get()))

            conn.close()

            if config_dict:
                self.log(
                    f"Configuration loaded from database ({len(config_dict)} settings)"
                )

        except Exception as e:
            self.log(f"Error loading configuration: {e}", "WARNING")

    def generate_api_key(self):
        """Generate a new random API key"""
        import secrets
        import string

        # Generate a secure random API key
        alphabet = string.ascii_letters + string.digits
        api_key = "".join(secrets.choice(alphabet) for _ in range(32))

        # Save to database FIRST
        if self.save_api_key(api_key):
            self.api_keys_list.append(api_key)
            self.update_api_keys_listbox()

            # Show key to user
            self.show_api_key_dialog(api_key, "Generated API Key")
            self.log(f"New API key generated and saved: {api_key[:8]}...", "SUCCESS")
        else:
            messagebox.showerror("Error", "Could not save API key to database")

    def add_custom_api_key(self):
        """Add a custom API key"""
        dialog = tk.Toplevel(self.root)
        dialog.title("Add Custom API Key")
        dialog.geometry("400x150")
        dialog.transient(self.root)
        dialog.grab_set()

        # Center dialog
        dialog.update_idletasks()
        x = (dialog.winfo_screenwidth() // 2) - (400 // 2)
        y = (dialog.winfo_screenheight() // 2) - (150 // 2)
        dialog.geometry(f"400x150+{x}+{y}")

        frame = ttk.Frame(dialog, padding="20")
        frame.pack(fill="both", expand=True)

        ttk.Label(frame, text="Enter API Key:").pack(anchor="w")

        key_var = tk.StringVar()
        entry = ttk.Entry(frame, textvariable=key_var, width=50)
        entry.pack(fill="x", pady=(5, 15))
        entry.focus()

        button_frame = ttk.Frame(frame)
        button_frame.pack(fill="x")

        def add_key():
            key = key_var.get().strip()
            if key:
                if key not in self.api_keys_list:
                    # Save to database FIRST
                    if self.save_api_key(key):
                        self.api_keys_list.append(key)
                        self.update_api_keys_listbox()
                        self.log(f"Custom API key added: {key[:8]}...", "SUCCESS")
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

        # Bind Enter key
        entry.bind("<Return>", lambda e: add_key())

    def remove_api_key(self):
        """Remove selected API key"""
        selection = self.api_keys_listbox.curselection()
        if selection:
            index = selection[0]
            api_key = self.api_keys_list[index]

            # Remove from database FIRST
            if self.remove_api_key_from_db(api_key):
                self.api_keys_list.pop(index)
                self.update_api_keys_listbox()
                self.log(f"API key removed: {api_key[:8]}...", "SUCCESS")
            else:
                messagebox.showerror("Error", "Could not remove API key from database")
        else:
            messagebox.showwarning("No Selection", "Please select an API key to remove")

    def copy_api_key(self):
        """Copy selected API key to clipboard"""
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
        """Show API key in a dialog with copy functionality"""
        dialog = tk.Toplevel(self.root)
        dialog.title(title)
        dialog.geometry("500x250")
        dialog.transient(self.root)
        dialog.grab_set()

        # Center dialog
        dialog.update_idletasks()
        x = (dialog.winfo_screenwidth() // 2) - (500 // 2)
        y = (dialog.winfo_screenheight() // 2) - (250 // 2)
        dialog.geometry(f"500x250+{x}+{y}")

        frame = ttk.Frame(dialog, padding="20")
        frame.pack(fill="both", expand=True)

        ttk.Label(frame, text="Your API Key:", font=("Arial", 12, "bold")).pack(
            anchor="w"
        )

        # Warning message
        warning_label = ttk.Label(
            frame,
            text="⚠️ Save this key securely - it won't be shown again in full!",
            foreground="orange",
            font=("Arial", 10),
        )
        warning_label.pack(anchor="w", pady=(5, 10))

        # API key display - use Entry for easy selection
        key_frame = ttk.Frame(frame)
        key_frame.pack(fill="x", pady=(0, 15))

        key_var = tk.StringVar(value=api_key)
        key_entry = ttk.Entry(
            key_frame,
            textvariable=key_var,
            font=("Consolas", 11),
            state="readonly",
            width=40,
        )
        key_entry.pack(fill="x")

        # Select all text when dialog opens
        def select_all():
            key_entry.select_range(0, tk.END)
            key_entry.focus()

        dialog.after(100, select_all)

        button_frame = ttk.Frame(frame)
        button_frame.pack(fill="x")

        def copy_and_close():
            self.root.clipboard_clear()
            self.root.clipboard_append(api_key)
            self.log("API key copied to clipboard", "SUCCESS")
            dialog.destroy()

        def select_all_key():
            key_entry.select_range(0, tk.END)

        ttk.Button(
            button_frame, text="📋 Copy to Clipboard", command=copy_and_close
        ).pack(side="left", padx=(0, 10))
        ttk.Button(button_frame, text="🔍 Select All", command=select_all_key).pack(
            side="left", padx=(0, 10)
        )
        ttk.Button(button_frame, text="Close", command=dialog.destroy).pack(side="left")

    def update_api_keys_listbox(self):
        """Update the API keys listbox display"""
        self.api_keys_listbox.delete(0, tk.END)
        for i, key in enumerate(self.api_keys_list):
            # Show only first 8 chars + ... for security
            display_key = f"{i+1}. {key[:8]}...{key[-4:]}"
            self.api_keys_listbox.insert(tk.END, display_key)

    def browse_model_path(self):
        """Browse for model file"""
        filename = filedialog.askopenfilename(
            title="Select LLM Model File",
            filetypes=[("GGUF files", "*.gguf"), ("All files", "*.*")],
            initialdir=self.model_path.get() if self.model_path.get() else ".",
        )
        if filename:
            self.model_path.set(filename)

    def reset_to_defaults(self):
        """Reset configuration to defaults"""
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
        """Start the OBSIDIAN-Neural server using command line flags"""
        if self.is_server_running:
            self.log("Server is already running", "WARNING")
            return

        # Validate required fields
        if not self.model_path.get() or not Path(self.model_path.get()).exists():
            messagebox.showerror("Error", "Please specify a valid model path")
            return

        try:
            install_dir = self.detect_installation_dir()
            main_py = install_dir / "main.py"

            if not main_py.exists():
                messagebox.showerror("Error", f"main.py not found at {install_dir}")
                return

            # Find Python executable
            python_exe = install_dir / "env" / "Scripts" / "python.exe"
            if not python_exe.exists():
                python_exe = sys.executable
                self.log("Using system Python (venv not found)", "WARNING")

            self.log(f"Using Python: {python_exe}")
            self.log(f"Starting server from: {install_dir}")

            # Build command with all parameters as flags
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

            # Add API keys if provided
            if self.api_keys_list:
                api_keys_string = ",".join(self.api_keys_list)
                cmd.extend(["--api-keys", api_keys_string])
                self.log(f"Using {len(self.api_keys_list)} API key(s)")

            self.log(f"Command: {' '.join(cmd[:8])}... [API keys hidden]")

            # Set environment with UTF-8 encoding
            env = os.environ.copy()
            env["PYTHONIOENCODING"] = "utf-8"

            self.server_process = subprocess.Popen(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                universal_newlines=True,
                bufsize=1,
                cwd=str(install_dir),
                env=env,  # Add the environment with UTF-8 encoding
                encoding="utf-8",  # Force UTF-8 encoding
                errors="replace",  # Replace invalid characters instead of crashing
            )

            self.is_server_running = True
            self.update_ui_state()

            # Start output monitoring
            threading.Thread(target=self.monitor_output, daemon=True).start()

            self.log("Server started successfully", "SUCCESS")

        except Exception as e:
            self.log(f"Failed to start server: {e}", "ERROR")
            messagebox.showerror("Error", f"Failed to start server:\n{e}")

    def stop_server(self):
        """Stop the server"""
        if not self.is_server_running or not self.server_process:
            self.log("No server process to stop", "WARNING")
            return

        try:
            self.log("Stopping server...")

            # Try graceful shutdown first
            self.server_process.terminate()

            # Wait a few seconds for graceful shutdown
            try:
                self.server_process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                # Force kill if needed
                self.server_process.kill()
                self.log("Server force-killed", "WARNING")

            self.server_process = None
            self.is_server_running = False
            self.update_ui_state()

            self.log("Server stopped", "SUCCESS")

        except Exception as e:
            self.log(f"Error stopping server: {e}", "ERROR")

    def restart_server(self):
        """Restart the server"""
        self.log("Restarting server...")
        self.stop_server()
        time.sleep(1)  # Brief pause
        self.start_server()

    def monitor_output(self):
        """Monitor server output in separate thread"""
        if not self.server_process:
            return

        try:
            for line in iter(self.server_process.stdout.readline, ""):
                if line:
                    # Remove trailing newline and log
                    line = line.rstrip()
                    if line:
                        self.log(f"[SERVER] {line}")

                # Check if process is still running
                if self.server_process.poll() is not None:
                    break

        except Exception as e:
            self.log(f"Error monitoring output: {e}", "ERROR")

        # Process ended
        if self.is_server_running:
            self.is_server_running = False
            self.server_process = None
            self.root.after(0, self.update_ui_state)
            self.log("Server process ended", "WARNING")

    def monitor_server(self):
        """Monitor server status periodically"""
        if self.is_server_running and self.server_process:
            if self.server_process.poll() is not None:
                # Process ended unexpectedly
                self.is_server_running = False
                self.server_process = None
                self.update_ui_state()
                self.log("Server stopped unexpectedly", "ERROR")

        self.update_ui_state()

        # Schedule next check
        self.root.after(2000, self.monitor_server)

    def update_ui_state(self):
        """Update UI based on server state"""
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
        """Check if this is the first launch and show setup wizard"""
        try:
            conn = sqlite3.connect(self.db_path)
            cursor = conn.cursor()

            # Check if we have any configuration
            cursor.execute("SELECT COUNT(*) FROM config")
            config_count = cursor.fetchone()[0]

            cursor.execute("SELECT COUNT(*) FROM api_keys")
            api_count = cursor.fetchone()[0]

            conn.close()

            # First launch if no config AND no API keys
            if config_count == 0 and api_count == 0:
                self.show_setup_wizard()
                return True

            return False

        except Exception as e:
            self.log(f"Error checking first launch: {e}", "WARNING")
            return False

    def show_setup_wizard(self):
        """Show setup wizard for first-time users"""
        wizard = tk.Toplevel(self.root)
        wizard.title("OBSIDIAN-Neural - First Time Setup")
        wizard.geometry("650x800")  # Un peu plus haut
        wizard.transient(self.root)
        wizard.grab_set()
        wizard.resizable(True, True)

        # Center dialog
        wizard.update_idletasks()
        x = (wizard.winfo_screenwidth() // 2) - (650 // 2)
        y = (wizard.winfo_screenheight() // 2) - (800 // 2)
        wizard.geometry(f"650x800+{x}+{y}")

        # Variables for wizard
        wizard_model_path = tk.StringVar()
        wizard_api_key = tk.StringVar()
        wizard_environment = tk.StringVar(value="dev")
        wizard_host = tk.StringVar(value="127.0.0.1")
        wizard_port = tk.StringVar(value="8000")
        wizard_audio_model = tk.StringVar(value="stabilityai/stable-audio-open-1.0")

        # Main frame
        main_frame = ttk.Frame(wizard, padding="30")
        main_frame.pack(fill="both", expand=True)

        # Header
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

        # Step 1: Model Path
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

        # Step 2: API Key
        step2_frame = ttk.LabelFrame(
            main_frame, text="Step 2: Create Your First API Key", padding="15"
        )
        step2_frame.pack(fill="x", pady=(0, 15))

        ttk.Label(
            step2_frame, text="Generate a secure API key for authentication:"
        ).pack(anchor="w")

        api_frame = ttk.Frame(step2_frame)
        api_frame.pack(fill="x", pady=(5, 10))

        # API key with show/hide functionality
        api_show_frame = ttk.Frame(api_frame)
        api_show_frame.pack(fill="x")

        show_key = tk.BooleanVar(value=False)

        def update_api_display():
            if show_key.get():
                api_entry.config(show="")
                show_button.config(text="🙈 Hide")
            else:
                api_entry.config(show="*")
                show_button.config(text="👁 Show")

        api_entry = ttk.Entry(
            api_show_frame,
            textvariable=wizard_api_key,
            width=35,
            font=("Consolas", 10),
            show="*",
        )
        api_entry.pack(side="left", fill="x", expand=True)

        show_button = ttk.Button(
            api_show_frame,
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

        ttk.Button(
            api_show_frame, text="🔑 Generate", command=generate_wizard_key
        ).pack(side="left")

        # Step 3: Server Configuration
        step3_frame = ttk.LabelFrame(
            main_frame, text="Step 3: Server Configuration", padding="15"
        )
        step3_frame.pack(fill="x", pady=(0, 15))

        # Host and Port
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

        # Audio Model
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

        # Buttons
        button_frame = ttk.Frame(main_frame)
        button_frame.pack(fill="x", pady=(20, 0))

        def validate_and_save():
            # Validate inputs
            errors = []

            if not wizard_model_path.get():
                errors.append("Please select a model file")
            elif not Path(wizard_model_path.get()).exists():
                errors.append("Model file does not exist")

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

            # Save configuration
            try:
                # Set the main configuration
                self.model_path.set(wizard_model_path.get())
                self.host.set(wizard_host.get())
                self.port.set(wizard_port.get())
                self.environment.set(wizard_environment.get())
                self.audio_model.set(wizard_audio_model.get())

                # Save API key
                if self.save_api_key(wizard_api_key.get()):
                    self.api_keys_list.append(wizard_api_key.get())
                    self.update_api_keys_listbox()

                # Save configuration
                if self.save_config():
                    self.log("Initial configuration completed successfully!", "SUCCESS")

                    # Show success message
                    messagebox.showinfo(
                        "Setup Complete",
                        "🎉 Configuration saved successfully!\n\n"
                        "✅ Model configured\n"
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

        # Auto-generate API key on load
        generate_wizard_key()

        # Make wizard modal
        wizard.protocol("WM_DELETE_WINDOW", skip_setup)
        wizard.focus_set()

    def open_project_folder(self):
        """Open project folder in file explorer"""
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
        """Show system information dialog"""
        try:
            import platform

            # Get system info
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
        """Clear the log display"""
        self.log_text.delete(1.0, tk.END)
        self.log("Logs cleared")

    def save_logs(self):
        """Save logs to file"""
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
        """Handle window closing"""
        if self.is_server_running:
            if messagebox.askokcancel("Quit", "Server is running. Stop it and quit?"):
                self.stop_server()
                time.sleep(1)
                self.root.quit()
        else:
            self.root.quit()

    def run(self):
        """Start the application"""
        self.root.protocol("WM_DELETE_WINDOW", self.on_closing)
        self.log("OBSIDIAN-Neural Launcher started")
        self.log("Configure your settings and click 'Start Server' to begin")
        self.root.mainloop()


if __name__ == "__main__":
    app = ObsidianNeuralLauncher()
    app.run()
