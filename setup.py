#!/usr/bin/env python3

import os
import subprocess
import sys
from pathlib import Path

CERT_FILE = "server.crt"
KEY_FILE = "server.key"
STATIC_ROOT = "./static"
BUILD_DIR = "./build"

RED = "\033[0;31m"
GREEN = "\033[0;32m"
NC = "\033[0m" 


def print_colored(message, color=NC):
    """Print a message with ANSI color codes."""
    print(f"{color}{message}{NC}")


def check_command_exists(command):
    """Check if a command is available on the system."""
    return subprocess.call(["which", command], stdout=subprocess.PIPE, stderr=subprocess.PIPE) == 0


def install_dependencies():
    """Install system-level dependencies based on the operating system."""
    print_colored("Checking and installing dependencies...")

    if sys.platform.startswith("linux"):
        if check_command_exists("apt"):
            package_manager = "apt"
        else:
            print_colored("No supported package manager found (apt required).", RED)
            sys.exit(1)
    elif sys.platform == "darwin":  
        if check_command_exists("brew"):
            package_manager = "brew"
        else:
            print_colored("Homebrew not found on macOS. Install it from https://brew.sh/", RED)
            sys.exit(1)
    else:
        print_colored(f"Unsupported platform: {sys.platform}. Install dependencies manually.", RED)
        sys.exit(1)

    deps = {
        "build-essential": "build-essential" if package_manager == "apt" else "gcc",
        "cmake": "cmake",
        "openssl": "openssl libssl-dev" if package_manager == "apt" else "openssl",
        "nghttp2": "libnghttp2-dev" if package_manager == "apt" else "nghttp2"
    }

    for name, pkg in deps.items():
        if not check_command_exists(name.split()[0]):  
            print_colored(f"Installing {name}...")
            try:
                if package_manager == "apt":
                    subprocess.run(["sudo", "apt", "update"], check=True)
                    subprocess.run(["sudo", "apt", "install", "-y", pkg], check=True)
                elif package_manager == "brew":
                    subprocess.run(["brew", "install", pkg], check=True)
            except subprocess.CalledProcessError as e:
                print_colored(f"Failed to install {name}: {e}", RED)
                sys.exit(1)
        else:
            print_colored(f"{name} already installed.")

    print_colored("Dependencies installed successfully", GREEN)


def generate_certificates():
    """Generate self-signed certificates if they don’t exist."""
    if not os.path.exists(CERT_FILE) or not os.path.exists(KEY_FILE):
        print_colored("Generating self-signed certificate and key...")
        try:
            subprocess.run([
                "openssl", "req", "-x509", "-newkey", "rsa:2048",
                "-keyout", KEY_FILE, "-out", CERT_FILE, "-days", "365", "-nodes",
                "-subj", "/C=IN/ST=WB/L=Ok/O=test/OU=test/CN=blaze"
            ], check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            print_colored(f"Certificates generated: {CERT_FILE}, {KEY_FILE}", GREEN)
        except subprocess.CalledProcessError as e:
            print_colored(f"Failed to generate certificates: {e}", RED)
            sys.exit(1)
    else:
        print_colored(f"Certificates already exist: {CERT_FILE}, {KEY_FILE}")


def setup_static_directory():
    """Create static directory and default index.html if missing."""
    static_path = Path(STATIC_ROOT)
    index_file = static_path / "index.html"

    if not static_path.exists():
        print_colored(f"Creating static directory: {STATIC_ROOT}")
        static_path.mkdir(parents=True)
        with index_file.open("w") as f:
            f.write("Hello, World!")
        print_colored("Static directory created with default index.html", GREEN)
    else:
        print_colored(f"Static directory already exists: {STATIC_ROOT}")
        if not index_file.exists():
            with index_file.open("w") as f:
                f.write("Hello, World!")
            print_colored("Added default index.html to static directory", GREEN)


def prepare_build():
    """Prepare the build environment with CMake."""
    build_path = Path(BUILD_DIR)
    print_colored("Preparing build environment...")

    if not build_path.exists():
        build_path.mkdir(parents=True)

    try:
        subprocess.run(["cmake", ".."], cwd=BUILD_DIR, check=True)
        print_colored("Build environment prepared successfully", GREEN)
    except subprocess.CalledProcessError as e:
        print_colored(f"CMake configuration failed: {e}", RED)
        sys.exit(1)


def main():
    """Main setup function."""
    print_colored("Setting up BlazeHTTP server environment...")

    install_dependencies()
    generate_certificates()
    setup_static_directory()
    prepare_build()

    print_colored("Setup completed successfully!", GREEN)
    print_colored("To build and start the server, run './start_server.sh' or manually execute:")
    print_colored("  cd build && make && ./http_server")


if __name__ == "__main__":
    main()