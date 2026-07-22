"""
telemetry_gui.py — WRO robot telemetry dashboard (PyQt5 GUI version)

Listens for JSON UDP packets from the ESP32 and displays them in a
graphical dashboard. Click any sensor card to open its detail panel
with a live graph and a scrollable history table.

CHANGES from the ToF-era version:
- Side ToF (L/R) removed entirely — replaced by camera-derived NAV
  wall-following data and OBST pillar data (both relayed by the ESP32
  from the Pi over UART).
- History capture now runs continuously in the background for ALL
  sensor groups regardless of which card is focused. Switching cards
  no longer clears anything — it just changes what's displayed.
- Each detail panel has an explicit "Restart history" button that
  clears ONLY that sensor's stored history, on demand.

Expected packet shape (all fields optional):
{
  "tof":   {"F": 340},
  "imu":   {"yaw": 45.2},
  "enc":   {"mm": 230},
  "nav":   {"L": 72, "R": 18},
  "obst":  {"color": "GREEN", "dist": 280, "lat": 60},
  "state": "FORWARD",
  "turn":  2,
  "lap":   1
}

Install once:
    pip install PyQt5 pyqtgraph

Run:
    python3 telemetry_gui.py
"""

import csv
import json
import socket
import threading
import time
from collections import deque

from PyQt5.QtCore import Qt, QTimer, pyqtSignal, QObject
from PyQt5.QtGui import QFont
from PyQt5.QtWidgets import (
    QApplication,
    QWidget,
    QMainWindow,
    QVBoxLayout,
    QHBoxLayout,
    QLabel,
    QFrame,
    QPushButton,
    QSlider,
    QTableWidget,
    QTableWidgetItem,
    QFileDialog,
    QSplitter,
)
import pyqtgraph as pg

# ---------------------------------------------------------------------------
# Config
# ---------------------------------------------------------------------------
LISTEN_PORT = 5005
STALE_TIMEOUT_S = 3.0
HISTORY_SECONDS = 60  # how much time-series history the live graph shows
MAX_TABLE_ROWS = 2000
MAX_HISTORY_POINTS = 5000  # per-series cap on stored (t, value) points
# ---------------------------------------------------------------------------

SENSOR_GROUPS = {
    "nav": {"title": "Wall Following (Camera)", "columns": ["L", "R"]},
    "obst": {"title": "Obstacle Pillar (Camera)", "columns": ["dist", "lat"]},
    "imu": {"title": "IMU Yaw", "columns": ["yaw"]},
    "state": {"title": "Algorithm / Odometry", "columns": ["enc_mm", "front_mm"]},
}


# ---------------------------------------------------------------------------
# UDP receiver — background thread, emits Qt signals
# ---------------------------------------------------------------------------
class UdpReceiver(QObject):
    packet_received = pyqtSignal(dict)

    def __init__(self, port):
        super().__init__()
        self.port = port
        self._running = True

    def start(self):
        threading.Thread(target=self._run, daemon=True).start()

    def stop(self):
        self._running = False

    def _run(self):
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.bind(("0.0.0.0", self.port))
        sock.settimeout(0.5)

        while self._running:
            try:
                data, _ = sock.recvfrom(2048)
            except socket.timeout:
                continue
            except OSError:
                break

            text = data.decode("utf-8", errors="replace").strip()
            try:
                parsed = json.loads(text)
            except json.JSONDecodeError:
                parsed = {"raw": text}

            self.packet_received.emit(parsed)


# ---------------------------------------------------------------------------
# Color helpers
# ---------------------------------------------------------------------------
def color_for_nav(pct):
    """High wall presence = green (safe/expected), low = red (opening/corner)."""
    if pct is None:
        return "#666666"
    if pct < 25:
        return "#e74c3c"
    if pct < 50:
        return "#f1c40f"
    return "#2ecc71"


def color_for_obst(color_name):
    if color_name == "RED":
        return "#e74c3c"
    if color_name == "GREEN":
        return "#2ecc71"
    return "#666666"


# ---------------------------------------------------------------------------
# Sensor card — clickable summary box
# ---------------------------------------------------------------------------
class SensorCard(QFrame):
    clicked = pyqtSignal(str)

    def __init__(self, key, title):
        super().__init__()
        self.key = key
        self.setFrameShape(QFrame.StyledPanel)
        self.setStyleSheet(
            "QFrame { background-color: #1e1e2e; border-radius: 10px; "
            "border: 2px solid #313244; }"
            "QFrame:hover { border: 2px solid #89b4fa; }"
        )
        self.setCursor(Qt.PointingHandCursor)
        self.setMinimumHeight(160)

        layout = QVBoxLayout(self)
        title_label = QLabel(title)
        title_label.setStyleSheet("color: #cdd6f4; font-weight: bold; border: none;")
        title_label.setFont(QFont("Sans", 12))
        layout.addWidget(title_label)

        self.body = QLabel("--")
        self.body.setStyleSheet("color: #a6e3a1; border: none;")
        self.body.setFont(QFont("Monospace", 11))
        self.body.setWordWrap(True)
        layout.addWidget(self.body)
        layout.addStretch()

    def mousePressEvent(self, event):
        self.clicked.emit(self.key)

    def set_body_html(self, html):
        self.body.setText(html)


# ---------------------------------------------------------------------------
# Detail panel — graph + history table for one sensor group
# ---------------------------------------------------------------------------
class DetailPanel(QWidget):
    restart_requested = pyqtSignal(str)

    def __init__(self):
        super().__init__()
        layout = QVBoxLayout(self)

        header = QHBoxLayout()
        self.title_label = QLabel("Select a sensor card")
        self.title_label.setStyleSheet("color: #cdd6f4; font-weight: bold;")
        self.title_label.setFont(QFont("Sans", 14))
        header.addWidget(self.title_label)
        header.addStretch()

        self.restart_btn = QPushButton("Restart history")
        self.restart_btn.setStyleSheet(
            "QPushButton { background-color: #45475a; color: #f38ba8; "
            "border-radius: 6px; padding: 4px 10px; }"
            "QPushButton:hover { background-color: #585b70; }"
        )
        self.restart_btn.clicked.connect(self._on_restart)
        header.addWidget(self.restart_btn)
        layout.addLayout(header)

        self.plot = pg.PlotWidget()
        self.plot.setBackground("#1e1e2e")
        self.plot.showGrid(x=True, y=True, alpha=0.2)
        self.plot.addLegend()
        layout.addWidget(self.plot, stretch=2)

        table_header = QHBoxLayout()
        table_label = QLabel("Captured snapshots")
        table_label.setStyleSheet("color: #cdd6f4;")
        table_header.addWidget(table_label)
        table_header.addStretch()
        self.export_btn = QPushButton("Export CSV")
        table_header.addWidget(self.export_btn)
        layout.addLayout(table_header)

        self.table = QTableWidget(0, 1)
        layout.addWidget(self.table, stretch=1)

        self.export_btn.clicked.connect(self.export_csv)

        self.current_key = None
        self.curves = {}

    def _on_restart(self):
        if self.current_key:
            self.restart_requested.emit(self.current_key)

    def export_csv(self):
        rows = [
            self.table.item(r, c).text() if self.table.item(r, c) else ""
            for r in range(self.table.rowCount())
            for c in range(self.table.columnCount())
        ]
        if self.table.rowCount() == 0:
            return
        path, _ = QFileDialog.getSaveFileName(
            self, "Export CSV", "", "CSV Files (*.csv)"
        )
        if not path:
            return
        headers = [
            self.table.horizontalHeaderItem(c).text()
            for c in range(self.table.columnCount())
        ]
        with open(path, "w", newline="") as f:
            writer = csv.writer(f)
            writer.writerow(headers)
            for r in range(self.table.rowCount()):
                writer.writerow(
                    [
                        self.table.item(r, c).text() if self.table.item(r, c) else ""
                        for c in range(self.table.columnCount())
                    ]
                )

    def set_focus(self, key):
        """Switch which sensor group is displayed. Does NOT clear any stored data."""
        group = SENSOR_GROUPS[key]
        self.current_key = key
        self.title_label.setText(group["title"])
        self.plot.clear()
        self.curves = {}
        colors = ["#f38ba8", "#a6e3a1", "#89b4fa", "#f9e2af"]
        for i, col in enumerate(group["columns"]):
            self.curves[col] = self.plot.plot(
                pen=pg.mkPen(colors[i % len(colors)], width=2), name=col
            )
        self.table.setColumnCount(len(group["columns"]) + 1)
        self.table.setHorizontalHeaderLabels(["time"] + group["columns"])

    def update_graph(self, series):
        for col, (xs, ys) in series.items():
            if col in self.curves:
                self.curves[col].setData(xs, ys)

    def reload_table(self, rows):
        """Fully repopulate the table from a persistent row list (list of [ts, v1, v2, ...])."""
        self.table.setRowCount(0)
        start = max(0, len(rows) - MAX_TABLE_ROWS)
        for row_data in rows[start:]:
            r = self.table.rowCount()
            self.table.insertRow(r)
            for c, val in enumerate(row_data):
                self.table.setItem(r, c, QTableWidgetItem(str(val)))
        self.table.scrollToBottom()


# ---------------------------------------------------------------------------
# Main window
# ---------------------------------------------------------------------------
class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("WRO Telemetry Dashboard")
        self.resize(1250, 780)
        self.setStyleSheet("QMainWindow { background-color: #11111b; }")

        self.last_data = {}
        self.last_rx_time = 0.0
        self.packet_count = 0
        self.start_time = time.time()

        # Persistent, always-on background storage — independent of focus.
        # Time-series for graphing:
        self.history_series = {
            "nav": {
                "L": deque(maxlen=MAX_HISTORY_POINTS),
                "R": deque(maxlen=MAX_HISTORY_POINTS),
            },
            "obst": {
                "dist": deque(maxlen=MAX_HISTORY_POINTS),
                "lat": deque(maxlen=MAX_HISTORY_POINTS),
            },
            "imu": {"yaw": deque(maxlen=MAX_HISTORY_POINTS)},
            "state": {
                "enc_mm": deque(maxlen=MAX_HISTORY_POINTS),
                "front_mm": deque(maxlen=MAX_HISTORY_POINTS),
            },
        }
        # Captured snapshot rows (at the configured interval), per group:
        self.history_rows = {key: [] for key in SENSOR_GROUPS}

        self.capture_interval = 0.5
        self.last_capture_time = 0.0

        self._build_ui()

        self.receiver = UdpReceiver(LISTEN_PORT)
        self.receiver.packet_received.connect(self.on_packet)
        self.receiver.start()

        self.ui_timer = QTimer()
        self.ui_timer.timeout.connect(self.refresh_ui)
        self.ui_timer.start(100)  # 10Hz UI refresh

    # -----------------------------------------------------------------
    def _build_ui(self):
        central = QWidget()
        self.setCentralWidget(central)
        main_layout = QVBoxLayout(central)

        status_bar = QHBoxLayout()
        self.status_dot = QLabel("● DISCONNECTED")
        self.status_dot.setStyleSheet("color: #e74c3c; font-weight: bold;")
        status_bar.addWidget(self.status_dot)

        self.pkt_label = QLabel("packets=0")
        self.pkt_label.setStyleSheet("color: #cdd6f4;")
        status_bar.addWidget(self.pkt_label)

        status_bar.addStretch()

        interval_label = QLabel("Capture interval:")
        interval_label.setStyleSheet("color: #cdd6f4;")
        status_bar.addWidget(interval_label)

        self.interval_slider = QSlider(Qt.Horizontal)
        self.interval_slider.setMinimum(1)  # 0.1s
        self.interval_slider.setMaximum(20)  # 2.0s
        self.interval_slider.setValue(5)  # 0.5s default
        self.interval_slider.setFixedWidth(160)
        self.interval_slider.valueChanged.connect(self.on_interval_changed)
        status_bar.addWidget(self.interval_slider)

        self.interval_value_label = QLabel("0.5s")
        self.interval_value_label.setStyleSheet("color: #f9e2af; font-weight: bold;")
        status_bar.addWidget(self.interval_value_label)

        main_layout.addLayout(status_bar)

        splitter = QSplitter(Qt.Horizontal)
        main_layout.addWidget(splitter, stretch=1)

        cards_widget = QWidget()
        cards_layout = QVBoxLayout(cards_widget)

        self.cards = {}
        for key, group in SENSOR_GROUPS.items():
            card = SensorCard(key, group["title"])
            card.clicked.connect(self.on_card_clicked)
            cards_layout.addWidget(card)
            self.cards[key] = card
        cards_layout.addStretch()

        splitter.addWidget(cards_widget)

        self.detail = DetailPanel()
        self.detail.restart_requested.connect(self.on_restart_requested)
        splitter.addWidget(self.detail)
        splitter.setSizes([350, 900])

    # -----------------------------------------------------------------
    def on_interval_changed(self, value):
        self.capture_interval = value / 10.0
        self.interval_value_label.setText(f"{self.capture_interval:.1f}s")

    def on_card_clicked(self, key):
        self.detail.set_focus(key)
        self.detail.reload_table(self.history_rows[key])

    def on_restart_requested(self, key):
        self.history_rows[key] = []
        for series in self.history_series[key].values():
            series.clear()
        self.detail.reload_table([])

    # -----------------------------------------------------------------
    def on_packet(self, parsed):
        self.last_data = parsed
        self.last_rx_time = time.time()
        self.packet_count += 1
        t = self.last_rx_time - self.start_time

        nav = parsed.get("nav", {})
        if "L" in nav:
            self.history_series["nav"]["L"].append((t, nav["L"]))
        if "R" in nav:
            self.history_series["nav"]["R"].append((t, nav["R"]))

        obst = parsed.get("obst", {})
        if "dist" in obst:
            self.history_series["obst"]["dist"].append((t, obst["dist"]))
        if "lat" in obst:
            self.history_series["obst"]["lat"].append((t, obst["lat"]))

        imu = parsed.get("imu", {})
        if "yaw" in imu:
            self.history_series["imu"]["yaw"].append((t, imu["yaw"]))

        enc = parsed.get("enc", {})
        if "mm" in enc:
            self.history_series["state"]["enc_mm"].append((t, enc["mm"]))
        tof = parsed.get("tof", {})
        if "F" in tof:
            self.history_series["state"]["front_mm"].append((t, tof["F"]))

        # Background capture for ALL groups every interval tick, regardless
        # of which card is currently focused.
        if self.last_rx_time - self.last_capture_time >= self.capture_interval:
            self.last_capture_time = self.last_rx_time
            self._capture_all_snapshots(parsed)

    def _capture_all_snapshots(self, parsed):
        ts_str = time.strftime("%H:%M:%S")

        nav = parsed.get("nav", {})
        self.history_rows["nav"].append([ts_str, nav.get("L", ""), nav.get("R", "")])

        obst = parsed.get("obst", {})
        self.history_rows["obst"].append(
            [ts_str, obst.get("dist", ""), obst.get("lat", "")]
        )

        imu = parsed.get("imu", {})
        self.history_rows["imu"].append([ts_str, imu.get("yaw", "")])

        enc = parsed.get("enc", {})
        tof = parsed.get("tof", {})
        self.history_rows["state"].append([ts_str, enc.get("mm", ""), tof.get("F", "")])

        # Trim to avoid unbounded memory growth over a long session
        for key in self.history_rows:
            if len(self.history_rows[key]) > MAX_TABLE_ROWS * 2:
                self.history_rows[key] = self.history_rows[key][-MAX_TABLE_ROWS:]

        # If the currently-focused panel matches, refresh its table live
        if self.detail.current_key:
            self.detail.reload_table(self.history_rows[self.detail.current_key])

    # -----------------------------------------------------------------
    def refresh_ui(self):
        now = time.time()
        age = now - self.last_rx_time if self.last_rx_time else float("inf")

        if age > STALE_TIMEOUT_S:
            self.status_dot.setText("● DISCONNECTED")
            self.status_dot.setStyleSheet("color: #e74c3c; font-weight: bold;")
        else:
            hz = 1.0 / max(age, 0.001)
            self.status_dot.setText(f"● LIVE  {hz:.1f} Hz")
            self.status_dot.setStyleSheet("color: #2ecc71; font-weight: bold;")

        self.pkt_label.setText(f"packets={self.packet_count}")

        # --- NAV card ---
        nav = self.last_data.get("nav", {})
        l, r = nav.get("L"), nav.get("R")
        html = (
            f'<span style="color:{color_for_nav(l)}">Left: {l if l is not None else "--"}%</span><br>'
            f'<span style="color:{color_for_nav(r)}">Right: {r if r is not None else "--"}%</span>'
        )
        self.cards["nav"].set_body_html(html)

        # --- OBST card ---
        obst = self.last_data.get("obst", {})
        c = obst.get("color", "--")
        d = obst.get("dist")
        lat = obst.get("lat")
        html = (
            f'<span style="color:{color_for_obst(c)}">Color: {c}</span><br>'
            f'Distance: {d if d is not None else "--"} mm<br>'
            f'Lateral: {lat if lat is not None else "--"} mm'
        )
        self.cards["obst"].set_body_html(html)

        # --- IMU card ---
        imu = self.last_data.get("imu", {})
        yaw = imu.get("yaw")
        self.cards["imu"].set_body_html(
            f"Yaw: {yaw:+.2f}°" if yaw is not None else "--"
        )

        # --- STATE card ---
        state = self.last_data.get("state", "--")
        lap = self.last_data.get("lap", "--")
        turn = self.last_data.get("turn", "--")
        enc_mm = self.last_data.get("enc", {}).get("mm", "--")
        front_mm = self.last_data.get("tof", {}).get("F", "--")
        self.cards["state"].set_body_html(
            f"State: <b>{state}</b><br>Lap: {lap}   Turn: {turn}<br>"
            f"Encoder: {enc_mm} mm<br>Front wall: {front_mm} mm"
        )

        # --- live graph for the focused sensor ---
        key = self.detail.current_key
        if key:
            series = {}
            for col, buf in self.history_series[key].items():
                pts = [
                    p for p in buf if p[0] > (now - self.start_time) - HISTORY_SECONDS
                ]
                if pts:
                    xs, ys = zip(*pts)
                    series[col] = (list(xs), list(ys))
            self.detail.update_graph(series)

    def closeEvent(self, event):
        self.receiver.stop()
        event.accept()


# ---------------------------------------------------------------------------
def main():
    app = QApplication([])
    win = MainWindow()
    win.show()
    app.exec_()


if __name__ == "__main__":
    main()
