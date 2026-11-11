#!/usr/bin/env bash
# Sign FrancDelay macOS bundles.
#
#   macos-sign.sh adhoc <bundle> [bundle...]
#   macos-sign.sh developer-id <bundle> [bundle...]
#
# adhoc: codesign --sign - so the bundles are loadable without a certificate.
# developer-id: import MACOS_CERTIFICATE_P12 into a temporary keychain, sign
# with the hardened runtime and a secure timestamp, notarize with notarytool,
# then staple. Requires MACOS_CERTIFICATE_P12, MACOS_CERTIFICATE_PASSWORD,
# MACOS_SIGNING_IDENTITY, APPLE_ID, APPLE_TEAM_ID, and APPLE_APP_PASSWORD.

set -euo pipefail

if [[ $# -lt 2 ]]; then
    echo "usage: macos-sign.sh adhoc|developer-id <bundle> [bundle...]" >&2
    exit 2
fi

mode="$1"
shift

case "$mode" in
    adhoc|developer-id) ;;
    *)
        echo "unknown mode: $mode" >&2
        exit 2
        ;;
esac

for bundle in "$@"; do
    if [[ ! -d "$bundle" ]]; then
        echo "bundle not found: $bundle" >&2
        exit 1
    fi
done

script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
entitlements="$script_dir/../macos/entitlements.plist"

sign_nested() {
    local bundle="$1"
    shift
    local file
    # Deepest paths first, so a parent is sealed after its contents.
    while IFS= read -r file; do
        if file -b "$file" | grep -q "Mach-O"; then
            codesign --force "$@" "$file"
        fi
    done < <(find "$bundle" -type f -print | awk '{ print length($0) "\t" $0 }' | sort -nr | cut -f2-)
    codesign --force "$@" "$bundle"
    codesign --verify --deep --strict --verbose=2 "$bundle"
}

if [[ "$mode" == "adhoc" ]]; then
    for bundle in "$@"; do
        echo "Ad-hoc signing $bundle"
        sign_nested "$bundle" --sign -
    done
    exit 0
fi

for name in MACOS_CERTIFICATE_P12 MACOS_CERTIFICATE_PASSWORD MACOS_SIGNING_IDENTITY APPLE_ID APPLE_TEAM_ID APPLE_APP_PASSWORD; do
    if [[ -z "${!name:-}" ]]; then
        echo "developer-id signing requires $name" >&2
        exit 1
    fi
done

keychain_path="${RUNNER_TEMP:-/tmp}/francdelay-signing.keychain-db"
keychain_password=$(openssl rand -base64 32)
cert_path="${RUNNER_TEMP:-/tmp}/francdelay-certificate.p12"
notarize_dir=$(mktemp -d)

cleanup() {
    security delete-keychain "$keychain_path" >/dev/null 2>&1 || true
    rm -f "$cert_path"
    rm -rf "$notarize_dir"
}
trap cleanup EXIT

printf '%s' "$MACOS_CERTIFICATE_P12" | tr -d '[:space:]' | base64 -D > "$cert_path"

security create-keychain -p "$keychain_password" "$keychain_path"
security set-keychain-settings -lut 21600 "$keychain_path"
security unlock-keychain -p "$keychain_password" "$keychain_path"
security import "$cert_path" -P "$MACOS_CERTIFICATE_PASSWORD" -A -t cert -f pkcs12 -k "$keychain_path"
security set-key-partition-list -S apple-tool:,apple:,codesign: -s -k "$keychain_password" "$keychain_path"
security list-keychains -d user -s "$keychain_path" $(security list-keychains -d user | tr -d '"')
security default-keychain -s "$keychain_path"

echo "Signing with identity: $MACOS_SIGNING_IDENTITY"
for bundle in "$@"; do
    sign_nested "$bundle" \
        --options runtime \
        --timestamp \
        --sign "$MACOS_SIGNING_IDENTITY" \
        --entitlements "$entitlements"
done

notarize_zip="$notarize_dir/notarize.zip"
ditto -c -k --keepParent "$@" "$notarize_zip"
echo "Submitting $(basename "$notarize_zip") to notarytool"
xcrun notarytool submit "$notarize_zip" \
    --apple-id "$APPLE_ID" \
    --team-id "$APPLE_TEAM_ID" \
    --password "$APPLE_APP_PASSWORD" \
    --wait

for bundle in "$@"; do
    stapled=false
    for attempt in 1 2 3 4 5; do
        if xcrun stapler staple "$bundle"; then
            stapled=true
            break
        fi
        echo "stapler could not find a ticket for $bundle yet (attempt $attempt)" >&2
        sleep 20
    done
    if [[ "$stapled" != true ]]; then
        echo "stapler failed for $bundle" >&2
        exit 1
    fi
    xcrun stapler validate "$bundle"
done
