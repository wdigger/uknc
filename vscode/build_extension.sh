#!/usr/bin/env sh
set -e

# Builds the VS Code extension into a .vsix and installs it.
#
#   cd vscode && sh ./build_extension.sh
#
# The toolchain it was built from is written into the package, so the
# extension knows where everything is without being told.  That is only
# the last resort, mind: it looks above the project folder first, then
# along PATH, and the uknc.toolchainPath setting beats all of them --
# move the toolchain and rebuild, or set that, whichever is easier.
#
# Needs node and npm; vsce is fetched by npx and not kept.

HERE=$(cd "$(dirname "$0")" && pwd)
UKNC=$(dirname "${HERE}")

for f in gcc/bin/pdp11-uknc-rt11-gcc gcc/bin/pdp11-uknc-rt11-gdb \
         debugger/bin/ukncbtldebug rom/uknc_rom_autoboot.bin \
         resources/rt11os.dsk gcc/dejagnu/uknc-run; do
	if [ ! -e "${UKNC}/${f}" ]; then
		echo "build_extension: missing ${UKNC}/${f}" >&2
		echo "  build the toolchain (gcc/build_gcc_uknc.sh) and the" >&2
		echo "  emulator (debugger/build_debugger_uknc.sh) first." >&2
		exit 1
	fi
done

# Packaged in a copy, so that a path belonging to this machine never
# lands in the repository.
BUILD=$(mktemp -d "${TMPDIR:-/tmp}/uknc-vsix.XXXXXX")
trap 'rm -rf "${BUILD}"' EXIT INT TERM

cp "${HERE}/package.json" "${HERE}/extension.js" "${HERE}/config.js" \
   "${HERE}/README.md" "${BUILD}/"
cat > "${BUILD}/packaged.json" <<EOF
{ "toolchain": "${UKNC}" }
EOF

# The extension is four files; everything else in the build directory is
# scaffolding npm left behind and must stay out of the package.
cat > "${BUILD}/.vscodeignore" <<'EOF'
**
!package.json
!extension.js
!config.js
!packaged.json
!README.md
EOF

cd "${BUILD}"
# Its own npm cache, in the same temporary directory: a shared one that
# has been written to as root once -- which happens -- otherwise stops
# this with a permission error that has nothing to do with the build.
npm_config_cache="${BUILD}/.npm" \
npx --yes @vscode/vsce package --allow-missing-repository --skip-license \
    -o "${HERE}/uknc.vsix"

echo
echo "Собрано: ${HERE}/uknc.vsix  (тулчейн: ${UKNC})"
echo "Установить:"
echo "  code --install-extension ${HERE}/uknc.vsix"
