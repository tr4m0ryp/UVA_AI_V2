#!/bin/bash
# Setup script for a fresh VPS (Ubuntu/Debian).
# Run this on the VPS to install Docker and build the codex image.
#
# Usage:
#   scp -r services/proxy/docker/ root@VPS_IP:/opt/uva-codex/
#   ssh root@VPS_IP 'bash /opt/uva-codex/setup-vps.sh'
set -e

echo "=== UvA Cloud Coding -- VPS Setup ==="

# Install Docker if not present
if ! command -v docker &>/dev/null; then
    echo "Installing Docker..."
    apt-get update
    apt-get install -y ca-certificates curl gnupg
    install -m 0755 -d /etc/apt/keyrings
    curl -fsSL https://download.docker.com/linux/ubuntu/gpg \
        | gpg --dearmor -o /etc/apt/keyrings/docker.gpg
    chmod a+r /etc/apt/keyrings/docker.gpg

    echo "deb [arch=$(dpkg --print-architecture) \
        signed-by=/etc/apt/keyrings/docker.gpg] \
        https://download.docker.com/linux/ubuntu \
        $(. /etc/os-release && echo "$VERSION_CODENAME") stable" \
        > /etc/apt/sources.list.d/docker.list

    apt-get update
    apt-get install -y docker-ce docker-ce-cli containerd.io
    systemctl enable docker
    systemctl start docker
    echo "Docker installed."
else
    echo "Docker already installed."
fi

# Build the codex image
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
echo "Building uva-codex:latest from ${SCRIPT_DIR}..."
docker build -t uva-codex:latest "${SCRIPT_DIR}"

echo ""
echo "=== Setup complete ==="
echo ""
echo "Add these to your proxy.env:"
echo "  VPS_HOST=root@$(hostname -I | awk '{print $1}')"
echo "  VPS_SSH_KEY=~/.ssh/id_ed25519"
echo "  VPS_DOCKER_IMAGE=uva-codex:latest"
