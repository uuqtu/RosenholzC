# ============================================================
# shell.nix — Rosenholz PM Development Environment
#
# Usage:
#   nix-shell          # enter dev shell
#   nix-shell --run "cmake -B build -S . && cmake --build build"
#
# Provides all dependencies to build, test and run Rosenholz PM:
#   C++17, CMake, SQLite3, LMDB, OpenSSL, nlohmann_json,
#   readline (CLI), Python3 (schema embedding), pkg-config
# ============================================================
{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  name = "rosenholz-pm";

  buildInputs = with pkgs; [
    # ── Toolchain ───────────────────────────────────────────
    gcc                   # C++17 compiler
    cmake                 # build system (>= 3.16 required)
    gnumake               # make backend for cmake
    pkg-config            # locate system libraries

    # ── Core runtime dependencies ───────────────────────────
    sqlite                # SQLite3 — primary structured storage
    lmdb                  # LMDB — binary archive store (file content)
    openssl               # OpenSSL — SHA-256 hashing for file integrity

    # ── JSON ────────────────────────────────────────────────
    nlohmann_json         # header-only JSON library

    # ── CLI ─────────────────────────────────────────────────
    readline              # line-editing and history for the CLI

    # ── Build tools ─────────────────────────────────────────
    python3               # SQL schema embedding (scripts/gen_schema_files.py)

    # ── Development utilities ────────────────────────────────
    gdb                   # debugger
    valgrind              # memory analysis
    git                   # version control
  ];

  # Environment variables for the development shell
  shellHook = ''
    echo ""
    echo "  ╔══════════════════════════════════════════╗"
    echo "  ║       Rosenholz PM — Dev Shell           ║"
    echo "  ║       C++17 · SQLite · LMDB · OpenSSL   ║"
    echo "  ╚══════════════════════════════════════════╝"
    echo ""
    echo "  Quick start:"
    echo "    cmake -B build -S .    # configure"
    echo "    cmake --build build    # compile"
    echo "    ./build/rosenholz_cli  # run CLI"
    echo "    ./build/rosenholz_test # run tests"
    echo ""

    # Put the built binaries on PATH if they exist
    if [ -d "$PWD/build" ]; then
      export PATH="$PWD/build:$PATH"
    fi

    # Default settings file for the dev environment
    export ROSENHOLZ_SETTINGS="$PWD/settings.json"
  '';
}
