#!/usr/bin/env python3

import re
import subprocess
import sys
import os
import platform
import ctypes
import tkinter as tk
from tkinter import ttk, messagebox, scrolledtext, filedialog
import threading
import shutil
import urllib.request
from pathlib import Path
import time
import psutil
import GPUtil

try:
    from PIL import Image, ImageTk

    PIL_AVAILABLE = True
except ImportError:
    PIL_AVAILABLE = False
    print("PIL not installed - logo disabled")


class ObsidianNeuralInstaller:
    def __init__(self):
        self.root = tk.Tk()
        self.root.title("OBSIDIAN-Neural - Full Installer")
        self.root.geometry("850x750")
        self.root.minsize(800, 600)
        self.root.resizable(True, True)

        self.root.update_idletasks()
        x = (self.root.winfo_screenwidth() // 2) - (850 // 2)
        y = (self.root.winfo_screenheight() // 2) - (750 // 2)
        self.root.geometry(f"850x750+{x}+{y}")

        self.is_admin = self.check_admin()
        if self.is_admin:
            default_install_path = "C:/ProgramData/OBSIDIAN-Neural"
        else:
            default_install_path = str(Path.home() / "OBSIDIAN-Neural")

        self.install_path = tk.StringVar(value=default_install_path)
        detected_vst_path_val = self._do_detect_vst_folder()
        self.vst_path = tk.StringVar(value=detected_vst_path_val)
        self.progress_var = tk.DoubleVar()
        self.status_var = tk.StringVar(value="Ready to install")

        self.install_python = tk.BooleanVar(value=not self.check_python())
        self.install_cmake = tk.BooleanVar(value=not self.check_cmake())
        self.install_git = tk.BooleanVar(value=not self.check_git())
        self.install_buildtools = tk.BooleanVar(value=not self.check_buildtools())
        self.install_cuda = tk.BooleanVar(value=False)
        self.run_benchmark = tk.BooleanVar(value=True)
        self.auto_copy_vst = tk.BooleanVar(value=True)
        self.start_after_install = tk.BooleanVar(value=False)

        self.system_info = self.get_system_info()
        self.setup_ui()
        self.log_system_info()

    def check_admin(self):
        if platform.system() == "Windows":
            try:
                return ctypes.windll.shell32.IsUserAnAdmin() != 0
            except:
                return False
        else:
            return os.geteuid() == 0

    def request_admin(self):
        if platform.system() == "Windows":
            if not self.is_admin:
                ctypes.windll.shell32.ShellExecuteW(
                    None, "runas", sys.executable, " ".join(sys.argv), None, 1
                )
                sys.exit()

    def _do_detect_vst_folder(
        self,
    ):
        print("Detecting VST3 folder (initial scan, output to console):")
        potential_paths = []

        if platform.system() == "Windows":
            program_files_vst3 = (
                Path(os.environ.get("PROGRAMFILES", "C:/Program Files"))
                / "Common Files"
                / "VST3"
            )
            program_files_x86_vst3 = (
                Path(os.environ.get("PROGRAMFILES(X86)", "C:/Program Files (x86)"))
                / "Common Files"
                / "VST3"
            )
            appdata_vst3 = Path.home() / "AppData/Roaming/VST3"
            home_dot_vst3 = Path.home() / ".vst3"

            if self.is_admin:
                potential_paths.extend(
                    [
                        program_files_vst3,
                        program_files_x86_vst3,
                        appdata_vst3,
                        home_dot_vst3,
                    ]
                )
            else:
                potential_paths.extend(
                    [
                        appdata_vst3,
                        home_dot_vst3,
                        program_files_vst3,
                        program_files_x86_vst3,
                    ]
                )

            custom_paths_to_check = [
                Path("C:/VSTPlugins/VST3"),
                Path("C:/VST3Plugins"),
                Path("D:/VSTPlugins/VST3"),
                Path("D:/VST3Plugins"),
            ]
            for cp in custom_paths_to_check:
                if cp.exists() and cp.is_dir():
                    if cp not in potential_paths:
                        potential_paths.append(cp)

        elif platform.system() == "Darwin":
            potential_paths = [
                Path("/Library/Audio/Plug-Ins/VST3"),
                Path.home() / "Library/Audio/Plug-Ins/VST3",
            ]
        else:
            potential_paths = [
                Path.home() / ".vst3",
                Path("/usr/lib/vst3"),
                Path("/usr/local/lib/vst3"),
            ]

        unique_ordered_paths = []
        seen_str_paths = set()
        for p in potential_paths:
            try:
                p_resolved = p.resolve()
                p_str = str(p_resolved)
                if p_str not in seen_str_paths:
                    unique_ordered_paths.append(p)
                    seen_str_paths.add(p_str)
            except Exception:
                if str(p) not in seen_str_paths:
                    unique_ordered_paths.append(p)
                    seen_str_paths.add(str(p))

        for path_to_check in unique_ordered_paths:
            exists = path_to_check.exists()
            is_dir = path_to_check.is_dir() if exists else False
            print(
                f"  Checking: {str(path_to_check)} (Exists: {exists}, IsDir: {is_dir})"
            )
            if exists and is_dir:
                print(f"  Found existing VST3 folder: {str(path_to_check)}")
                return str(path_to_check)

        default_vst_path_str = ""
        if platform.system() == "Windows":
            default_vst_path_str = str(
                program_files_vst3 if self.is_admin else appdata_vst3
            )
        elif platform.system() == "Darwin":
            default_vst_path_str = str(Path("/Library/Audio/Plug-Ins/VST3"))
        else:
            default_vst_path_str = str(Path.home() / ".vst3")

        print(
            f"  No suitable existing VST3 folder found. Defaulting to: {default_vst_path_str} (will be created if necessary)."
        )
        return default_vst_path_str

    def detect_vst_folder(self):
        if platform.system() == "Windows":
            common_paths = [
                Path.home() / "AppData/Roaming/VST3",
                Path("C:/Program Files/Common Files/VST3"),
                Path("C:/Program Files (x86)/Common Files/VST3"),
            ]
        elif platform.system() == "Darwin":
            common_paths = [
                Path.home() / "Library/Audio/Plug-Ins/VST3",
                Path("/Library/Audio/Plug-Ins/VST3"),
                Path("/System/Library/Audio/Plug-Ins/VST3"),
            ]
        else:
            common_paths = [
                Path.home() / ".vst3",
                Path("/usr/lib/vst3"),
                Path("/usr/local/lib/vst3"),
            ]

        for path in common_paths:
            if path.exists():
                return str(path)

        return str(
            Path.home() / "VST3"
            if platform.system() != "Windows"
            else Path.home() / "AppData/Roaming/VST3"
        )

    def check_python(self):
        try:
            result = subprocess.run(
                [sys.executable, "--version"], capture_output=True, text=True
            )
            if result.returncode == 0:
                version = result.stdout.strip().split()[1]
                major, minor = map(int, version.split(".")[:2])
                return major >= 3 and minor >= 10
        except:
            pass
        return False

    def check_cmake(self):
        try:
            result = subprocess.run(
                ["cmake", "--version"], capture_output=True, text=True
            )
            return result.returncode == 0
        except:
            return False

    def check_git(self):
        try:
            result = subprocess.run(
                ["git", "--version"], capture_output=True, text=True
            )
            return result.returncode == 0
        except:
            return False

    def check_buildtools(self):
        if platform.system() != "Windows":
            return True

        try:
            vs_paths = [
                "C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools",
                "C:/Program Files/Microsoft Visual Studio/2022/BuildTools",
                "C:/Program Files/Microsoft Visual Studio/2022/Community",
                "C:/Program Files/Microsoft Visual Studio/2022/Professional",
                "C:/Program Files/Microsoft Visual Studio/2022/Enterprise",
                "C:/Program Files (x86)/Microsoft Visual Studio/2019/BuildTools",
                "C:/Program Files/Microsoft Visual Studio/2019/BuildTools",
                "C:/Program Files (x86)/Microsoft Visual Studio/2019/Community",
                "C:/Program Files (x86)/Microsoft Visual Studio/2019/Professional",
                "C:/Program Files (x86)/Microsoft Visual Studio/2019/Enterprise",
                "C:/Program Files (x86)/Microsoft Visual Studio/2017/BuildTools",
                "C:/Program Files (x86)/Microsoft Visual Studio/2017/Community",
                "C:/Program Files (x86)/Microsoft Visual Studio/2017/Professional",
                "C:/Program Files (x86)/Microsoft Visual Studio/2017/Enterprise",
            ]

            for path in vs_paths:
                if Path(path).exists():
                    msbuild_paths = [
                        Path(path) / "MSBuild/Current/Bin/MSBuild.exe",
                        Path(path) / "MSBuild/Current/Bin/amd64/MSBuild.exe",
                        Path(path) / "VC/Tools",
                    ]
                    if any(msbuild_path.exists() for msbuild_path in msbuild_paths):
                        return True

            return False

        except Exception:
            return False

    def get_system_info(self):
        info = {
            "os": f"{platform.system()} {platform.release()}",
            "cpu": platform.processor(),
            "cpu_cores": psutil.cpu_count(logical=False),
            "cpu_threads": psutil.cpu_count(logical=True),
            "ram_gb": round(psutil.virtual_memory().total / (1024**3), 1),
            "gpu": "None",
        }

        info["cuda_available"] = False
        info["rocm_available"] = False
        info["gpu_type"] = "cpu"
        info["gpu_list"] = []
        info["recommended_install"] = "cpu"

        try:
            gpus = GPUtil.getGPUs()
            if gpus:
                gpu = gpus[0]
                info["gpu"] = f"{gpu.name} ({gpu.memoryTotal}MB)"
                info["cuda_available"] = True
                info["gpu_type"] = "nvidia"
                info["recommended_install"] = "cuda"
                info["gpu_list"].append(
                    {"name": gpu.name, "memory_mb": gpu.memoryTotal, "type": "nvidia"}
                )
        except Exception as e:
            pass

        if not info["cuda_available"]:
            amd_gpus = []
            try:
                result = subprocess.run(
                    ["rocm-smi", "--showproductname"],
                    capture_output=True,
                    text=True,
                    timeout=5,
                )

                if result.returncode == 0:
                    lines = result.stdout.strip().split("\n")
                    for line in lines:
                        if "Card series:" in line or "GPU" in line:
                            gpu_name = line.split(":")[-1].strip()
                            if gpu_name and gpu_name != "N/A":
                                amd_gpus.append(
                                    {
                                        "name": gpu_name,
                                        "memory_mb": 0,  #
                                        "type": "amd",
                                        "detected_via": "rocm-smi",
                                    }
                                )
                                break

            except (
                subprocess.TimeoutExpired,
                subprocess.CalledProcessError,
                FileNotFoundError,
            ):
                pass

            if not amd_gpus:
                try:
                    if platform.system() == "Windows":
                        result = subprocess.run(
                            [
                                "wmic",
                                "path",
                                "win32_VideoController",
                                "get",
                                "name,AdapterRAM",
                                "/format:csv",
                            ],
                            capture_output=True,
                            text=True,
                            timeout=10,
                        )

                        if result.returncode == 0:
                            lines = result.stdout.split("\n")
                            for line in lines:
                                if any(
                                    amd_keyword in line.lower()
                                    for amd_keyword in [
                                        "amd",
                                        "radeon",
                                        "rx ",
                                        "vega",
                                        "navi",
                                    ]
                                ):
                                    parts = line.split(",")
                                    if len(parts) >= 2:
                                        gpu_name = (
                                            parts[2].strip()
                                            if len(parts) > 2
                                            else "AMD GPU"
                                        )
                                        memory_bytes = (
                                            parts[1].strip() if len(parts) > 1 else "0"
                                        )

                                        try:
                                            memory_mb = (
                                                int(memory_bytes) // (1024 * 1024)
                                                if memory_bytes.isdigit()
                                                else 0
                                            )
                                        except:
                                            memory_mb = 0

                                        amd_gpus.append(
                                            {
                                                "name": gpu_name,
                                                "memory_mb": memory_mb,
                                                "type": "amd",
                                                "detected_via": "wmic",
                                            }
                                        )
                                        break

                    else:
                        result = subprocess.run(
                            ["lspci"], capture_output=True, text=True, timeout=5
                        )
                        if result.returncode == 0:
                            for line in result.stdout.split("\n"):
                                if re.search(
                                    r"VGA.*AMD|VGA.*ATI|VGA.*Radeon",
                                    line,
                                    re.IGNORECASE,
                                ):
                                    match = re.search(r":\s*(.+)", line)
                                    if match:
                                        gpu_name = match.group(1).strip()
                                        amd_gpus.append(
                                            {
                                                "name": gpu_name,
                                                "memory_mb": 0,
                                                "type": "amd",
                                                "detected_via": "lspci",
                                            }
                                        )
                                        break

                except (
                    subprocess.TimeoutExpired,
                    subprocess.CalledProcessError,
                    FileNotFoundError,
                ):
                    pass

            if amd_gpus:
                info["gpu_type"] = "amd"
                info["gpu_list"] = amd_gpus
                info["gpu"] = f"{amd_gpus[0]['name']}"
                if amd_gpus[0]["memory_mb"] > 0:
                    info["gpu"] += f" ({amd_gpus[0]['memory_mb']}MB)"

                rocm_detected = False
                rocm_paths = [
                    "/opt/rocm",
                    "/usr/lib/x86_64-linux-gnu/rocm",
                    Path.home() / ".local/rocm",
                ]

                for path in rocm_paths:
                    if Path(path).exists():
                        rocm_detected = True
                        break

                if not rocm_detected:
                    try:
                        result = subprocess.run(
                            ["rocm-smi", "--version"],
                            capture_output=True,
                            text=True,
                            timeout=5,
                        )
                        if result.returncode == 0:
                            rocm_detected = True
                    except:
                        pass

                if not rocm_detected:
                    rocm_env_vars = ["ROCM_PATH", "HIP_PATH", "ROCM_HOME"]
                    for var in rocm_env_vars:
                        if os.environ.get(var):
                            rocm_detected = True
                            break

                info["rocm_available"] = rocm_detected

        if info["gpu_type"] == "cpu":
            try:
                if platform.system() == "Windows":
                    result = subprocess.run(
                        ["wmic", "path", "win32_VideoController", "get", "name"],
                        capture_output=True,
                        text=True,
                        timeout=5,
                    )

                    if result.returncode == 0:
                        for line in result.stdout.split("\n"):
                            if "intel" in line.lower() and "arc" in line.lower():
                                info["gpu"] = line.strip()
                                info["gpu_type"] = "intel"
                                break
                else:
                    result = subprocess.run(
                        ["lspci"], capture_output=True, text=True, timeout=5
                    )
                    if result.returncode == 0:
                        for line in result.stdout.split("\n"):
                            if re.search(r"VGA.*Intel.*Arc", line, re.IGNORECASE):
                                match = re.search(r":\s*(.+)", line)
                                if match:
                                    info["gpu"] = match.group(1).strip()
                                    info["gpu_type"] = "intel"
                                    break
            except:
                pass
        info["cuda_installed"] = (
            self.check_cuda_installed() if info["cuda_available"] else False
        )
        return info

    def log_system_info(self):
        self.log("🎵 OBSIDIAN-Neural Installer v1.0")
        self.log(f"🖥️ System: {self.system_info['os']}")
        self.log(f"💾 RAM: {self.system_info['ram_gb']} GB")

        if self.system_info["cuda_available"]:
            self.log(f"🟢 NVIDIA GPU: {self.system_info['gpu']}")
            self.log("🎯 Recommendation: CUDA Installation")
        elif self.system_info["rocm_available"]:
            self.log(f"🔴 AMD GPU: {self.system_info['gpu']}")
            self.log("🎯 Recommendation: ROCm Installation")
        elif self.system_info["gpu_type"] == "amd":
            self.log(f"🔴 AMD GPU detected: {self.system_info['gpu']}")
            self.log("⚠️ ROCm not installed")
            self.log("💡 Install ROCm for better performance")
        else:
            self.log("🎯Recommendation: CPU Installation")

    def setup_ui(self):
        style = ttk.Style()
        if "clam" in style.theme_names():
            style.theme_use("clam")
        main_frame = ttk.Frame(self.root, padding="20")
        main_frame.pack(fill="both", expand=True)
        header_frame = ttk.Frame(main_frame)
        header_frame.pack(fill="x", pady=(0, 20))
        logo_path = Path(__file__).parent / "logo.png"
        if logo_path.exists() and PIL_AVAILABLE:
            try:
                from PIL import Image, ImageTk

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
                    text="Full Installer - Real-time AI music generation VST3 plugin for live performance",
                    font=("Arial", 12),
                )
                subtitle_label.pack(anchor="w")
            except ImportError:
                self._create_title_without_logo(header_frame)
        else:
            self._create_title_without_logo(header_frame)

        if not self.is_admin:
            admin_frame = ttk.Frame(main_frame)
            admin_frame.pack(fill="x", pady=(0, 10))

            ttk.Label(
                admin_frame,
                text="⚠️ Recommended administrator privileges",
                foreground="orange",
                font=("Arial", 10, "bold"),
            ).pack()
            ttk.Button(
                admin_frame,
                text="Restart as administrator",
                command=self.request_admin,
            ).pack(pady=5)

        canvas = tk.Canvas(main_frame, highlightthickness=0, bd=0)
        scrollbar = ttk.Scrollbar(main_frame, orient="vertical", command=canvas.yview)
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
        content_frame = scrollable_frame
        sysinfo_frame = ttk.LabelFrame(
            content_frame, text="🖥️ System Information", padding="10"
        )
        sysinfo_frame.pack(fill="x", pady=(0, 20), padx=10)
        info_text = (
            f"OS: {self.system_info['os']}\n"
            f"CPU: {self.system_info['cpu']} ({self.system_info['cpu_cores']} cores, "
            f"{self.system_info['cpu_threads']} threads)\n"
            f"RAM: {self.system_info['ram_gb']} GB\n"
            f"GPU: {self.system_info['gpu']}"
        )
        ttk.Label(sysinfo_frame, text=info_text, font=("Consolas", 9)).pack(anchor="w")

        prereq_frame = ttk.LabelFrame(
            content_frame, text="🔧 Prerequisites Status", padding="10"
        )
        prereq_frame.pack(fill="x", pady=(0, 20), padx=10)

        prereq_options = [
            (self.install_python, "Python 3.10+", "main language", self.check_python()),
            (self.install_cmake, "CMake 4.0.2+", "compilation", self.check_cmake()),
            (self.install_git, "Git", "code management", self.check_git()),
            (
                self.install_buildtools,
                "Visual Studio Build Tools",
                "Windows compilation",
                self.check_buildtools() if platform.system() == "Windows" else True,
            ),
            (
                self.install_cuda,
                "CUDA Toolkit",
                "NVIDIA GPU acceleration",
                self.system_info.get("cuda_installed", False),
            ),
        ]

        for var, name, description, is_installed in prereq_options:
            item_frame = ttk.Frame(prereq_frame)
            item_frame.pack(fill="x", pady=2)

            if name == "CUDA Toolkit":
                if not self.system_info.get("cuda_available", False):
                    var.set(False)
                    cb = ttk.Checkbutton(item_frame, variable=var, state="disabled")
                    cb.pack(side="left")

                    gpu_type = self.system_info.get("gpu_type", "cpu")
                    if gpu_type == "cpu":
                        reason = "No dedicated GPU detected"
                        color = "gray"
                        status_text = f"❌ {name} - {reason}"
                    elif gpu_type == "amd":
                        reason = "AMD GPU detected (use ROCm instead)"
                        color = "orange"
                        status_text = f"❌ {name} - {reason}"
                    elif gpu_type == "intel":
                        reason = "Intel GPU detected (not supported)"
                        color = "gray"
                        status_text = f"❌ {name} - {reason}"
                    else:
                        reason = "NVIDIA GPU not detected"
                        color = "gray"
                        status_text = f"❌ {name} - {reason}"

                else:
                    suggested = not is_installed
                    var.set(suggested)
                    cb = ttk.Checkbutton(item_frame, variable=var)
                    cb.pack(side="left")

                    if is_installed:
                        status_text = f"✅ {name} - Already installed"
                        color = "dark green"
                    else:
                        status_text = f"📦 {name} - Will be installed"
                        color = "dark orange"
            else:
                suggested = not is_installed
                var.set(suggested)
                cb = ttk.Checkbutton(item_frame, variable=var)
                cb.pack(side="left")

                if is_installed:
                    status_text = f"✅ {name} - Already installed"
                    color = "dark green"
                else:
                    status_text = f"📦 {name} - Will be installed"
                    color = "dark orange"

            status_label = ttk.Label(item_frame, text=status_text, foreground=color)
            status_label.pack(side="left", padx=(5, 0))

            desc_label = ttk.Label(
                item_frame,
                text=f"({description})",
                font=("Arial", 8),
                foreground="gray",
            )
            desc_label.pack(side="left", padx=(10, 0))

        paths_frame = ttk.LabelFrame(
            content_frame, text="📁 Installation Paths", padding="10"
        )
        paths_frame.pack(fill="x", pady=(0, 20), padx=10)

        ttk.Label(paths_frame, text="Main file:").pack(anchor="w")
        path_frame1 = ttk.Frame(paths_frame)
        path_frame1.pack(fill="x", pady=(0, 10))
        ttk.Entry(path_frame1, textvariable=self.install_path, width=50).pack(
            side="left", fill="x", expand=True
        )
        ttk.Button(
            path_frame1,
            text="Browse",
            command=lambda: self.browse_folder(self.install_path),
        ).pack(side="right", padx=(5, 0))

        ttk.Label(paths_frame, text="VST3 File:").pack(anchor="w")
        path_frame2 = ttk.Frame(paths_frame)
        path_frame2.pack(fill="x")
        ttk.Entry(path_frame2, textvariable=self.vst_path, width=50).pack(
            side="left", fill="x", expand=True
        )
        ttk.Button(
            path_frame2,
            text="Browse",
            command=lambda: self.browse_folder(self.vst_path),
        ).pack(side="right", padx=(5, 0))

        options_frame = ttk.LabelFrame(content_frame, text="⚙️ Options", padding="10")
        options_frame.pack(fill="x", pady=(0, 20), padx=10)

        ttk.Checkbutton(
            options_frame,
            text="🧪 Run a performance benchmark",
            variable=self.run_benchmark,
        ).pack(anchor="w")
        ttk.Checkbutton(
            options_frame,
            text="📋 Automatically copy VST",
            variable=self.auto_copy_vst,
        ).pack(anchor="w")
        ttk.Checkbutton(
            options_frame,
            text="🚀 Start the server after installation",
            variable=self.start_after_install,
        ).pack(anchor="w")

        progress_frame = ttk.LabelFrame(content_frame, text="📊 Progress", padding="10")
        progress_frame.pack(fill="x", pady=(0, 20), padx=10)

        self.progress_bar = ttk.Progressbar(
            progress_frame, variable=self.progress_var, maximum=100
        )
        self.progress_bar.pack(fill="x", pady=(0, 5))

        status_label = ttk.Label(progress_frame, textvariable=self.status_var)
        status_label.pack()

        log_frame = ttk.LabelFrame(
            content_frame, text="📝 Installation Log", padding="5"
        )
        log_frame.pack(fill="both", expand=True, pady=(0, 15), padx=10)

        self.log_text = scrolledtext.ScrolledText(
            log_frame, height=8, font=("Consolas", 9)
        )
        self.log_text.pack(fill="both", expand=True)

        button_frame = ttk.Frame(content_frame)
        button_frame.pack(fill="x", padx=10, pady=(0, 10))

        self.install_button = ttk.Button(
            button_frame,
            text="🚀 Install OBSIDIAN-Neural",
            command=self.start_installation,
        )
        self.install_button.pack(side="left", padx=(0, 10))

        ttk.Button(button_frame, text="❌ Cancel", command=self.root.quit).pack(
            side="left"
        )

        def _on_mousewheel(event):
            canvas.yview_scroll(int(-1 * (event.delta / 120)), "units")

        def _bind_mousewheel(event):
            canvas.bind_all("<MouseWheel>", _on_mousewheel)

        def _unbind_mousewheel(event):
            canvas.unbind_all("<MouseWheel>")

        canvas.bind("<Enter>", _bind_mousewheel)
        canvas.bind("<Leave>", _unbind_mousewheel)

        def _unbind_global_scroll(event):
            canvas.unbind_all("<MouseWheel>")

        def _rebind_global_scroll(event):
            canvas.bind_all("<MouseWheel>", _on_mousewheel)

        self.log_text.bind("<Enter>", _unbind_global_scroll)
        self.log_text.bind("<Leave>", _rebind_global_scroll)

    def check_cuda_installed(self):
        try:
            result = subprocess.run(
                ["nvcc", "--version"], capture_output=True, text=True
            )
            if result.returncode == 0:
                return True

            cuda_paths = [
                "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA",
                "C:/Program Files (x86)/NVIDIA GPU Computing Toolkit/CUDA",
            ]
            for path in cuda_paths:
                if Path(path).exists():
                    return True

        except:
            pass

        return False

    def _create_title_without_logo(self, header_frame):
        title_label = ttk.Label(
            header_frame, text="OBSIDIAN-Neural", font=("Arial", 20, "bold")
        )
        title_label.pack()

        subtitle_label = ttk.Label(
            header_frame,
            text="Full Installer - Real-time AI music generation VST3 plugin for live performance",
            font=("Arial", 12),
        )
        subtitle_label.pack()

    def browse_folder(self, var):
        folder = filedialog.askdirectory(initialdir=var.get())
        if folder:
            var.set(folder)

    def log(self, message, level="INFO"):
        timestamp = time.strftime("%H:%M:%S")
        emoji = {"INFO": "ℹ️", "SUCCESS": "✅", "WARNING": "⚠️", "ERROR": "❌"}.get(
            level, "ℹ️"
        )
        formatted_msg = f"[{timestamp}] {emoji} {message}\n"

        if hasattr(self, "log_text") and self.log_text:
            self.log_text.insert(tk.END, formatted_msg)
            self.log_text.see(tk.END)
            if hasattr(self, "root") and self.root:
                self.root.update()
        else:
            print(f"LOG (UI not ready): {formatted_msg.strip()}")

    def update_progress(self, value, status=""):
        self.progress_var.set(value)
        if status:
            self.status_var.set(status)
        self.root.update()

    def start_installation(self):
        self.install_button.config(state="disabled")
        thread = threading.Thread(target=self.install_process)
        thread.daemon = True
        thread.start()

    def install_process(self):
        try:
            install_dir = Path(self.install_path.get())
            install_dir.mkdir(parents=True, exist_ok=True)
            steps = []
            if self.install_python.get():
                steps.append(("Installing Python 3.10", self.install_python_func))
            if self.install_cmake.get():
                steps.append(("Installing CMake", self.install_cmake_func))
            if self.install_git.get():
                steps.append(("Installing Git", self.install_git_func))
            if self.install_buildtools.get():
                steps.append(
                    ("Installing the Build Tools", self.install_buildtools_func)
                )
            if self.install_cuda.get():
                steps.append(("Installing CUDA Toolkit", self.install_cuda_func))

            steps.extend(
                [
                    ("Downloading the source code", self.download_source),
                    ("Creating the Python Environment", self.create_venv),
                    ("Installing Python Dependencies", self.install_python_deps),
                    ("AI Model Download (2.49 GB)", self.download_model),
                    ("Compiling the VST plugin", self.build_vst),
                    ("Environment configuration", self.setup_environment),
                ]
            )

            if self.auto_copy_vst.get():
                steps.append(("Installing the VST plugin", self.install_vst))

            if self.run_benchmark.get():
                steps.append(("Performance benchmark", self.run_benchmark_func))

            steps.append(("Finalization", self.log("Installation finalized!")))

            for i, (step_name, step_func) in enumerate(steps):
                progress = (i / len(steps)) * 100
                self.update_progress(progress, f"Étape {i+1}/{len(steps)}: {step_name}")
                self.log(f"Startup: {step_name}")

                try:
                    step_func(install_dir)
                    self.log(f"Finished: {step_name}", "SUCCESS")
                except Exception as e:
                    self.log(f"Error during {step_name}: {str(e)}", "ERROR")
                    raise

            self.update_progress(100, "Installation completed successfully!")
            self.log(
                "🎉 OBSIDIAN-Neural installation completed successfully!", "SUCCESS"
            )

            if self.start_after_install.get():
                self.start_server(install_dir)

            messagebox.showinfo(
                "Success",
                "Installation completed successfully!\n\n"
                "Check the log for configuration details.",
            )

        except Exception as e:
            self.log(f"Installation failed: {str(e)}", "ERROR")
            messagebox.showerror("Error", f"Installation failed:\n{str(e)}")
        finally:
            self.install_button.config(state="normal")

    def run_benchmark_func(self, install_dir):
        self.log("🧪 Starting the performance benchmark...")
        import json
        import math

        self.log("⚡ CPU test in progress...")
        start_time = time.time()

        result = 0
        iterations = 2000000
        for i in range(iterations):
            result += math.sqrt(i) * math.sin(i / 1000)

        cpu_time = time.time() - start_time
        cpu_score = max(1, min(100, int(100 / max(cpu_time, 0.1))))

        self.log(f"⏱️ CPU time: {cpu_time:.2f}s")

        self.log("💾 Memory test in progress...")
        memory_info = psutil.virtual_memory()
        available_gb = memory_info.available / (1024**3)
        total_gb = memory_info.total / (1024**3)

        memory_score = min(100, int(available_gb * 10))

        self.log(f"💾 Available RAM: {available_gb:.1f}GB / {total_gb:.1f}GB")

        self.log("🎮 GPU testing in progress...")
        gpu_score = 0
        gpu_info = "No GPU detected"

        try:
            gpus = GPUtil.getGPUs()
            if gpus:
                gpu = gpus[0]
                gpu_memory_gb = gpu.memoryTotal / 1024
                gpu_score = min(100, int(gpu_memory_gb * 12))
                gpu_info = f"{gpu.name} ({gpu.memoryTotal}MB VRAM)"
                try:
                    gpu_usage = gpu.load * 100
                    self.log(f"🎮 Current GPU usage: {gpu_usage:.1f}%")
                except:
                    pass

            else:
                gpu_score = 0
                gpu_info = "No GPU detected"

        except Exception as e:
            self.log(f"⚠️ GPU detection error: {e}", "WARNING")
            gpu_score = 0

        self.log(f"🎮 GPU: {gpu_info}")

        self.log("💿 Storage test in progress...")
        try:
            disk_usage = psutil.disk_usage(install_dir)
            free_gb = disk_usage.free / (1024**3)
            total_gb = disk_usage.total / (1024**3)
            storage_score = min(100, int(free_gb / 2))

            self.log(f"💿 Free storage: {free_gb:.1f}GB / {total_gb:.1f}GB")
        except:
            storage_score = 50
            self.log("💿 Storage: Unable to determine")

        weights = {
            "cpu": 0.3,
            "memory": 0.25,
            "gpu": 0.35,
            "storage": 0.1,
        }

        global_score = int(
            cpu_score * weights["cpu"]
            + memory_score * weights["memory"]
            + gpu_score * weights["gpu"]
            + storage_score * weights["storage"]
        )

        if global_score >= 85:
            performance_level = "🚀 Excellent"
            perf_color = "SUCCESS"
        elif global_score >= 70:
            performance_level = "✅ Very good"
            perf_color = "SUCCESS"
        elif global_score >= 55:
            performance_level = "⚡ Good"
            perf_color = "INFO"
        elif global_score >= 40:
            performance_level = "⚠️ Correct"
            perf_color = "WARNING"
        else:
            performance_level = "❌ Insufficient"
            perf_color = "ERROR"

        recommendations = []

        if cpu_score < 50:
            recommendations.append(
                "🔧 Slow CPU - Consider a more powerful processor (Intel i5+ or AMD Ryzen 5+)"
            )

        if memory_score < 60:
            recommendations.append(
                "💾 Insufficient RAM - 16GB+ recommended for large AI models"
            )

        if gpu_score == 0:
            recommendations.append(
                "🎮 No dedicated GPU detected - RTX 3060+ highly recommended for reasonable generation times"
            )
        elif gpu_score < 30:
            recommendations.append(
                "🎮 Low-end GPU detected - Expect very long generation times (20+ minutes)"
            )
        elif gpu_score < 50:
            recommendations.append(
                "🎮 Entry-level GPU - Generation will be slow (5-15 minutes)"
            )
        elif gpu_score < 70:
            recommendations.append(
                "🎮 Good GPU - Decent generation times (1-5 minutes)"
            )
        else:
            recommendations.append("🎮 Excellent GPU - Fast generation times")

        if global_score < 70:
            recommendations.append(
                "⚡ Performance limited - Consider upgrading GPU for faster audio generation"
            )
        else:
            recommendations.append(
                "🎉 Good configuration for OBSIDIAN-Neural audio generation!"
            )

        if not recommendations:
            recommendations.append("🎉 Excellent setup for OBSIDIAN-Neural!")

        if global_score >= 90:
            perf_estimate = (
                "Audio generation: 30-60 seconds for 10sec sample (High-end RTX 4080+)"
            )
        elif global_score >= 80:
            perf_estimate = (
                "Audio generation: 1-2 minutes for 10sec sample (RTX 3070/4070 class)"
            )
        elif global_score >= 70:
            perf_estimate = (
                "Audio generation: 2-5 minutes for 10sec sample (RTX 3060/laptop RTX)"
            )
        elif global_score >= 60:
            perf_estimate = (
                "Audio generation: 5-15 minutes for 10sec sample (GTX 1660/lower RTX)"
            )
        elif global_score >= 40:
            perf_estimate = (
                "Audio generation: 15-30 minutes for 10sec sample (Integrated/old GPU)"
            )
        else:
            perf_estimate = (
                "Audio generation: 30+ minutes for 10sec sample (CPU only - very slow)"
            )

        benchmark_results = {
            "timestamp": time.strftime("%Y-%m-%d %H:%M:%S"),
            "system_info": self.system_info,
            "scores": {
                "cpu": cpu_score,
                "memory": memory_score,
                "gpu": gpu_score,
                "storage": storage_score,
                "global": global_score,
            },
            "performance_level": performance_level,
            "performance_estimate": perf_estimate,
            "recommendations": recommendations,
            "hardware_details": {
                "cpu_time": f"{cpu_time:.2f}s",
                "available_ram_gb": f"{available_gb:.1f}",
                "gpu_info": gpu_info,
                "free_storage_gb": (
                    f"{free_gb:.1f}" if "free_gb" in locals() else "Unknown"
                ),
            },
        }

        benchmark_file = install_dir / "benchmark_results.json"
        with open(benchmark_file, "w", encoding="utf-8") as f:
            json.dump(benchmark_results, f, indent=2, ensure_ascii=False)

        self.log("=" * 50)
        self.log("📊 BENCHMARK RESULTS", "SUCCESS")
        self.log("=" * 50)
        self.log(f"🔥CPU Score: {cpu_score}/100")
        self.log(f"💾 Memory Score: {memory_score}/100")
        self.log(f"🎮 GPU Score: {gpu_score}/100")
        self.log(f"💿 Storage Score: {storage_score}/100")
        self.log("=" * 30)
        self.log(f"🎯 Global Score: {global_score}/100", perf_color)
        self.log(f"⭐ Performance: {performance_level}", perf_color)
        self.log(f"⚡ Estimate: {perf_estimate}")
        self.log("=" * 30)

        self.log("💡 RECOMMENDATIONS:")

        for i, rec in enumerate(recommendations, 1):
            self.log(f" {i}. {rec}")

        self.log("=" * 50)
        self.log(f"📋 Saved report: {benchmark_file}")

        return benchmark_results

    def install_python_func(self, install_dir):
        if platform.system() == "Windows":
            self.log("Downloading Python 3.10...")
            python_url = (
                "https://www.python.org/ftp/python/3.10.11/python-3.10.11-amd64.exe"
            )
            python_installer = install_dir / "python_installer.exe"

            urllib.request.urlretrieve(python_url, python_installer)

            self.log("Installing Python...")
            cmd = [
                str(python_installer),
                "/quiet",
                "InstallAllUsers=1",
                "PrependPath=1",
                "Include_test=0",
            ]
            subprocess.run(cmd, check=True)

            python_installer.unlink()
        else:
            raise Exception(
                "Automatic installation of Python is not supported on this system."
                "Please install Python 3.10+ manually."
            )

    def install_cmake_func(self, install_dir):
        if platform.system() == "Windows":
            self.log("Downloading CMake...")
            cmake_url = "https://github.com/Kitware/CMake/releases/download/v3.28.1/cmake-3.28.1-windows-x86_64.msi"
            cmake_installer = install_dir / "cmake_installer.msi"

            urllib.request.urlretrieve(cmake_url, cmake_installer)

            self.log("Installing CMake...")
            cmd = [
                "msiexec",
                "/i",
                str(cmake_installer),
                "/quiet",
                "ADD_CMAKE_TO_PATH=System",
            ]
            subprocess.run(cmd, check=True)

            cmake_installer.unlink()
        else:
            if shutil.which("apt-get"):
                subprocess.run(["sudo", "apt-get", "update"], check=True)
                subprocess.run(
                    ["sudo", "apt-get", "install", "-y", "cmake"], check=True
                )
            elif shutil.which("brew"):
                subprocess.run(["brew", "install", "cmake"], check=True)
            else:
                raise Exception(
                    "Package manager not supported. Install CMake manually."
                )

    def download_model(self, install_dir):
        models_dir = install_dir / "models"
        model_path = models_dir / "gemma-3-4b-it.gguf"

        if model_path.exists():
            self.log("Model already uploaded, ignored")
            return

        model_url = "https://huggingface.co/unsloth/gemma-3-4b-it-GGUF/resolve/main/gemma-3-4b-it-Q4_K_M.gguf"

        self.log("Download the Gemma-3-4B model (2.49 GB)...")

        def download_progress(block_num, block_size, total_size):
            downloaded = block_num * block_size
            percent = min(100, (downloaded / total_size) * 100)
            self.update_progress(percent * 0.7, f"Download model: {percent:.1f}%")

        urllib.request.urlretrieve(model_url, model_path, reporthook=download_progress)
        self.log("✅ Model downloaded successfully.")

    def install_git_func(self, install_dir):
        if platform.system() == "Windows":
            self.log("Downloading Git...")
            git_url = "https://github.com/git-for-windows/git/releases/download/v2.43.0.windows.1/Git-2.43.0-64-bit.exe"
            git_installer = install_dir / "git_installer.exe"

            urllib.request.urlretrieve(git_url, git_installer)

            self.log("Installing Git...")
            cmd = [
                str(git_installer),
                "/SILENT",
                "/COMPONENTS=icons,ext\\reg\\shellhere,assoc,assoc_sh",
            ]
            subprocess.run(cmd, check=True)

            git_installer.unlink()
        else:
            if shutil.which("apt-get"):
                subprocess.run(["sudo", "apt-get", "install", "-y", "git"], check=True)
            elif shutil.which("brew"):
                subprocess.run(["brew", "install", "git"], check=True)

    def install_buildtools_func(self, install_dir):
        if platform.system() != "Windows":
            return

        self.log("Downloading Visual Studio Build Tools...")
        buildtools_url = "https://aka.ms/vs/17/release/vs_buildtools.exe"
        buildtools_installer = install_dir / "vs_buildtools.exe"

        urllib.request.urlretrieve(buildtools_url, buildtools_installer)

        self.log("Installing the Build Tools (this may take some time)...")
        cmd = [
            str(buildtools_installer),
            "--quiet",
            "--wait",
            "--add",
            "Microsoft.VisualStudio.Workload.VCTools",
            "--add",
            "Microsoft.VisualStudio.Component.VC.CMake.Project",
        ]
        subprocess.run(cmd, check=True)

        buildtools_installer.unlink()

    def install_cuda_func(self, install_dir):
        if platform.system() == "Windows":
            self.log("Downloading CUDA Toolkit 11.8...")
            cuda_url = "https://developer.download.nvidia.com/compute/cuda/11.8.0/local_installers/cuda_11.8.0_522.06_windows.exe"
            cuda_installer = install_dir / "cuda_installer.exe"
            self.log("⚠️ CUDA download (3+ GB) - this may take some time...")
            urllib.request.urlretrieve(cuda_url, cuda_installer)

            self.log("CUDA installation in progress...")
            cmd = [str(cuda_installer), "-s"]
            subprocess.run(cmd, check=True)

            cuda_installer.unlink()

    def download_source(self, install_dir):
        current_dir = Path(__file__).parent

        repo_markers = [
            current_dir / "main.py",
            current_dir / "vst",
            current_dir / "server",
            current_dir / "core",
            current_dir / "install.bat",
            current_dir / "install.sh",
        ]

        is_in_repo = all(marker.exists() for marker in repo_markers)

        if is_in_repo:
            self.log("Source code detected locally (development mode)")
            self.log(f"Using code from: {current_dir}")

            if str(install_dir) == str(current_dir):
                self.log("Installing in same folder - creating symbolic link")
                return install_dir

            import shutil

            exclude_items = {
                "installer.py",
                "__pycache__",
                ".git",
                "env",
                "venv",
                ".env",
                "models",
                "node_modules",
                "build",
                "dist",
                ".pytest_cache",
                "screenshot.png",
            }

            install_dir.mkdir(parents=True, exist_ok=True)

            if self.is_admin:
                import subprocess

                subprocess.run(
                    [
                        "icacls",
                        str(install_dir),
                        "/grant",
                        "Authenticated Users:(OI)(CI)F",
                    ],
                    check=False,
                    capture_output=True,
                )

            for item in current_dir.iterdir():
                if item.name not in exclude_items:
                    target = install_dir / item.name
                    try:
                        if item.is_file():
                            shutil.copy2(item, target)
                            self.log(f"Copied file: {item.name}")
                        elif item.is_dir():
                            shutil.copytree(item, target, dirs_exist_ok=True)
                            self.log(f"Copied directory: {item.name}/")
                    except Exception as e:
                        self.log(f"Warning copying {item.name}: {e}", "WARNING")

            self.log("Source code copied from local folder")

        else:
            self.log("Cloning innermost47/ai-dj repository from GitHub...")

            install_dir.mkdir(parents=True, exist_ok=True)

            if self.is_admin:
                import subprocess

                subprocess.run(
                    [
                        "icacls",
                        str(install_dir),
                        "/grant",
                        "Authenticated Users:(OI)(CI)F",
                    ],
                    check=False,
                    capture_output=True,
                )

            cmd = [
                "git",
                "clone",
                "https://github.com/innermost47/ai-dj.git",
                str(install_dir),
            ]
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=300)

            if result.returncode != 0:
                raise Exception(f"Git clone error: {result.stderr}")

            self.log("Repository cloned successfully")

        essential_dirs = ["server", "core", "vst"]
        for dir_name in essential_dirs:
            if (install_dir / dir_name).exists():
                self.log(f"Directory {dir_name}/ found")
            else:
                self.log(f"Directory {dir_name}/ missing", "WARNING")

        return install_dir

    def create_venv(self, install_dir):
        venv_path = install_dir / "env"
        models_dir = install_dir / "models"

        try:
            install_dir.mkdir(parents=True, exist_ok=True)
            models_dir.mkdir(exist_ok=True)

            if self.is_admin:
                import subprocess

                subprocess.run(
                    [
                        "icacls",
                        str(install_dir),
                        "/grant",
                        "Authenticated Users:(OI)(CI)F",
                    ],
                    check=False,
                    capture_output=True,
                )

        except PermissionError as e:
            self.log(f"Permission error: {e}", "ERROR")
            raise Exception(f"Cannot create installation directory: {e}")

        self.log("Creating Python virtual environment...")
        self.log(f"Installation directory: {install_dir}")

        if venv_path.exists():
            try:
                import shutil

                shutil.rmtree(venv_path)
                self.log("Old environment removed")
            except Exception as e:
                self.log(f"Could not remove old environment: {e}", "WARNING")

        python_exe = self.find_system_python()
        cmd = [python_exe, "-m", "venv", str(venv_path)]

        try:
            env = os.environ.copy()
            if self.is_admin:
                env.pop("PYTHONPATH", None)
                env.pop("VIRTUAL_ENV", None)
                env.pop("CONDA_DEFAULT_ENV", None)

            result = subprocess.run(
                cmd, capture_output=True, text=True, check=True, env=env, timeout=120
            )
            self.log("Virtual environment created successfully")
            return install_dir

        except subprocess.CalledProcessError as e:
            self.log(f"Failed to create virtual environment", "ERROR")
            self.log(f"Error output: {e.stderr}", "ERROR")
            raise Exception(f"Virtual environment creation failed: {e.stderr}")

    def install_vst(self, install_dir):
        vst_build_dir = install_dir / "vst" / "build"
        vst_target_dir_str = self.vst_path.get()
        self.log(f"Attempting to use VST3 path: {vst_target_dir_str}")
        vst_target_dir = Path(vst_target_dir_str)

        if not vst_target_dir.exists():
            self.log(
                f"Provided VST3 path '{vst_target_dir}' does not exist. Attempting to create it.",
                "WARNING",
            )
        elif not vst_target_dir.is_dir():
            self.log(
                f"Provided VST3 path '{vst_target_dir}' is not a directory. Cannot install VST here.",
                "ERROR",
            )
            raise Exception(f"VST3 path '{vst_target_dir}' is not a directory.")
        else:
            self.log(f"VST3 path '{vst_target_dir}' exists and is a directory.")

        possible_vst_locations = [
            vst_build_dir,
            vst_build_dir / "VST3",
            vst_build_dir / "Release" / "VST3",
            vst_build_dir / "RelWithDebInfo" / "VST3",
            vst_build_dir / "Debug" / "VST3",
            vst_build_dir / "ObsidianNeural_artefacts" / "Release" / "VST3",
            vst_build_dir / "ObsidianNeural_artefacts" / "VST3",
            vst_build_dir
            / "ObsidianNeuralVST_artefacts"
            / "Release"
            / "VST3"
            / "OBSIDIAN-Neural.vst3"
            / "Contents"
            / "x86_64-win",
        ]

        vst_files_found = []
        for loc in possible_vst_locations:
            if loc.exists() and loc.is_dir():
                self.log(f"Searching for .vst3 in {loc}")
                vst_files_found.extend(list(loc.glob("*.vst3")))

        vst_files = []
        seen_paths = set()
        for f_path in vst_files_found:
            resolved_path = f_path.resolve()
            if resolved_path not in seen_paths:
                vst_files.append(f_path)
                seen_paths.add(resolved_path)
                self.log(f"Found potential VST3: {f_path} (Is Dir: {f_path.is_dir()})")

        if not vst_files:
            self.log(
                f"No VST3 file/bundle found after compilation in standard build output locations under {vst_build_dir}.",
                "ERROR",
            )
            self.log(f"Checked locations: {possible_vst_locations}", "INFO")
            raise Exception(
                "VST3 plugin compilation result not found. Check build logs."
            )

        vst_file_to_copy = None
        for f in vst_files:
            if f.is_dir():
                vst_file_to_copy = f
                break

        if not vst_file_to_copy and vst_files:
            vst_file_to_copy = vst_files[0]
            self.log(
                f"Warning: Found .vst3 as a file, not a directory: {vst_file_to_copy}",
                "WARNING",
            )
        elif not vst_file_to_copy:
            self.log(
                f"Critical error: vst_files list was populated but no suitable vst_file_to_copy selected.",
                "ERROR",
            )
            raise Exception("VST3 selection logic error after finding candidates.")

        self.log(
            f"Selected VST3 source for copy: {vst_file_to_copy} (Name: {vst_file_to_copy.name}, Is Dir: {vst_file_to_copy.is_dir()})"
        )
        target_plugin_path = vst_target_dir / vst_file_to_copy.name

        try:
            self.log(f"Ensuring VST3 parent directory exists: {vst_target_dir}")
            vst_target_dir.mkdir(parents=True, exist_ok=True)
            self.log(f"VST3 parent directory '{vst_target_dir}' confirmed/created.")

            if self.is_admin and platform.system() == "Windows":
                self.log(
                    f"Attempting to set permissions for Users on {vst_target_dir} (Admin context)"
                )
                acl_result = subprocess.run(
                    [
                        "icacls",
                        str(vst_target_dir),
                        "/grant",
                        "Users:(OI)(CI)F",
                        "/T",
                    ],
                    check=False,
                    capture_output=True,
                    text=True,
                    timeout=30,
                )
                if acl_result.returncode != 0:
                    self.log(
                        f"icacls for {vst_target_dir} failed (code {acl_result.returncode}): {acl_result.stdout.strip()} {acl_result.stderr.strip()}",
                        "WARNING",
                    )
                else:
                    self.log(
                        f"icacls for {vst_target_dir} grant Users:F succeeded.", "INFO"
                    )

            self.log(f"VST3 source for copy operation: {vst_file_to_copy}")
            self.log(f"VST3 target destination for copy: {target_plugin_path}")

            if not vst_file_to_copy.exists():
                self.log(
                    f"Source VST file/bundle {vst_file_to_copy} does not exist before copy attempt!",
                    "ERROR",
                )
                raise Exception(
                    f"Source VST {vst_file_to_copy} vanished or was not found correctly."
                )

            if vst_file_to_copy.is_dir():
                self.log(
                    f"Source {vst_file_to_copy.name} is a directory. Using shutil.copytree."
                )
                if target_plugin_path.exists():
                    self.log(
                        f"Target {target_plugin_path} already exists. Removing it first."
                    )
                    try:
                        shutil.rmtree(target_plugin_path)
                        self.log(f"Successfully removed existing {target_plugin_path}.")
                    except Exception as e_rm:
                        self.log(
                            f"Failed to remove existing {target_plugin_path}: {e_rm}",
                            "ERROR",
                        )
                        raise Exception(
                            f"Failed to overwrite existing VST at {target_plugin_path}: {e_rm}"
                        )
                shutil.copytree(vst_file_to_copy, target_plugin_path)
                self.log(
                    f"VST3 bundle copied from {vst_file_to_copy} to {target_plugin_path}"
                )
            else:
                self.log(
                    f"Source {vst_file_to_copy.name} is a file (unexpected for VST3). Using shutil.copy2.",
                    "WARNING",
                )
                if target_plugin_path.exists() and target_plugin_path.is_dir():
                    self.log(
                        f"Target {target_plugin_path} is a directory, but source is a file. Removing target directory.",
                        "WARNING",
                    )
                    shutil.rmtree(target_plugin_path)
                elif target_plugin_path.exists():
                    self.log(
                        f"Target file {target_plugin_path} exists. It will be overwritten."
                    )
                shutil.copy2(vst_file_to_copy, target_plugin_path)
                self.log(
                    f"VST3 file copied from {vst_file_to_copy} to {target_plugin_path}"
                )

            time.sleep(0.2)

            if target_plugin_path.exists():
                self.log(
                    f"VST plugin successfully installed to: {target_plugin_path}",
                    "SUCCESS",
                )
                if self.is_admin and platform.system() == "Windows":
                    self.log(
                        f"Attempting to set read permissions for Users on {target_plugin_path} (Admin context)"
                    )
                    acl_target_result = subprocess.run(
                        [
                            "icacls",
                            str(target_plugin_path),
                            "/grant",
                            "Users:(OI)(CI)R",
                            "/T",
                        ],
                        check=False,
                        capture_output=True,
                        text=True,
                        timeout=30,
                    )
                    if acl_target_result.returncode != 0:
                        self.log(
                            f"icacls for {target_plugin_path} grant Users:R failed (code {acl_target_result.returncode}): {acl_target_result.stdout.strip()} {acl_target_result.stderr.strip()}",
                            "WARNING",
                        )
                    else:
                        self.log(
                            f"icacls for {target_plugin_path} grant Users:R succeeded.",
                            "INFO",
                        )
            else:
                self.log(
                    f"CRITICAL: VST copy verification failed. Target {target_plugin_path} does not exist after copy attempt.",
                    "ERROR",
                )
                self.log(
                    f"Source was: {vst_file_to_copy} (exists: {vst_file_to_copy.exists()})",
                    "INFO",
                )
                self.log(
                    f"Target VST3 parent folder: {vst_target_dir} (exists: {vst_target_dir.exists()})",
                    "INFO",
                )
                raise Exception(
                    f"VST copy verification failed: {target_plugin_path} not found after copy."
                )

        except PermissionError as e:
            self.log(f"Permission denied during VST installation step: {e}", "ERROR")
            self.log(
                f"Details: Source='{vst_file_to_copy}', Target='{target_plugin_path}', TargetParent='{vst_target_dir}'",
                "INFO",
            )
            self.log(
                "This usually happens if the script is not run as administrator when targeting system VST folders (e.g., Program Files).",
                "WARNING",
            )
            self.log(
                "You might need to run the installer as an administrator or choose a user-writable VST3 path.",
                "WARNING",
            )
            self.log("Manual copy instructions:", "WARNING")
            self.log(f"   Copy FROM: {vst_file_to_copy}", "INFO")
            self.log(f"   Copy TO:   {target_plugin_path}", "INFO")
            raise

        except Exception as e:
            self.log(
                f"An unexpected error occurred during VST installation: {e}", "ERROR"
            )
            self.log(
                f"Details: Source='{vst_file_to_copy if 'vst_file_to_copy' in locals() else 'N/A'}', Target='{target_plugin_path if 'target_plugin_path' in locals() else 'N/A'}', TargetParent='{vst_target_dir}'",
                "INFO",
            )
            self.log("Manual copy may be required (see paths above).", "WARNING")
            raise

    def find_system_python(self):
        if not (hasattr(sys, "base_prefix") and sys.base_prefix != sys.prefix):
            current_python = sys.executable
            if self.test_python_executable(current_python):
                return current_python

        if hasattr(sys, "base_prefix") and sys.base_prefix != sys.prefix:
            if platform.system() == "Windows":
                base_python = Path(sys.base_prefix) / "python.exe"
            else:
                base_python = Path(sys.base_prefix) / "bin" / "python"

            if base_python.exists() and self.test_python_executable(str(base_python)):
                return str(base_python)

        import shutil

        for py_name in ["python", "python3", "python.exe"]:
            py_path = shutil.which(py_name)
            if py_path and self.test_python_executable(py_path):
                return py_path

        if platform.system() == "Windows":
            common_paths = [
                "C:\\Python311\\python.exe",
                "C:\\Python310\\python.exe",
                "C:\\Python39\\python.exe",
                "C:\\Python38\\python.exe",
                "C:\\Program Files\\Python311\\python.exe",
                "C:\\Program Files\\Python310\\python.exe",
                "C:\\Users\\Public\\AppData\\Local\\Programs\\Python\\Python311\\python.exe",
            ]

            for py_path in common_paths:
                if Path(py_path).exists() and self.test_python_executable(py_path):
                    return py_path

        return "python"

    def test_python_executable(self, python_path):
        try:
            result = subprocess.run(
                [python_path, "--version"], capture_output=True, text=True, timeout=10
            )
            if result.returncode == 0:
                version_output = result.stdout.strip()
                if "Python 3." in version_output:
                    version_parts = version_output.split()[1].split(".")
                    major, minor = int(version_parts[0]), int(version_parts[1])
                    return major == 3 and minor >= 8
        except:
            pass
        return False

    def install_python_deps(self, install_dir):
        self.log(f"Installing dependencies in: {install_dir}")

        if platform.system() == "Windows":
            pip_path = install_dir / "env" / "Scripts" / "pip.exe"
            python_path = install_dir / "env" / "Scripts" / "python.exe"
        else:
            pip_path = install_dir / "env" / "bin" / "pip"
            python_path = install_dir / "env" / "bin" / "python"

        self.log(f"Looking for Python at: {python_path}")

        if not python_path.exists():
            self.log(f"Python executable not found at: {python_path}", "ERROR")
            self.log("Searching for Python in virtual environment...", "INFO")

            venv_dir = install_dir / "env"
            self.log(f"Checking virtual environment directory: {venv_dir}")

            if not venv_dir.exists():
                self.log(
                    f"Virtual environment directory does not exist: {venv_dir}", "ERROR"
                )
                raise Exception(f"Virtual environment not found at: {venv_dir}")

            if platform.system() == "Windows":
                possible_python_paths = [
                    venv_dir / "Scripts" / "python.exe",
                    venv_dir / "Scripts" / "python3.exe",
                    venv_dir / "python.exe",
                ]
            else:
                possible_python_paths = [
                    venv_dir / "bin" / "python",
                    venv_dir / "bin" / "python3",
                ]

            for py_path in possible_python_paths:
                self.log(f"Checking: {py_path}")
                if py_path.exists():
                    python_path = py_path
                    self.log(f"Found Python at: {python_path}")
                    break
            else:
                try:
                    venv_contents = list(venv_dir.iterdir())
                    self.log(
                        f"Virtual environment contents: {[str(p) for p in venv_contents]}"
                    )
                    if (venv_dir / "Scripts").exists():
                        scripts_contents = list((venv_dir / "Scripts").iterdir())
                        self.log(
                            f"Scripts directory contents: {[str(p) for p in scripts_contents]}"
                        )
                except:
                    self.log("Could not list virtual environment contents")

                raise Exception(
                    f"No Python executable found in virtual environment: {venv_dir}"
                )

        if not pip_path.exists():
            self.log(f"Pip executable not found at: {pip_path}", "WARNING")
            self.log("Will use python -m pip instead")
            pip_path = None

        if self.system_info.get("cuda_available"):
            self.log("Installing PyTorch with CUDA support...")
            cmd = [
                str(python_path),
                "-m",
                "pip",
                "install",
                "torch",
                "torchvision",
                "torchaudio",
                "--index-url",
                "https://download.pytorch.org/whl/cu118",
            ]
            try:
                subprocess.run(cmd, check=True, timeout=900)
                self.log("PyTorch with CUDA installed successfully")
            except subprocess.TimeoutExpired:
                self.log("PyTorch installation timed out", "ERROR")
                raise Exception("PyTorch installation timed out")
            except subprocess.CalledProcessError as e:
                self.log(f"PyTorch CUDA installation failed: {e.stderr}", "ERROR")
                raise

            self.log("Installing Llama CPP Python with CUDA support...")
            cmd = [
                str(python_path),
                "-m",
                "pip",
                "install",
                "llama-cpp-python==0.3.9",
                "--prefer-binary",
                "--extra-index-url=https://jllllll.github.io/llama-cpp-python-cuBLAS-wheels/AVX2/cu118",
            ]
            try:
                subprocess.run(cmd, check=True, timeout=900)
                self.log("Llama CPP Python with CUDA installed successfully")
            except subprocess.TimeoutExpired:
                self.log("Llama CPP Python installation timed out", "ERROR")
                raise Exception("Llama CPP Python installation timed out")
            except subprocess.CalledProcessError as e:
                self.log(f"Llama CPP Python installation failed: {e.stderr}", "ERROR")
                raise

        elif self.system_info.get("gpu_type") == "amd":
            self.log("Installing PyTorch with ROCm support...")
            cmd = [
                str(python_path),
                "-m",
                "pip",
                "install",
                "torch",
                "torchvision",
                "torchaudio",
                "--index-url",
                "https://download.pytorch.org/whl/rocm5.6",
            ]
            try:
                subprocess.run(cmd, check=True, timeout=900)
                self.log("PyTorch with ROCm installed successfully")
            except subprocess.TimeoutExpired:
                self.log("PyTorch installation timed out", "ERROR")
                raise Exception("PyTorch installation timed out")
            except subprocess.CalledProcessError as e:
                self.log(f"PyTorch ROCm installation failed: {e.stderr}", "ERROR")
                raise

            self.log("Installing Llama CPP Python for CPU/ROCm...")
            cmd = [str(python_path), "-m", "pip", "install", "llama-cpp-python==0.3.9"]
            try:
                subprocess.run(cmd, check=True, timeout=600)
                self.log("Llama CPP Python installed successfully")
            except subprocess.TimeoutExpired:
                self.log("Llama CPP Python installation timed out", "ERROR")
                raise Exception("Llama CPP Python installation timed out")
            except subprocess.CalledProcessError as e:
                self.log(f"Llama CPP Python installation failed: {e.stderr}", "ERROR")
                raise

        else:
            self.log("Installing PyTorch for CPU...")
            cmd = [
                str(python_path),
                "-m",
                "pip",
                "install",
                "torch",
                "torchvision",
                "torchaudio",
            ]
            try:
                subprocess.run(cmd, check=True, timeout=900)
                self.log("PyTorch for CPU installed successfully")
            except subprocess.TimeoutExpired:
                self.log("PyTorch installation timed out", "ERROR")
                raise Exception("PyTorch installation timed out")
            except subprocess.CalledProcessError as e:
                self.log(f"PyTorch installation failed: {e.stderr}", "ERROR")
                raise

            self.log("Installing Llama CPP Python for CPU...")
            cmd = [str(python_path), "-m", "pip", "install", "llama-cpp-python==0.3.9"]
            try:
                subprocess.run(cmd, check=True, timeout=600)
                self.log("Llama CPP Python for CPU installed successfully")
            except subprocess.TimeoutExpired:
                self.log("Llama CPP Python installation timed out", "ERROR")
                raise Exception("Llama CPP Python installation timed out")
            except subprocess.CalledProcessError as e:
                self.log(f"Llama CPP Python installation failed: {e.stderr}", "ERROR")
                raise

        self.log("Installing additional libraries...")
        packages = [
            "stable-audio-tools",
            "librosa",
            "soundfile",
            "fastapi",
            "uvicorn",
            "python-dotenv",
            "requests",
            "apscheduler",
            "demucs",
            "cryptography",
        ]

        for package in packages:
            self.log(f"Installing {package}...")
            cmd = [str(python_path), "-m", "pip", "install", package]
            try:
                subprocess.run(
                    cmd, check=True, capture_output=True, text=True, timeout=300
                )
                self.log(f"{package} installed successfully")
            except subprocess.CalledProcessError as e:
                self.log(f"Installation error for {package}: {e.stderr}", "WARNING")
            except subprocess.TimeoutExpired:
                self.log(f"Installation timeout for {package}", "WARNING")

        self.log("All Python dependencies installation completed")

    def setup_environment(self, install_dir):
        env_content = f"""DJ_IA_API_KEYS=api keys separated by commas
    LLM_MODEL_PATH={install_dir}/models/gemma-3-4b-it.gguf
    ENVIRONMENT=dev
    HOST=127.0.0.1
    PORT=8000
    AUDIO_MODEL=stabilityai/stable-audio-open-1.0
    """

        (install_dir / ".env").write_text(env_content)
        self.log("✅ .env configuration created")

    def build_vst(self, install_dir):
        vst_dir = install_dir / "vst"

        if not vst_dir.exists():
            raise Exception("VST source code missing")

        cmake_file = vst_dir / "CMakeLists.txt"
        if not cmake_file.exists():
            raise Exception("CMakeLists.txt not found in vst/")

        juce_dir = vst_dir / "JUCE"
        if not juce_dir.exists():
            self.log("Downloading JUCE framework...")
            cmd = [
                "git",
                "clone",
                "https://github.com/juce-framework/JUCE.git",
                str(juce_dir),
                "--depth",
                "1",
            ]
            result = subprocess.run(cmd, capture_output=True, text=True)
            if result.returncode != 0:
                raise Exception(f"JUCE download failed: {result.stderr}")

        soundtouch_dir = vst_dir / "soundtouch"
        if not soundtouch_dir.exists():
            self.log("Downloading SoundTouch library...")
            cmd = [
                "git",
                "clone",
                "https://codeberg.org/soundtouch/soundtouch.git",
                str(soundtouch_dir),
                "--depth",
                "1",
            ]
            result = subprocess.run(cmd, capture_output=True, text=True)
            if result.returncode != 0:
                raise Exception(f"SoundTouch download failed: {result.stderr}")

        build_dir = vst_dir / "build"

        if build_dir.exists():
            self.log("Cleaning previous build cache...")
            import shutil

            shutil.rmtree(build_dir)

        build_dir.mkdir(exist_ok=True)

        if self.is_admin:
            import subprocess

            subprocess.run(
                ["icacls", str(build_dir), "/grant", "Authenticated Users:(OI)(CI)F"],
                check=False,
                capture_output=True,
            )

        self.log("Configuring CMake...")
        self.log(f"Build directory: {build_dir}")
        self.log(f"Source directory: {vst_dir}")

        cmake_cmd = ["cmake", ".."]
        result = subprocess.run(
            cmake_cmd, cwd=build_dir, capture_output=True, text=True
        )

        if result.returncode != 0:
            self.log("CMake configuration failed", "ERROR")
            self.log(f"Stdout: {result.stdout}", "ERROR")
            self.log(f"Stderr: {result.stderr}", "ERROR")
            self.log("Check that CMake and Build Tools are installed", "WARNING")
            raise Exception("CMake configuration failed")

        self.log("CMake configuration successful")

        self.log("Compiling VST plugin...")
        build_cmd = ["cmake", "--build", ".", "--config", "Release"]
        result = subprocess.run(
            build_cmd, cwd=build_dir, capture_output=True, text=True
        )

        if result.returncode != 0:
            self.log("VST compilation failed", "ERROR")
            self.log(f"Stdout: {result.stdout}", "ERROR")
            self.log(f"Stderr: {result.stderr}", "ERROR")
            self.log("Check CMake configuration and Build Tools", "WARNING")
            raise Exception("VST compilation failed")
        else:
            self.log("VST plugin compiled successfully")

    def start_server(self, install_dir):
        try:
            if platform.system() == "Windows":
                python_path = install_dir / "env" / "Scripts" / "python.exe"
            else:
                python_path = install_dir / "env" / "bin" / "python"

            main_script = install_dir / "main.py"

            subprocess.Popen([str(python_path), str(main_script)], cwd=install_dir)

            self.log("✅ Server started via main.py!")

        except Exception as e:
            self.log(f"❌ Startup error: {e}", "ERROR")

    def run(self):

        if not self.is_admin:
            self.log("⚠️ Recommended admin privileges", "WARNING")

        self.root.mainloop()


if __name__ == "__main__":
    try:
        import GPUtil
        import psutil

        if PIL_AVAILABLE:
            print("✅ PIL available for the logo")
    except ImportError:
        subprocess.check_call(
            [sys.executable, "-m", "pip", "install", "GPUtil", "psutil", "Pillow"]
        )

    app = ObsidianNeuralInstaller()
    app.run()
