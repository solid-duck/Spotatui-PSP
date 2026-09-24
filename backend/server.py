from http.server import BaseHTTPRequestHandler, HTTPServer
from urllib.parse import urlencode, urlparse, parse_qs
from urllib.request import Request, urlopen
from urllib.error import HTTPError
import base64
import hashlib
import json
import secrets
import time
import socket
import threading
from pathlib import Path

HOST = "0.0.0.0"
PORT = 8080
DISCOVERY_PORT = 8081

CLIENT_ID = "b67c13b0a7d54eeab3dcd00f2f6a20f0"
REDIRECT_URI = "http://127.0.0.1:8080/spotify/callback"

TOKEN_FILE = Path(__file__).with_name("spotify_token.json")

SCOPES = [
    "user-read-private",
    "playlist-read-private",
    "playlist-read-collaborative",
    "user-library-read",
    "user-read-playback-state",
    "user-read-currently-playing",
    "user-modify-playback-state",
]

spotify_session = {
    "state": None,
    "code_verifier": None,
    "access_token": None,
    "refresh_token": None,
    "expires_at": 0,
}


def generate_code_verifier():
    return secrets.token_urlsafe(64)


def generate_code_challenge(verifier):
    digest = hashlib.sha256(verifier.encode()).digest()
    return base64.urlsafe_b64encode(digest).decode().rstrip("=")


def save_token():
    data = {
        "access_token": spotify_session["access_token"],
        "refresh_token": spotify_session["refresh_token"],
        "expires_at": spotify_session["expires_at"],
    }
    TOKEN_FILE.write_text(json.dumps(data, indent=2), encoding="utf-8")


def load_token():
    if not TOKEN_FILE.exists():
        return

    try:
        data = json.loads(TOKEN_FILE.read_text(encoding="utf-8"))
        spotify_session["access_token"] = data.get("access_token")
        spotify_session["refresh_token"] = data.get("refresh_token")
        spotify_session["expires_at"] = data.get("expires_at", 0)
        print("[SPOTIFY] Saved session loaded")
    except Exception as error:
        print("[SPOTIFY] Could not load saved session:", error)


def create_login_url():
    state = secrets.token_urlsafe(24)
    verifier = generate_code_verifier()
    challenge = generate_code_challenge(verifier)

    spotify_session["state"] = state
    spotify_session["code_verifier"] = verifier

    params = {
        "client_id": CLIENT_ID,
        "response_type": "code",
        "redirect_uri": REDIRECT_URI,
        "scope": " ".join(SCOPES),
        "state": state,
        "code_challenge_method": "S256",
        "code_challenge": challenge,
    }

    return "https://accounts.spotify.com/authorize?" + urlencode(params)


def token_request(data):
    request = Request(
        "https://accounts.spotify.com/api/token",
        data=urlencode(data).encode(),
        headers={"Content-Type": "application/x-www-form-urlencoded"},
        method="POST",
    )

    with urlopen(request, timeout=15) as response:
        return json.loads(response.read().decode())


def store_token_result(result):
    spotify_session["access_token"] = result.get("access_token")

    if result.get("refresh_token"):
        spotify_session["refresh_token"] = result["refresh_token"]

    spotify_session["expires_at"] = (
        time.time() + result.get("expires_in", 3600) - 60
    )

    save_token()


def exchange_code(code):
    result = token_request(
        {
            "client_id": CLIENT_ID,
            "grant_type": "authorization_code",
            "code": code,
            "redirect_uri": REDIRECT_URI,
            "code_verifier": spotify_session["code_verifier"],
        }
    )

    store_token_result(result)


def refresh_access_token():
    refresh_token = spotify_session.get("refresh_token")

    if not refresh_token:
        return False

    result = token_request(
        {
            "client_id": CLIENT_ID,
            "grant_type": "refresh_token",
            "refresh_token": refresh_token,
        }
    )

    store_token_result(result)
    print("[SPOTIFY] Access token refreshed")
    return True


def ensure_access_token():
    if not spotify_session.get("refresh_token"):
        return False

    if (
        not spotify_session.get("access_token")
        or time.time() >= spotify_session.get("expires_at", 0)
    ):
        try:
            return refresh_access_token()
        except Exception as error:
            print("[SPOTIFY] Refresh error:", error)
            return False

    return True


def spotify_api_get(path):
    if not ensure_access_token():
        raise RuntimeError("Spotify is not authenticated")

    request = Request(
        "https://api.spotify.com/v1" + path,
        headers={"Authorization": "Bearer " + spotify_session["access_token"]},
    )

    try:
        with urlopen(request, timeout=15) as response:
            body = response.read()
            if not body:
                return None
            return json.loads(body.decode())

    except HTTPError as error:
        if error.code == 204:
            return None

        if error.code == 401 and refresh_access_token():
            request = Request(
                "https://api.spotify.com/v1" + path,
                headers={"Authorization": "Bearer " + spotify_session["access_token"]},
            )

            try:
                with urlopen(request, timeout=15) as response:
                    body = response.read()
                    if not body:
                        return None
                    return json.loads(body.decode())
            except HTTPError as retry_error:
                if retry_error.code == 204:
                    return None
                raise

        raise


def spotify_api_request(method, path, data=None):
    if not ensure_access_token():
        raise RuntimeError("Spotify is not authenticated")

    body = None
    headers = {
        "Authorization": "Bearer " + spotify_session["access_token"],
        "Content-Type": "application/json",
    }

    if data is not None:
        body = json.dumps(data).encode("utf-8")

    request = Request(
        "https://api.spotify.com/v1" + path,
        data=body,
        headers=headers,
        method=method,
    )

    try:
        with urlopen(request, timeout=15) as response:
            raw = response.read()
            if not raw:
                return None
            return json.loads(raw.decode())

    except HTTPError as error:
        if error.code == 204:
            return None

        if error.code == 401 and refresh_access_token():
            headers["Authorization"] = "Bearer " + spotify_session["access_token"]

            request = Request(
                "https://api.spotify.com/v1" + path,
                data=body,
                headers=headers,
                method=method,
            )

            with urlopen(request, timeout=15) as response:
                raw = response.read()
                if not raw:
                    return None
                return json.loads(raw.decode())

        raise


def psp_text(value):
    value = (value or "").upper()
    allowed = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 .-/"
    cleaned = "".join(
        character if character in allowed else " "
        for character in value
    )
    return " ".join(cleaned.split())[:42]


class SpotatuiHandler(BaseHTTPRequestHandler):
    def send_text(self, code, text, content_type="text/plain; charset=utf-8"):
        body = text.encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        parsed = urlparse(self.path)
        path = parsed.path
        query = parse_qs(parsed.query)

        if path == "/ping":
            self.send_text(200, "SPOTATUI SERVER ONLINE")
            print("[PSP] GET /ping -> 200 OK")
            return

        if path == "/spotify/login":
            self.send_response(302)
            self.send_header("Location", create_login_url())
            self.end_headers()
            print("[SPOTIFY] Login started")
            return

        if path == "/spotify/callback":
            if "error" in query:
                self.send_text(400, "Spotify authorization denied.")
                print("[SPOTIFY] Authorization denied")
                return

            code = query.get("code", [None])[0]
            state = query.get("state", [None])[0]

            if not code:
                self.send_text(400, "Missing authorization code.")
                return

            if state != spotify_session["state"]:
                self.send_text(400, "Invalid OAuth state.")
                print("[SPOTIFY] Invalid state")
                return

            try:
                exchange_code(code)

                html = """
                <html><head><title>SpotatuiPSP</title></head>
                <body style="background:#0b1006;color:#88ff39;font-family:monospace;text-align:center;padding-top:80px">
                <h1>SPOTATUI PSP</h1>
                <h2>SPOTIFY CONNECTED</h2>
                <p>You can return to your PSP.</p>
                </body></html>
                """

                self.send_text(200, html, "text/html; charset=utf-8")
                print("[SPOTIFY] CONNECTED - token saved")

            except Exception as error:
                print("[SPOTIFY] TOKEN ERROR:", error)
                self.send_text(500, "Spotify token exchange failed.")

            return

        if path == "/spotify/status":
            self.send_text(
                200,
                "CONNECTED" if ensure_access_token() else "DISCONNECTED",
            )
            return

        if path == "/spotify/me":
            try:
                profile = spotify_api_get("/me")
                name = (
                    profile.get("display_name")
                    or profile.get("id")
                    or "SPOTIFY USER"
                )
                self.send_text(200, psp_text(name))
                print("[PSP] GET /spotify/me -> 200 OK")

            except Exception as error:
                print("[SPOTIFY] /me error:", error)
                self.send_text(401, "NOT AUTHENTICATED")

            return


        if path == "/spotify/search":
            try:
                term = (query.get("q", [""])[0] or "").strip()

                if not term:
                    self.send_text(400, "EMPTY QUERY")
                    return

                result = spotify_api_get(
                    "/search?" + urlencode(
                        {
                            "q": term,
                            "type": "track",
                            "limit": 5,
                        }
                    )
                ) or {}

                lines = []

                for item in result.get("tracks", {}).get("items", []):
                    artists = ", ".join(
                        artist.get("name", "")
                        for artist in item.get("artists", [])
                        if artist.get("name")
                    )

                    title = psp_text(item.get("name") or "UNKNOWN TRACK")
                    artist = psp_text(artists or "UNKNOWN ARTIST")
                    track_id = item.get("id") or ""

                    if track_id:
                        lines.append(f"{title}|{artist}|{track_id}")

                self.send_text(200, "\n".join(lines) if lines else "NONE")
                print(f"[PSP] SEARCH -> {len(lines)} result(s) | {psp_text(term)}")

            except HTTPError as error:
                print("[SPOTIFY] /search HTTP error:", error.code)
                self.send_text(error.code, "SEARCH ERROR")
            except Exception as error:
                print("[SPOTIFY] /search error:", error)
                self.send_text(500, "SEARCH ERROR")

            return

        if path == "/spotify/play-track":
            try:
                track_id = (query.get("id", [""])[0] or "").strip()

                if not track_id:
                    self.send_text(400, "MISSING TRACK ID")
                    return

                spotify_api_request(
                    "PUT",
                    "/me/player/play",
                    {"uris": [f"spotify:track:{track_id}"]},
                )

                self.send_text(204, "")
                print(f"[PSP] PLAY TRACK -> {track_id}")

            except HTTPError as error:
                print("[SPOTIFY] /play-track HTTP error:", error.code)
                self.send_text(error.code, "PLAY TRACK ERROR")
            except Exception as error:
                print("[SPOTIFY] /play-track error:", error)
                self.send_text(500, "PLAY TRACK ERROR")

            return

        if path == "/spotify/player":
            try:
                playback = spotify_api_get("/me/player/currently-playing")

                if not playback or not playback.get("item"):
                    self.send_text(200, "NONE")
                    print("[PSP] GET /spotify/player -> NO ACTIVE TRACK")
                    return

                item = playback["item"]
                artists = item.get("artists") or []

                if artists:
                    artist = ", ".join(
                        entry.get("name", "")
                        for entry in artists
                        if entry.get("name")
                    )
                else:
                    artist = item.get("show", {}).get("name", "UNKNOWN ARTIST")

                title = psp_text(item.get("name") or "UNKNOWN TRACK")
                artist = psp_text(artist or "UNKNOWN ARTIST")
                progress_ms = playback.get("progress_ms") or 0
                duration_ms = item.get("duration_ms") or 0
                is_playing = 1 if playback.get("is_playing") else 0

                payload = (
                    f"{title}\n"
                    f"{artist}\n"
                    f"{progress_ms}\n"
                    f"{duration_ms}\n"
                    f"{is_playing}"
                )

                self.send_text(200, payload)
                print(
                    f"[PSP] GET /spotify/player -> 200 OK | "
                    f"{title} / {artist}"
                )

            except HTTPError as error:
                print("[SPOTIFY] /player HTTP error:", error.code)
                self.send_text(error.code, "SPOTIFY PLAYER ERROR")

            except Exception as error:
                print("[SPOTIFY] /player error:", error)
                self.send_text(500, "SPOTIFY PLAYER ERROR")

            return

        if path == "/spotify/play":
            try:
                spotify_api_request("PUT", "/me/player")
                self.send_text(204, "")
                print("[PSP] PLAY -> 204")
            except HTTPError as error:
                print("[SPOTIFY] /play HTTP error:", error.code)
                self.send_text(error.code, "PLAY ERROR")
            except Exception as error:
                print("[SPOTIFY] /play error:", error)
                self.send_text(500, "PLAY ERROR")
            return

        if path == "/spotify/pause":
            try:
                spotify_api_request("PUT", "/me/player/pause")
                self.send_text(204, "")
                print("[PSP] PAUSE -> 204")
            except HTTPError as error:
                print("[SPOTIFY] /pause HTTP error:", error.code)
                self.send_text(error.code, "PAUSE ERROR")
            except Exception as error:
                print("[SPOTIFY] /pause error:", error)
                self.send_text(500, "PAUSE ERROR")
            return

        if path == "/spotify/next":
            try:
                spotify_api_request("POST", "/me/player/next")
                self.send_text(204, "")
                print("[PSP] NEXT -> 204")
            except HTTPError as error:
                print("[SPOTIFY] /next HTTP error:", error.code)
                self.send_text(error.code, "NEXT ERROR")
            except Exception as error:
                print("[SPOTIFY] /next error:", error)
                self.send_text(500, "NEXT ERROR")
            return

        if path == "/spotify/previous":
            try:
                spotify_api_request("POST", "/me/player/previous")
                self.send_text(204, "")
                print("[PSP] PREVIOUS -> 204")
            except HTTPError as error:
                print("[SPOTIFY] /previous HTTP error:", error.code)
                self.send_text(error.code, "PREVIOUS ERROR")
            except Exception as error:
                print("[SPOTIFY] /previous error:", error)
                self.send_text(500, "PREVIOUS ERROR")
            return

        if path == "/spotify/devices":
            try:
                devices = spotify_api_get("/me/player/devices") or {}
                lines = []

                for device in devices.get("devices", []):
                    name = psp_text(device.get("name") or "UNKNOWN")
                    device_id = device.get("id") or ""
                    active = "1" if device.get("is_active") else "0"
                    lines.append(f"{name}|{active}|{device_id}")

                self.send_text(200, "\n".join(lines) if lines else "NONE")
                print("[PSP] GET /spotify/devices -> 200 OK")
            except Exception as error:
                print("[SPOTIFY] /devices error:", error)
                self.send_text(500, "DEVICES ERROR")
            return

        self.send_text(404, "NOT FOUND")

    def log_message(self, format, *args):
        return



def discovery_server():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    sock.bind(("0.0.0.0", DISCOVERY_PORT))

    print(f"[DISCOVERY] UDP listening on 0.0.0.0:{DISCOVERY_PORT}")

    while True:
        try:
            data, address = sock.recvfrom(256)

            if data.strip() == b"SPOTATUI_DISCOVER":
                sock.sendto(b"SPOTATUI_BACKEND", address)
                print(f"[DISCOVERY] PSP found backend from {address[0]}")
        except Exception as error:
            print("[DISCOVERY] Error:", error)


def main():
    load_token()

    discovery_thread = threading.Thread(
        target=discovery_server,
        name="SpotatuiDiscovery",
        daemon=True,
    )
    discovery_thread.start()

    server = HTTPServer((HOST, PORT), SpotatuiHandler)

    print("================================")
    print(" SPOTATUI PSP BACKEND v0.12")
    print("================================")
    print(f"HTTP listening on {HOST}:{PORT}")
    print(f"Discovery UDP on 0.0.0.0:{DISCOVERY_PORT}")
    print("Endpoints:")
    print("  /ping")
    print("  /spotify/login")
    print("  /spotify/callback")
    print("  /spotify/status")
    print("  /spotify/me")
    print("  /spotify/player")
    print("  /spotify/search?q=...")
    print("  /spotify/play-track?id=...")
    print("  /spotify/play")
    print("  /spotify/pause")
    print("  /spotify/next")
    print("  /spotify/previous")
    print("  /spotify/devices")
    print()
    print("Waiting for PSP...")

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nStopping server...")
        server.server_close()


if __name__ == "__main__":
    main()
