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

OUT_EXE="$FILE_DIR/.build_$$.exe"
g++ -std=c++17 -I"$PROJECT_ROOT" "${EXTRA_INCLUDES[@]}" "$ABS_FILE" "${DEPS[@]}" "${EXTRA_SOURCES[@]}" -o "$OUT_EXE" "${EXTRA_LIBS[@]}"

cd "$PROJECT_ROOT"
"$OUT_EXE"
STATUS=$?

rm -f "$OUT_EXE"
exit $STATUS
