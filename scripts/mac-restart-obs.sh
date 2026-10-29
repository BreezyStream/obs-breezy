#!/usr/bin/env bash
set -euo pipefail

APP_NAME="OBS"
BUNDLE_ID="com.obsproject.obs-studio"
WAIT_MS=5000   # total wait ~5s before fallback

if pgrep -x "$APP_NAME" >/dev/null; then
  # Try graceful quit; ignore “user canceled” (-128)
  osascript <<'AS' || true
    tell application id "com.obsproject.obs-studio"
      if it is running then
        try
          quit saving no
        on error number -128
          -- user canceled (e.g., confirmation dialog) -> ignore
        end try
      end if
    end tell
AS

  # Wait up to WAIT_MS for exit
  for i in $(seq 1 $((WAIT_MS/100))); do
    pgrep -x "$APP_NAME" >/dev/null || break
    sleep 0.1
  done

  # Fallbacks if still running
  if pgrep -x "$APP_NAME" >/dev/null; then
    pkill -TERM -x "$APP_NAME" || true
    sleep 1
    pgrep -x "$APP_NAME" >/dev/null && pkill -KILL -x "$APP_NAME" || true
  fi
fi

# Relaunch regardless
open -a "$APP_NAME" --args --remote-debugging-port=9222 --remote-allow-origins=http://localhost:9222 || true
exit 0