# shell.nix — Nix development shell for Rosenholz PM
#
# Usage:
#   nix-shell            # enter the dev shell
#   nix-shell --run "cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build -j$(nproc)"
#
# Provides:
#   - GCC + Clang (choose either)
#   - CMake (default build system)
#   - SQLite3 (library + headers)
#   - nlohmann-json (header-only, C++17 JSON)
#   - pkg-config (used by CMakeLists.txt)
#   - curl + wget (used by FileOps::downloadUrl)
#   - wkhtmltopdf (used by Document::archiveWebsite for PDF archiving)
#   - gdb + valgrind (debugging)
#   - clang-tools (clangd LSP, clang-format, clang-tidy)
#   - git (for GitHub state writing feature)
#   - zip/unzip (for packaging)

{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  name = "rosenholz-dev";

  # ── Build tools ────────────────────────────────────────────────
  nativeBuildInputs = with pkgs; [
    # Compilers — both available, CMake will prefer GCC by default.
    # To use clang: CC=clang CXX=clang++ cmake -B build
    gcc
    clang

    # Build system
    cmake
    # ninja       # optional: faster incremental builds
    pkg-config
    gnumake        # cmake --build delegates to make by default
  ];

  # ── Runtime / link-time libraries ──────────────────────────────
  buildInputs = with pkgs; [
    # Core dependencies
    sqlite         # libsqlite3 + sqlite3.h
    nlohmann_json  # header-only JSON (nlohmann/json.hpp)
    lmdb           # Lightning Memory-Mapped Database (file archive store)
    openssl        # SHA-256 for content-addressed chunk storage
    readline       # GNU Readline: Ctrl+C handling, tab-completion, history

    # Download & archiving (used by FileOps + Document)
    curl
    wget
    wkhtmltopdf    # website → PDF archiving (Document::archiveWebsite)

    # Debugging
    gdb
    valgrind

    # LSP + formatting (works with clangd in VS Code / neovim)
    clang-tools    # clangd, clang-format, clang-tidy

    # Utilities
    git
    zip
    unzip
    ripgrep        # fast codebase search
    jq             # inspect the JSON settings files + DB JSON fields
  ];

  # ── Shell hook ─────────────────────────────────────────────────
  shellHook = ''
    echo ""
    echo "  Rosenholz PM — Nix dev shell"
    echo "  ──────────────────────────────────────────────"
    echo "  Compiler : $(g++ --version | head -1)"
    echo "  CMake    : $(cmake --version | head -1)"
    echo "  SQLite   : $(sqlite3 --version)"
    echo "  LMDB     : available (file archive)"
    echo ""
    echo "  Quick start:"
    echo "    cmake -B build -DCMAKE_BUILD_TYPE=Debug"
    echo "    cmake --build build -j\$(nproc)"
    echo "    ./build/rosenholz --basepath ~/rosenholz-data"
    echo ""
    echo "  Release build:"
    echo "    cmake -B build-rel -DCMAKE_BUILD_TYPE=Release"
    echo "    cmake --build build-rel -j\$(nproc)"
    echo ""
    echo "  Clang build:"
    echo "    CC=clang CXX=clang++ cmake -B build-clang"
    echo "    cmake --build build-clang -j\$(nproc)"
    echo ""

    # Convenience aliases
    alias rhbuild='cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build -j$(nproc)'
    alias rhtest='./build/rosenholz --basepath /tmp/rh-test-$(date +%s)'
    alias rhclean='rm -rf build && echo "build/ removed"'
  '';

  # ── Environment variables ──────────────────────────────────────
  # Tell CMake where nlohmann_json lives so find_package() succeeds
  # without falling back to FetchContent (which needs network in Nix)
  CMAKE_PREFIX_PATH = "${pkgs.nlohmann_json}";

  # C++17 standard enforced
  NIX_CFLAGS_COMPILE = "-std=c++17";
}
