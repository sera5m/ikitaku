#!/usr/bin/env bash
# Start the phosphor viewer and wrap it in a site-specific browser window
# (Firefox --ssb, else Chromium --app). Looks like its own app, still Firefox.
set -euo pipefail
cd "$(dirname "$0")/.."
URL="http://127.0.0.1:5173"

need() {
  command -v "$1" >/dev/null 2>&1
}

if ! need npm; then
  echo "need node + npm. On Mint:  sudo apt install -y nodejs npm"
  exit 1
fi

if [ ! -d node_modules ]; then
  echo "first run: npm install…"
  npm install
fi

# reuse a live server if one is already up
if ! curl -sf "$URL" >/dev/null 2>&1; then
  npm run dev >/tmp/ikitaku-ui.log 2>&1 &
  echo $! > /tmp/ikitaku-ui.pid
  for _ in $(seq 1 60); do
    if curl -sf "$URL" >/dev/null 2>&1; then
      break
    fi
    sleep 0.25
  done
  if ! curl -sf "$URL" >/dev/null 2>&1; then
    echo "vite failed to bind $URL"
    echo "log: /tmp/ikitaku-ui.log"
    tail -n 40 /tmp/ikitaku-ui.log || true
    exit 1
  fi
fi

open_window() {
  if need firefox; then
    firefox --class=Ikitaku --name=Ikitaku --ssb "$URL" && return 0
    firefox --class=Ikitaku --kiosk "$URL" && return 0
    firefox --new-window "$URL" && return 0
  fi
  if need firefox-esr; then
    firefox-esr --class=Ikitaku --ssb "$URL" && return 0
    firefox-esr --new-window "$URL" && return 0
  fi
  if need chromium-browser; then
    chromium-browser --app="$URL" --class=Ikitaku && return 0
  fi
  if need chromium; then
    chromium --app="$URL" --class=Ikitaku && return 0
  fi
  if need google-chrome; then
    google-chrome --app="$URL" && return 0
  fi
  if need xdg-open; then
    xdg-open "$URL" && return 0
  fi
  echo "server is up at $URL — open that in a browser"
  return 1
}

open_window
echo "viewer: $URL"
echo "leave this terminal open. Ctrl-C does not kill vite if it was already running."
echo "to stop the server: kill \$(cat /tmp/ikitaku-ui.pid) 2>/dev/null"
