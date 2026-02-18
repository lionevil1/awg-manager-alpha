#!/bin/bash
# Create GitHub release and upload IPK package
# Requires: gh CLI or GITHUB_TOKEN environment variable

set -eu

ROOT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
VERSION="0.1.0-18"
IPK_FILE="$ROOT_DIR/dist/awg-manager-alpha_${VERSION}_aarch64-3.10.ipk"
TAG="v${VERSION}"

cd "$ROOT_DIR"

echo "Creating release ${TAG}..."

# Check if gh CLI is available
if command -v gh >/dev/null 2>&1; then
    echo "Using gh CLI..."
    
    # Create release
    gh release create "$TAG" \
        --title "AWG Manager Alpha v${VERSION}" \
        --notes "Security hardening release v${VERSION}

## Changes

### Security Improvements
- JSON string escaping to prevent injection attacks
- Constant-time comparison for session tokens (timing attack protection)
- Rate limiting for /api/login (5 attempts per 5 minutes, 15 min block)
- IPv4 address validation for environment overrides
- Secure memory zeroing with volatile
- Security HTTP headers (CSP, X-Frame-Options, X-XSS-Protection, X-Content-Type-Options)
- Socket leak fixes in tcp_connect error paths

### Tests
- Added constant-time comparison tests
- Added IPv4 validation tests

### Installation
\`\`\`sh
opkg install awg-manager-alpha_${VERSION}_aarch64-3.10.ipk
/opt/etc/init.d/S99awg-manager-alpha start
\`\`\`" \
        --draft \
        "$IPK_FILE"
    
    echo "Release created as draft. Publish at: https://github.com/lionevil1/awg-manager-alpha/releases"
    exit 0
fi

# Fallback: use GitHub API with token
if [ -z "${GITHUB_TOKEN:-}" ]; then
    echo "Error: gh CLI not found and GITHUB_TOKEN not set"
    echo ""
    echo "Options:"
    echo "1. Install gh CLI: https://github.com/cli/cli#installation"
    echo "2. Or set GITHUB_TOKEN environment variable:"
    echo "   export GITHUB_TOKEN=your_token_here"
    echo "   ./scripts/create_release.sh"
    exit 1
fi

REPO="lionevil1/awg-manager-alpha"

# Create release via API
RESPONSE=$(curl -s -X POST \
    -H "Authorization: token ${GITHUB_TOKEN}" \
    -H "Accept: application/vnd.github.v3+json" \
    https://api.github.com/repos/${REPO}/releases \
    -d "{
        \"tag_name\": \"${TAG}\",
        \"name\": \"AWG Manager Alpha v${VERSION}\",
        \"body\": \"Security hardening release v${VERSION}\",
        \"draft\": true
    }")

UPLOAD_URL=$(echo "$RESPONSE" | grep -o '"upload_url": "[^"]*' | cut -d'"' -f4 | sed 's/{?name,label}//')

if [ -z "$UPLOAD_URL" ]; then
    echo "Failed to create release: $RESPONSE"
    exit 1
fi

# Upload IPK
curl -s -X POST \
    -H "Authorization: token ${GITHUB_TOKEN}" \
    -H "Accept: application/vnd.github.v3+json" \
    -H "Content-Type: application/octet-stream" \
    --data-binary @"$IPK_FILE" \
    "${UPLOAD_URL}?name=awg-manager-alpha_${VERSION}_aarch64-3.10.ipk"

echo "Release created: https://github.com/${REPO}/releases"
