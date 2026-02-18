#!/usr/bin/env bash
# Bump Solis S6 UI version in VERSION file. Usage:
#   ./bump_version.sh          # bump patch: 1.0.0 -> 1.0.1
#   ./bump_version.sh minor   # bump minor: 1.0.1 -> 1.1.0
#   ./bump_version.sh major   # bump major: 1.1.0 -> 2.0.0
set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VERSION_FILE="$SCRIPT_DIR/VERSION"
kind="${1:-patch}"

if [[ ! -f "$VERSION_FILE" ]]; then
  echo "1.0.0" > "$VERSION_FILE"
fi
current=$(cat "$VERSION_FILE" | tr -d '[:space:]')
IFS=. read -r major minor patch <<< "$current"
patch="${patch:-0}"
minor="${minor:-0}"
major="${major:-0}"

case "$kind" in
  major) major=$((major + 1)); minor=0; patch=0 ;;
  minor) minor=$((minor + 1)); patch=0 ;;
  patch) patch=$((patch + 1)) ;;
  *) echo "Usage: $0 [major|minor|patch]"; exit 1 ;;
esac

new_version="$major.$minor.$patch"
echo "$new_version" > "$VERSION_FILE"
echo "Bumped $current -> $new_version ($VERSION_FILE)"
