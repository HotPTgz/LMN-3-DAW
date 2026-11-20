#!/bin/bash
# Puts the files given on the command line on the branch's always-current
# GitHub release, replacing the old ones:
#   lmn3          -> release "latest-lmn3"        (marked Latest)
#   lmn3-v3-wide  -> release "latest-limoncello"  (marked Pre-release)
# Other branches are skipped. Needs GH_TOKEN with contents: write.
# Called by windows.yaml and android.yaml after their builds.
set -euo pipefail

BRANCH="${GITHUB_REF_NAME:?}"
REPO="${GITHUB_REPOSITORY:?}"
SHA="${GITHUB_SHA:?}"

case "$BRANCH" in
lmn3)
    TAG=latest-lmn3
    TITLE="Latest LMN-3 (original hardware)"
    KIND=(--latest --prerelease=false)
    ABOUT="the original LMN-3 (800 x 480 screen)"
    ;;
lmn3-v3-wide)
    TAG=latest-limoncello
    TITLE="Latest Limoncello (wide screen)"
    KIND=(--prerelease --latest=false)
    ABOUT="Limoncello, the wide-screen LMN-3 (new hardware, in progress)"
    ;;
*)
    echo "No release for branch $BRANCH"
    exit 0
    ;;
esac

URL="https://github.com/$REPO/releases/download/$TAG"
NOTES="The newest build of the \`$BRANCH\` branch, for $ABOUT. It is replaced automatically every time the branch changes.

Built from $(echo "$SHA" | cut -c1-7) on $(date -u '+%Y-%m-%d %H:%M') UTC.

**Windows:** [LMN-3-windows.zip]($URL/LMN-3-windows.zip). Unzip it and double-click \`LMN-3.exe\`. If Windows says it protected your PC, click **More info**, then **Run anyway**. The computer keyboard plays the LMN-3; press **F12** for the cheat sheet.

**Android phone:** [LMN-3-android.apk]($URL/LMN-3-android.apk). Download it on the phone and open it, allowing installs from your browser when asked. A newer version installs over the old one and keeps your projects.

\`LMN-3-android-emulator.apk\` is only for Android emulators on a PC.

**On the LMN-3 itself (Raspberry Pi):** build it from source, as the [README](https://github.com/$REPO/tree/$BRANCH#install-or-update-on-your-lmn-3) explains."

# point the tag at this commit (making it the first time)
if gh api "repos/$REPO/git/refs/tags/$TAG" >/dev/null 2>&1; then
    gh api -X PATCH "repos/$REPO/git/refs/tags/$TAG" -f sha="$SHA" -F force=true >/dev/null
else
    gh api "repos/$REPO/git/refs" -f ref="refs/tags/$TAG" -f sha="$SHA" >/dev/null ||
        gh api -X PATCH "repos/$REPO/git/refs/tags/$TAG" -f sha="$SHA" -F force=true >/dev/null
fi

# the release (the Windows and Android builds may both get here first)
if ! gh release view "$TAG" -R "$REPO" >/dev/null 2>&1; then
    gh release create "$TAG" -R "$REPO" --title "$TITLE" --notes "$NOTES" "${KIND[@]}" ||
        sleep 5
fi
gh release edit "$TAG" -R "$REPO" --title "$TITLE" --notes "$NOTES" "${KIND[@]}"
gh release upload "$TAG" -R "$REPO" --clobber "$@"
echo "Published to https://github.com/$REPO/releases/tag/$TAG:"
printf '  %s\n' "$@"
