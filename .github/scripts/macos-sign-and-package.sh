#!/usr/bin/env bash
# Sign FrancDelay.vst3 and FrancDelay.app, optionally notarize them, then zip
# both bundles with ditto so the bundle structure and signature survive.
#
# Developer ID signing runs when MACOS_CERTIFICATE_P12, MACOS_CERTIFICATE_PASSWORD,
# and MACOS_SIGNING_IDENTITY are all set. Notarization runs only after that, when
# APPLE_ID, APPLE_TEAM_ID, and APPLE_APP_PASSWORD are also set. With none of the
# signing secrets set, both bundles are ad-hoc signed and the script succeeds.
# A partial set of either group fails, so a typo is not published as an ad-hoc build.
#
# The script stays compatible with the bash 3.2 that ships on macOS runners.

set -euo pipefail

if [[ $# -ne 4 ]]; then
  echo "usage: $0 <FrancDelay.vst3> <FrancDelay.app> <output.zip> <entitlements.plist>" >&2
  exit 2
fi

VST3=$1
APP=$2
OUTPUT_ZIP=$3
ENTITLEMENTS=$4

KEYCHAIN=""
P12_PATH=""

cleanup() {
  if [[ -n "${KEYCHAIN:-}" ]]; then
    security delete-keychain "$KEYCHAIN" >/dev/null 2>&1 || true
    KEYCHAIN=""
  fi
  if [[ -n "${P12_PATH:-}" ]]; then
    rm -f "$P12_PATH"
    P12_PATH=""
  fi
}
trap cleanup EXIT

require_dir() {
  if [[ ! -d "$1" ]]; then
    echo "Missing bundle: $1" >&2
    exit 1
  fi
}

require_dir "$VST3"
require_dir "$APP"
if [[ ! -f "$ENTITLEMENTS" ]]; then
  echo "Missing entitlements plist: $ENTITLEMENTS" >&2
  exit 1
fi

main_binary() {
  local bundle=$1
  local macos_dir="$bundle/Contents/MacOS"
  local candidate=""
  if [[ ! -d "$macos_dir" ]]; then
    echo "No Contents/MacOS directory in $bundle" >&2
    exit 1
  fi
  for candidate in "$macos_dir"/*; do
    if [[ -f "$candidate" && ! -L "$candidate" ]]; then
      printf '%s\n' "$candidate"
      return 0
    fi
  done
  echo "No executable in $macos_dir" >&2
  exit 1
}

require_universal() {
  local bin=$1
  local archs minos padded
  archs=$(lipo -archs "$bin")
  padded=" $archs "
  echo "$bin architectures: $archs"
  if ! printf '%s' "$padded" | grep -q ' arm64 '; then
    echo "Expected arm64 in $bin" >&2
    exit 1
  fi
  if ! printf '%s' "$padded" | grep -q ' x86_64 '; then
    echo "Expected x86_64 in $bin" >&2
    exit 1
  fi
  minos=$(otool -l "$bin" | awk '/minos / { print $2; exit }')
  echo "$bin deployment target: ${minos:-unknown}"
  if [[ "${minos:-}" != "11.0" ]]; then
    echo "Expected macOS deployment target 11.0" >&2
    exit 1
  fi
}

require_universal "$(main_binary "$VST3")"
require_universal "$(main_binary "$APP")"

signing_present=0
signing_missing=""
if [[ -n "${MACOS_CERTIFICATE_P12:-}" ]]; then
  signing_present=$((signing_present + 1))
else
  signing_missing="$signing_missing MACOS_CERTIFICATE_P12"
fi
if [[ -n "${MACOS_CERTIFICATE_PASSWORD:-}" ]]; then
  signing_present=$((signing_present + 1))
else
  signing_missing="$signing_missing MACOS_CERTIFICATE_PASSWORD"
fi
if [[ -n "${MACOS_SIGNING_IDENTITY:-}" ]]; then
  signing_present=$((signing_present + 1))
else
  signing_missing="$signing_missing MACOS_SIGNING_IDENTITY"
fi
if [[ "$signing_present" -gt 0 && "$signing_present" -lt 3 ]]; then
  echo "macOS signing secrets are only partly set. Missing:$signing_missing" >&2
  exit 1
fi

notary_present=0
notary_missing=""
if [[ -n "${APPLE_ID:-}" ]]; then
  notary_present=$((notary_present + 1))
else
  notary_missing="$notary_missing APPLE_ID"
fi
if [[ -n "${APPLE_TEAM_ID:-}" ]]; then
  notary_present=$((notary_present + 1))
else
  notary_missing="$notary_missing APPLE_TEAM_ID"
fi
if [[ -n "${APPLE_APP_PASSWORD:-}" ]]; then
  notary_present=$((notary_present + 1))
else
  notary_missing="$notary_missing APPLE_APP_PASSWORD"
fi
if [[ "$notary_present" -gt 0 && "$notary_present" -lt 3 ]]; then
  echo "Apple notarization secrets are only partly set. Missing:$notary_missing" >&2
  exit 1
fi
if [[ "$notary_present" -eq 3 && "$signing_present" -ne 3 ]]; then
  echo "Notarization secrets are set but the Developer ID certificate secrets are not." >&2
  exit 1
fi

IDENTITY="-"
USE_TIMESTAMP=0

if [[ "$signing_present" -eq 3 ]]; then
  echo "Importing Developer ID certificate into a temporary keychain."
  KEYCHAIN="${RUNNER_TEMP:-/tmp}/francdelay-signing.keychain-db"
  P12_PATH="${RUNNER_TEMP:-/tmp}/francdelay-developer-id.p12"
  KEYCHAIN_PASSWORD=$(openssl rand -base64 32)
  printf '%s' "$MACOS_CERTIFICATE_P12" | tr -d '[:space:]' | base64 -D >"$P12_PATH"
  if [[ ! -s "$P12_PATH" ]]; then
    echo "MACOS_CERTIFICATE_P12 did not decode to a certificate." >&2
    exit 1
  fi

  security create-keychain -p "$KEYCHAIN_PASSWORD" "$KEYCHAIN"
  security set-keychain-settings -lut 21600 "$KEYCHAIN"
  security unlock-keychain -p "$KEYCHAIN_PASSWORD" "$KEYCHAIN"
  security list-keychains -d user -s "$KEYCHAIN"
  security default-keychain -s "$KEYCHAIN"
  security import "$P12_PATH" -k "$KEYCHAIN" -P "$MACOS_CERTIFICATE_PASSWORD" -f pkcs12 -A -t cert
  rm -f "$P12_PATH"
  P12_PATH=""
  security set-key-partition-list -S apple-tool:,apple:,codesign: -s -k "$KEYCHAIN_PASSWORD" "$KEYCHAIN"
  unset KEYCHAIN_PASSWORD

  if ! security find-identity -v -p codesigning "$KEYCHAIN" | grep -F -- "$MACOS_SIGNING_IDENTITY" >/dev/null; then
    echo "MACOS_SIGNING_IDENTITY does not match a codesigning certificate in the imported .p12." >&2
    echo "Identities found:" >&2
    security find-identity -v -p codesigning "$KEYCHAIN" >&2 || true
    exit 1
  fi

  IDENTITY=$MACOS_SIGNING_IDENTITY
  USE_TIMESTAMP=1
  echo "::notice::Signing with Developer ID and a temporary keychain."
else
  echo "::notice::Apple signing secrets are not set. Ad-hoc signing both bundles."
fi

is_macho() {
  local info
  info=$(file -b "$1" || true)
  [[ "$info" == *Mach-O* ]]
}

# Inside-out signature. Nested code is signed first. The outer bundle is signed
# last, which is what Apple documents instead of codesign --deep.
codesign_one() {
  local target=$1
  local entitlements=${2:-}
  if [[ "$USE_TIMESTAMP" -eq 1 ]]; then
    if [[ -n "$entitlements" ]]; then
      codesign --force --sign "$IDENTITY" --options runtime --timestamp \
        --keychain "$KEYCHAIN" \
        --entitlements "$entitlements" --generate-entitlement-der \
        "$target"
    else
      codesign --force --sign "$IDENTITY" --options runtime --timestamp \
        --keychain "$KEYCHAIN" \
        "$target"
    fi
  else
    if [[ -n "$entitlements" ]]; then
      codesign --force --sign - --options runtime \
        --entitlements "$entitlements" --generate-entitlement-der \
        "$target"
    else
      codesign --force --sign - --options runtime "$target"
    fi
  fi
  echo "codesign $target"
}

sign_bundle() {
  local bundle=$1
  local entitlements=${2:-}
  local item=""
  local base=""
  while IFS= read -r -d '' item; do
    [[ "$item" == "$bundle" ]] && continue
    # The bundle signature covers the main executable and applies its entitlements.
    if [[ "$item" == "$bundle/Contents/MacOS/"* && -f "$item" ]]; then
      continue
    fi
    base=$(basename "$item")
    if [[ -d "$item" ]]; then
      case "$base" in
        *.framework|*.app|*.xpc|*.bundle|*.vst3|*.appex|*.plugin)
          echo "codesign nested $item"
          codesign_one "$item" ""
          ;;
      esac
    elif is_macho "$item"; then
      echo "codesign nested $item"
      codesign_one "$item" ""
    fi
  done < <(find "$bundle" -depth -print0)

  echo "codesign bundle $bundle"
  codesign_one "$bundle" "$entitlements"
  codesign --verify --deep --strict --verbose=2 "$bundle"
  if ! codesign -dv --verbose=4 "$bundle" 2>&1 | grep -q 'runtime'; then
    echo "Hardened runtime flag missing on $bundle" >&2
    exit 1
  fi
}

sign_bundle "$VST3" ""
sign_bundle "$APP" "$ENTITLEMENTS"

if ! codesign -d --entitlements :- "$APP" 2>/dev/null | grep -q 'com.apple.security.device.audio-input'; then
  echo "Standalone app is missing the audio-input entitlement." >&2
  exit 1
fi

# notarytool uses the Apple ID password, not the signing keychain.
cleanup

if [[ "$notary_present" -eq 3 ]]; then
  NOTARY_ZIP="${RUNNER_TEMP:-/tmp}/FrancDelay-notarize.zip"
  rm -f "$NOTARY_ZIP"
  echo "Submitting bundles to notarytool."
  ditto -c -k --keepParent "$VST3" "$APP" "$NOTARY_ZIP"

  set +e
  NOTARY_OUTPUT=$(xcrun notarytool submit "$NOTARY_ZIP" \
    --apple-id "$APPLE_ID" \
    --team-id "$APPLE_TEAM_ID" \
    --password "$APPLE_APP_PASSWORD" \
    --wait \
    --timeout 30m 2>&1)
  NOTARY_STATUS=$?
  set -e
  printf '%s\n' "$NOTARY_OUTPUT"
  if [[ "$NOTARY_STATUS" -ne 0 ]]; then
    SUBMISSION_ID=$(printf '%s\n' "$NOTARY_OUTPUT" | awk '/id: / { print $2; exit }')
    if [[ -n "${SUBMISSION_ID:-}" ]]; then
      xcrun notarytool log "$SUBMISSION_ID" \
        --apple-id "$APPLE_ID" \
        --team-id "$APPLE_TEAM_ID" \
        --password "$APPLE_APP_PASSWORD" || true
    fi
    exit "$NOTARY_STATUS"
  fi

  staple() {
    local bundle=$1
    local attempt
    for attempt in 1 2 3 4 5 6 7 8 9 10; do
      if xcrun stapler staple "$bundle"; then
        xcrun stapler validate "$bundle"
        return 0
      fi
      echo "stapler attempt ${attempt} failed for $bundle; waiting for the ticket."
      sleep 20
    done
    echo "Could not staple $bundle" >&2
    return 1
  }

  staple "$VST3"
  staple "$APP"
  echo "::notice::Notarized and stapled both bundles."
else
  echo "::notice::Notarization secrets are not set. Skipping notarytool and stapler."
fi

mkdir -p "$(dirname "$OUTPUT_ZIP")"
rm -f "$OUTPUT_ZIP"
ditto -c -k --keepParent "$VST3" "$APP" "$OUTPUT_ZIP"

listing=$(unzip -l "$OUTPUT_ZIP")
printf '%s\n' "$listing"
printf '%s\n' "$listing" | grep -q 'FrancDelay.vst3/' || {
  echo "Zip is missing the VST3 bundle." >&2
  exit 1
}
printf '%s\n' "$listing" | grep -q 'FrancDelay.app/' || {
  echo "Zip is missing the app bundle." >&2
  exit 1
}
echo "Wrote $OUTPUT_ZIP"
