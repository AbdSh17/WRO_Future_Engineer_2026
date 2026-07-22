#!/usr/bin/env python3
"""
WRO 2026 — PID Tuner CLI
Run on your laptop to tune ESP32 PID gains over WiFi (UDP).

Usage:
    python3 pid_tuner_laptop.py <ESP32_IP>

Commands:
    start           start the robot
    stop            stop the robot
    status          print current PID values from ESP32
    fKp=<val>       set forward Kp   (e.g. fKp=2.5)
    fKi=<val>       set forward Ki
    fKd=<val>       set forward Kd
    tKp=<val>       set turn Kp
    tKi=<val>       set turn Ki
    tKd=<val>       set turn Kd
    q / quit        exit
"""

import socket
import sys
import threading

ESP32_PORT = 5006
LISTEN_PORT = 5007  # local port to receive replies on
TIMEOUT_S = 1.0


def receive_loop(sock: socket.socket) -> None:
    """Background thread — prints any reply from the ESP32."""
    while True:
        try:
            data, _ = sock.recvfrom(512)
            print(f"\n[ESP32] {data.decode().strip()}")
        except (socket.timeout, OSError):
            pass


def main() -> None:
    if len(sys.argv) < 2:
        print("Usage: python3 pid_tuner_laptop.py <ESP32_IP>")
        sys.exit(1)

    esp_ip = sys.argv[1]
    esp_addr = (esp_ip, ESP32_PORT)

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("", LISTEN_PORT))
    sock.settimeout(TIMEOUT_S)

    # Start background receiver
    t = threading.Thread(target=receive_loop, args=(sock,), daemon=True)
    t.start()

    print(f"Connected to ESP32 at {esp_ip}:{ESP32_PORT}")
    print("Type a command (start / stop / status / fKp=1.2 / tKi=0.05 / q)\n")

    VALID_PREFIXES = (
        "start",
        "stop",
        "status",
        "fKp=",
        "fKi=",
        "fKd=",
        "tKp=",
        "tKi=",
        "tKd=",
    )

    while True:
        try:
            cmd = input("> ").strip()
        except (EOFError, KeyboardInterrupt):
            print("\nExiting.")
            break

        if cmd in ("q", "quit", "exit"):
            break

        if not cmd:
            continue

        if not any(cmd == p or cmd.startswith(p) for p in VALID_PREFIXES):
            print(f"Unknown command: {cmd}")
            print("Valid: start stop status fKp= fKi= fKd= tKp= tKi= tKd=")
            continue

        sock.sendto(cmd.encode(), esp_addr)

    sock.close()


if __name__ == "__main__":
    main()
