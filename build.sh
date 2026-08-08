#!/usr/bin/env bash
# Global build+run for AZBacktest. Callable from anywhere in the project.
#
# Usage:
#   build.sh -runfile <path>
#     <path> is resolved relative to the current directory (where you invoked the script).
#   build.sh -clean
#     Sweep leftover .build_*.exe artifacts from interrupted runs across the project.
#
# Behavior:
#   - Auto-discovers sibling .cpp files matching #include "X.h" directives and links them.
#   - Adds the project root to the include path so csvConfig.h is available anywhere.
#   - Runs the binary from the target file's own directory so its relative paths work.
#   - Deletes the built executable when done.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR"
INVOKE_DIR="$(pwd)"

RUNFILE=""
CLEAN=0
while [[ $# -gt 0 ]]; do
    case "$1" in
        -runfile)
            RUNFILE="$2"
            shift 2
            ;;
        -clean)
            CLEAN=1
            shift
            ;;
        *)
            echo "unknown arg: $1" >&2
            echo "usage: build.sh -runfile <path> | -clean" >&2
            exit 1
            ;;
    esac
done

if [[ $CLEAN -eq 1 ]]; then
    count=0
    while IFS= read -r -d '' f; do
        rm -f "$f"
        count=$((count + 1))
    done < <(find "$PROJECT_ROOT" -type f -name '.build_*.exe' -print0)
    echo "removed $count build artifact(s)"
    exit 0
fi

if [[ -z "$RUNFILE" ]] && [[ $CLEAN -eq 0 ]]; then
    bash "$SCRIPT_DIR/amalgamate.sh"
    exit 0
fi

# Resolve to absolute path, relative to invocation directory.
if [[ "$RUNFILE" = /* ]] || [[ "$RUNFILE" =~ ^[a-zA-Z]: ]]; then
    ABS_FILE="$RUNFILE"
else
    ABS_FILE="$INVOKE_DIR/$RUNFILE"
fi

if [[ ! -f "$ABS_FILE" ]]; then
    echo "file not found: $ABS_FILE" >&2
    exit 1
fi

FILE_DIR="$(cd "$(dirname "$ABS_FILE")" && pwd)"
DEPS=()

SCANNED=()
collect_deps() {
    local file="$1"
    local dir
    dir="$(cd "$(dirname "$file")" && pwd)"

    for s in "${SCANNED[@]}"; do
        if [[ "$s" = "$file" ]]; then return; fi
    done
    SCANNED+=("$file")

    while IFS= read -r line; do
        if [[ "$line" =~ ^[[:space:]]*#include[[:space:]]*\"([^\"]+)\" ]]; then
            local inc="${BASH_REMATCH[1]}"
            local inc_path="$dir/$inc"
            if [[ -f "$inc_path" ]]; then
                collect_deps "$inc_path"
            fi
            local cpp_path
            cpp_path="$(cd "$(dirname "${inc_path%.h}.cpp")" 2>/dev/null && echo "$(pwd)/$(basename "${inc_path%.h}.cpp")" || true)"
            if [[ -f "$cpp_path" ]] && [[ "$cpp_path" != "$ABS_FILE" ]]; then
                local already=0
                for d in "${DEPS[@]}"; do
                    if [[ "$d" = "$cpp_path" ]]; then already=1; break; fi
                done
                if [[ $already -eq 0 ]]; then
                    DEPS+=("$cpp_path")
                    collect_deps "$cpp_path"
                fi
            fi
        fi
    done < "$file"
}

collect_deps "$ABS_FILE"

# Detect vendored ImGui usage anywhere in the target or its deps.
EXTRA_INCLUDES=()
EXTRA_SOURCES=()
EXTRA_LIBS=()
EXTRA_DEFINES=()
uses_imgui=0
for f in "${SCANNED[@]}"; do
    if grep -qE '^\s*#\s*include\s*[<"]imgui\.h[>"]' "$f"; then
        uses_imgui=1; break
    fi
done
if [[ $uses_imgui -eq 1 ]]; then
    EXTRA_INCLUDES+=(-I"$PROJECT_ROOT/vendor/imgui" -I"$PROJECT_ROOT/vendor/imgui/backends")
    EXTRA_SOURCES+=(
        "$PROJECT_ROOT/vendor/imgui/imgui.cpp"
        "$PROJECT_ROOT/vendor/imgui/imgui_draw.cpp"
        "$PROJECT_ROOT/vendor/imgui/imgui_tables.cpp"
        "$PROJECT_ROOT/vendor/imgui/imgui_widgets.cpp"
        "$PROJECT_ROOT/vendor/imgui/backends/imgui_impl_glfw.cpp"
        "$PROJECT_ROOT/vendor/imgui/backends/imgui_impl_opengl3.cpp"
    )
    # Platform-specific libraries and includes
    if [[ "$OSTYPE" == "darwin"* ]]; then
        # macOS - use Homebrew GLFW (detect correct prefix, with fallback)
        if [[ -f "/opt/homebrew/lib/libglfw.dylib" ]] || [[ -f "/opt/homebrew/lib/libglfw3.a" ]]; then
            BREW_PREFIX="/opt/homebrew"
        elif [[ -f "/usr/local/lib/libglfw.dylib" ]] || [[ -f "/usr/local/lib/libglfw3.a" ]]; then
            BREW_PREFIX="/usr/local"
        else
            echo "Error: GLFW not found. Install with: brew install glfw" >&2
            exit 1
        fi
        EXTRA_INCLUDES+=(-I"$BREW_PREFIX/include")
        EXTRA_LIBS+=(-L"$BREW_PREFIX/lib" -lglfw -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo)
    else
        # Windows (assumes MinGW/MSYS) - use vendored GLFW
        EXTRA_INCLUDES+=(-I"$PROJECT_ROOT/vendor/glfw/include")
        EXTRA_LIBS+=(-L"$PROJECT_ROOT/vendor/glfw/lib" -lglfw3 -lopengl32 -lgdi32 -lshell32 -lwinmm)
    fi
fi

uses_implot=0
for f in "${SCANNED[@]}"; do
    if grep -qE '^\s*#\s*include\s*[<"]implot\.h[>"]' "$f"; then
        uses_implot=1; break
    fi
done
if [[ $uses_implot -eq 1 ]]; then
    EXTRA_INCLUDES+=(-I"$PROJECT_ROOT/vendor/implot")
    EXTRA_SOURCES+=(
        "$PROJECT_ROOT/vendor/implot/implot.cpp"
        "$PROJECT_ROOT/vendor/implot/implot_items.cpp"
    )
fi

# marketData.h always carries the Parquet backend behind #ifdef AZBT_PARQUET
# (both in the src/ tree and inlined into the amalgamated azbacktest.h), so
# detect it by grepping for that macro rather than a specific #include - it
# needs to fire whether the strategy includes marketData.h/backtestApi.h
# directly or just the single-header release build.
uses_parquet=0
for f in "${SCANNED[@]}"; do
    if grep -q 'AZBT_PARQUET' "$f" 2>/dev/null; then
        uses_parquet=1; break
    fi
done
CXX_STD="c++17"
if [[ $uses_parquet -eq 1 ]]; then
    if [[ "$OSTYPE" == "darwin"* ]]; then
        if [[ -f "/opt/homebrew/lib/libarrow.dylib" ]] && [[ -f "/opt/homebrew/lib/libparquet.dylib" ]]; then
            ARROW_PREFIX="/opt/homebrew"
        elif [[ -f "/usr/local/lib/libarrow.dylib" ]] && [[ -f "/usr/local/lib/libparquet.dylib" ]]; then
            ARROW_PREFIX="/usr/local"
        fi
        if [[ -n "${ARROW_PREFIX:-}" ]]; then
            EXTRA_INCLUDES+=(-I"$ARROW_PREFIX/include")
            EXTRA_LIBS+=(-L"$ARROW_PREFIX/lib" -larrow -lparquet)
            EXTRA_DEFINES+=(-DAZBT_PARQUET)
            # Arrow's headers use C++20 library features (std::span,
            # std::popcount); bump the standard only when it's actually
            # linked in so CSV-only builds without Arrow are unaffected.
            CXX_STD="c++20"
        else
            echo "Note: building without Parquet support (Arrow not found). .parquet data files won't work until you: brew install apache-arrow" >&2
        fi
    else
        # Windows (assumes MinGW/MSYS, matching the GLFW branch above). Arrow
        # is too heavy to vendor like GLFW (its own dependency graph pulls in
        # llvm/grpc/aws-sdk-cpp/etc), so look for a vcpkg install instead -
        # specifically a MinGW triplet, since vcpkg's default MSVC-built libs
        # aren't ABI-compatible with g++:
        #   vcpkg install arrow[parquet]:x64-mingw-dynamic
        #
        # UNTESTED on Windows - this is a best-effort guess at vcpkg's layout
        # and triplet naming. If it doesn't find your install, check the
        # candidate paths/triplets below against your actual
        # <vcpkg root>/installed/<triplet> directory.
        VCPKG_ARROW_ROOT=""
        VCPKG_CANDIDATES=("$PROJECT_ROOT/vcpkg" "/c/vcpkg" "/c/tools/vcpkg")
        if [[ -n "${VCPKG_ROOT:-}" ]]; then
            NORMALIZED_VCPKG_ROOT="$VCPKG_ROOT"
            if command -v cygpath >/dev/null 2>&1; then
                NORMALIZED_VCPKG_ROOT="$(cygpath -u "$VCPKG_ROOT" 2>/dev/null || echo "$VCPKG_ROOT")"
            fi
            VCPKG_CANDIDATES=("$NORMALIZED_VCPKG_ROOT" "${VCPKG_CANDIDATES[@]}")
        fi
        for VCPKG_CANDIDATE in "${VCPKG_CANDIDATES[@]}"; do
            # dynamic first: linking just -larrow -lparquet against a static
            # triplet also needs every transitive dep (thrift/snappy/zstd/...)
            # spelled out explicitly, which the dynamic triplet avoids
            for TRIPLET in x64-mingw-dynamic x64-mingw-static; do
                if [[ -f "$VCPKG_CANDIDATE/installed/$TRIPLET/include/arrow/api.h" ]]; then
                    VCPKG_ARROW_ROOT="$VCPKG_CANDIDATE/installed/$TRIPLET"
                    break 2
                fi
            done
        done

        if [[ -n "$VCPKG_ARROW_ROOT" ]]; then
            EXTRA_INCLUDES+=(-I"$VCPKG_ARROW_ROOT/include")
            EXTRA_LIBS+=(-L"$VCPKG_ARROW_ROOT/lib" -larrow -lparquet)
            EXTRA_DEFINES+=(-DAZBT_PARQUET)
            CXX_STD="c++20"
        else
            echo "Note: building without Parquet support (no vcpkg Arrow install found)." >&2
            echo "  Install with: vcpkg install arrow[parquet]:x64-mingw-dynamic" >&2
            echo "  Then either set VCPKG_ROOT, or install vcpkg at C:\\vcpkg." >&2
        fi
    fi
fi

OUT_EXE="$FILE_DIR/.build_$$.exe"
g++ -std="$CXX_STD" -I"$PROJECT_ROOT" "${EXTRA_DEFINES[@]}" "${EXTRA_INCLUDES[@]}" "$ABS_FILE" "${DEPS[@]}" "${EXTRA_SOURCES[@]}" -o "$OUT_EXE" "${EXTRA_LIBS[@]}"

cd "$PROJECT_ROOT"
"$OUT_EXE"
STATUS=$?

rm -f "$OUT_EXE"
exit $STATUS
