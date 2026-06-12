#!/usr/bin/env bash
set -euo pipefail

REPO="Sambit-Kumar-Mohanty-26/PerfGuardian"
BIN_DIR="${INSTALL_DIR:-/usr/local/bin}"
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

# detect platform
OS="$(uname -s)"
ARCH="$(uname -m)"

case "${OS}" in
  Linux)
    case "${ARCH}" in
      x86_64)  ASSET="perfguardian-linux-x86_64.tar.gz" ;;
      *)        echo "Unsupported architecture: ${ARCH}"; exit 1 ;;
    esac ;;
  Darwin)
    case "${ARCH}" in
      arm64)   ASSET="perfguardian-macos-arm64.tar.gz"  ;;
      x86_64)  ASSET="perfguardian-macos-x86_64.tar.gz" ;;
      *)        echo "Unsupported architecture: ${ARCH}"; exit 1 ;;
    esac ;;
  *)
    echo "Unsupported OS: ${OS}"; exit 1 ;;
esac

# resolve latest version
VERSION="${PERFGUARDIAN_VERSION:-}"
if [ -z "$VERSION" ]; then
  VERSION="$(curl -fsSL "https://api.github.com/repos/${REPO}/releases/latest" \
    | grep '"tag_name"' | sed 's/.*"tag_name": *"\(.*\)".*/\1/')"
fi

echo "Installing PerfGuardian ${VERSION} (${OS}/${ARCH}) ..."

# download + verify
BASE_URL="https://github.com/${REPO}/releases/download/${VERSION}"
curl -fsSL "${BASE_URL}/${ASSET}"        -o "${TMP_DIR}/${ASSET}"
curl -fsSL "${BASE_URL}/${ASSET}.sha256" -o "${TMP_DIR}/${ASSET}.sha256"

cd "$TMP_DIR"
if command -v sha256sum &>/dev/null; then
  sha256sum -c "${ASSET}.sha256"
else
  shasum -a 256 -c "${ASSET}.sha256"
fi

# install
tar xzf "${ASSET}"

SUDO=""
if [ ! -w "$BIN_DIR" ]; then
  SUDO="sudo"
fi

${SUDO} install -m 755 perfguardian "${BIN_DIR}/perfguardian"

echo ""
echo "Installed: $(which perfguardian)"
perfguardian --version
