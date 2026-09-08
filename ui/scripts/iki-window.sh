#!/usr/bin/env bash
# Vite on 127.0.0.1:5173, then a real window with the page loaded.
# Firefox --ssb is skipped: it often paints an empty frame and exits 0.
set -euo pipefail
cd "$(dirname "$0")/.."
URL="http://127.0.0.1:5173"

need() { command -v "$1" >/dev/null 2>&1; }

if ! need npm; then
  echo "need node + npm"
  exit 1
fi

if [ ! -d node_modules ]; then
  echo "first run: npm install…"
  npm install
fi

if ! curl -sf "$URL" >/dev/null 2>&1; then
  npm run dev >/tmp/ikitaku-ui.log 2>&1 &
  echo $! > /tmp/ikitaku-ui.pid
  for _ in $(seq 1 80); do
    if curl -sf "$URL" >/dev/null 2>&1; then
      break
    fi
    sleep 0.25
  done
  if ! curl -sf "$URL" >/dev/null 2>&1; then
    echo "vite failed to bind $URL"
    tail -n 50 /tmp/ikitaku-ui.log || true
    exit 1
  fi
fi

# Prefer an app-mode Chromium window. Else a normal Firefox window on the URL.
if need chromium; then
  chromium --app="$URL" --class=Ikitaku --user-data-dir="$HOME/.cache/ikitaku-ui-chromium" &
elif need chromium-browser; then
  chromium-browser --app="$URL" --class=Ikitaku &
elif need google-chrome-stable; then
  google-chrome-stable --app="$URL" &
elif need google-chrome; then
  google-chrome --app="$URL" &
elif need firefox; then
  firefox --new-window "$URL" &
elif need firefox-esr; then
  firefox-esr --new-window "$URL" &
elif need xdg-open; then
  xdg-open "$URL"
else
  echo "server is up at $URL — open that yourself"
fi

echo "viewer: $URL"
echo "if the window is empty, paste that URL into firefox yourself"
echo "log: /tmp/ikitaku-ui.log"
echo "stop: kill \$(cat /tmp/ikitaku-ui.pid) 2>/dev/null"
