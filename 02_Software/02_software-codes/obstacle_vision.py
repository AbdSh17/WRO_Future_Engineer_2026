"""
Code Description:
Obstacle Challenge vision pipeline. Detects red/green pillars, estimates
real-world distance and lateral offset from the robot's centerline using
a single-camera pinhole model, applies temporal smoothing to avoid acting
on single noisy frames, and streams the result to the ESP32 over UART.

The ESP32 owns the actual avoidance decision (heading bias, timing via
encoder). This script only reports what it sees — color, distance_mm,
lateral_offset_mm — never a command.

Libraries:
opencv: color detection, contour extraction
picamera2: camera capture (Camera Module 3, wide-angle)
serial: UART link to ESP32
numpy: array math
time: timing / smoothing window
"""

import cv2
import numpy as np
from picamera2 import Picamera2
import serial
import time

# =============================================================================
# CONFIG - must be calibrated against the real camera before competition use
# =============================================================================

FRAME_WIDTH = 640
FRAME_HEIGHT = 480

# Pillar real-world height per WRO rules (50x50x100mm parallelepiped)
PILLAR_REAL_HEIGHT_MM = 100.0

# --- CALIBRATION REQUIRED ---
# Place a pillar at a known distance D_mm, measure its apparent pixel height
# h_px in this camera's frame, then:
#     FOCAL_LENGTH_PX = (h_px * D_mm) / PILLAR_REAL_HEIGHT_MM
# Repeat at 2-3 distances and average. Placeholder value below is NOT calibrated.
FOCAL_LENGTH_PX = 700.0

# Camera Module 3 Wide horizontal field of view (degrees).
# Verify against the actual lens variant in use.
HORIZONTAL_FOV_DEG = 102.0

# Minimum contour area to consider a real detection (filters noise/distant specks)
MIN_AREA_PX = 800

# Trigger distance: only report pillars closer than this (mm). Beyond this,
# the ESP32 doesn't need to know about it yet.
TRIGGER_DISTANCE_MM = 600

# Temporal smoothing: require this many consecutive consistent frames
# before reporting a detection, and this many consecutive empty frames
# before reporting NONE. Prevents one bad frame from triggering/cancelling
# a maneuver.
CONFIRM_FRAMES = 3

# UART to ESP32
SERIAL_PORT = "/dev/ttyAMA0"
SERIAL_BAUD = 115200

# Report rate
REPORT_HZ = 10

# Set True to show a live debug window (requires X11/VNC). False for
# headless competition runs.
DEBUG_VISUAL = False

# =============================================================================

kernel = np.ones((5, 5), np.uint8)

LOWER_RED1 = np.array([0, 100, 100])
UPPER_RED1 = np.array([10, 255, 255])
LOWER_RED2 = np.array([160, 100, 100])
UPPER_RED2 = np.array([179, 255, 255])
LOWER_GREEN = np.array([40, 70, 70])
UPPER_GREEN = np.array([85, 255, 255])


def detect_pillars(frame):
    """
    Returns a list of detections, each:
    (color_label, distance_mm, lateral_offset_mm, area_px, bbox)
    Sorted by distance ascending (closest first).
    """
    hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)

    mask_r1 = cv2.inRange(hsv, LOWER_RED1, UPPER_RED1)
    mask_r2 = cv2.inRange(hsv, LOWER_RED2, UPPER_RED2)
    red_mask = cv2.bitwise_or(mask_r1, mask_r2)
    green_mask = cv2.inRange(hsv, LOWER_GREEN, UPPER_GREEN)

    red_mask = cv2.morphologyEx(red_mask, cv2.MORPH_OPEN, kernel)
    green_mask = cv2.morphologyEx(green_mask, cv2.MORPH_OPEN, kernel)

    detections = []

    for mask, label in [(red_mask, "RED"), (green_mask, "GREEN")]:
        contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        for cnt in contours:
            area = cv2.contourArea(cnt)
            if area < MIN_AREA_PX:
                continue

            approx = cv2.approxPolyDP(cnt, 0.04 * cv2.arcLength(cnt, True), True)
            if len(approx) < 4:
                continue

            x, y, w, h = cv2.boundingRect(cnt)
            aspect_ratio = w / float(h)
            if not (0.4 < aspect_ratio < 1.6):
                continue

            # --- distance estimation (pinhole model) ---
            if h <= 0:
                continue
            distance_mm = (PILLAR_REAL_HEIGHT_MM * FOCAL_LENGTH_PX) / h

            # --- lateral offset estimation ---
            cx = x + w / 2.0
            pixel_offset_from_center = cx - (FRAME_WIDTH / 2.0)
            angle_from_center_deg = (pixel_offset_from_center / (FRAME_WIDTH / 2.0)) * (
                HORIZONTAL_FOV_DEG / 2.0
            )
            lateral_offset_mm = distance_mm * np.tan(np.radians(angle_from_center_deg))

            detections.append(
                (label, distance_mm, lateral_offset_mm, area, (x, y, w, h))
            )

    detections.sort(key=lambda d: d[1])  # closest first
    return detections


class TemporalSmoother:
    """
    Requires CONFIRM_FRAMES consecutive agreeing frames before switching
    the reported state. Prevents single-frame noise from triggering or
    cancelling a maneuver.
    """

    def __init__(self, confirm_frames):
        self.confirm_frames = confirm_frames
        self.candidate_label = None
        self.candidate_count = 0
        self.confirmed = None  # (label, distance_mm, lateral_offset_mm) or None

    def update(self, closest_detection):
        if closest_detection is None:
            label = "NONE"
        else:
            label = closest_detection[0]

        if label == self.candidate_label:
            self.candidate_count += 1
        else:
            self.candidate_label = label
            self.candidate_count = 1

        if self.candidate_count >= self.confirm_frames:
            if label == "NONE":
                self.confirmed = None
            else:
                self.confirmed = closest_detection

        return self.confirmed


def format_message(confirmed):
    if confirmed is None:
        return "OBST:NONE\n"
    label, distance_mm, lateral_offset_mm, _, _ = confirmed
    return f"OBST:{label},{int(distance_mm)},{int(lateral_offset_mm)}\n"


def main():
    picam2 = Picamera2()
    video_config = picam2.create_video_configuration(
        main={"size": (FRAME_WIDTH, FRAME_HEIGHT), "format": "BGR888"}
    )
    picam2.configure(video_config)
    picam2.start()
    time.sleep(1)

    try:
        ser = serial.Serial(SERIAL_PORT, SERIAL_BAUD, timeout=1)
    except serial.SerialException as e:
        print(f"WARNING: could not open {SERIAL_PORT}: {e}")
        print("Continuing without serial output (debug mode only).")
        ser = None

    smoother = TemporalSmoother(CONFIRM_FRAMES)
    period = 1.0 / REPORT_HZ
    last_sent = None

    print("Obstacle vision running. Ctrl+C to stop.")

    try:
        while True:
            loop_start = time.time()

            frame_pil = picam2.capture_image("main")
            frame = np.array(frame_pil.convert("RGB"))
            frame = cv2.cvtColor(frame, cv2.COLOR_RGB2BGR)

            detections = detect_pillars(frame)

            # only consider pillars inside the trigger distance
            in_range = [d for d in detections if d[1] <= TRIGGER_DISTANCE_MM]
            closest = in_range[0] if in_range else None

            confirmed = smoother.update(closest)
            message = format_message(confirmed)

            if message != last_sent:
                print(f"[{time.strftime('%H:%M:%S')}] {message.strip()}")
                last_sent = message

            if ser is not None:
                ser.write(message.encode("utf-8"))

            if DEBUG_VISUAL:
                for (
                    label,
                    distance_mm,
                    lateral_offset_mm,
                    area,
                    (x, y, w, h),
                ) in detections:
                    color = (0, 0, 255) if label == "RED" else (0, 255, 0)
                    cv2.rectangle(frame, (x, y), (x + w, y + h), color, 2)
                    cv2.putText(
                        frame,
                        f"{label} d={int(distance_mm)}mm lat={int(lateral_offset_mm)}mm",
                        (x, y - 8),
                        cv2.FONT_HERSHEY_SIMPLEX,
                        0.5,
                        color,
                        2,
                    )
                cv2.imshow("Obstacle Vision", frame)
                if cv2.waitKey(1) & 0xFF == ord("q"):
                    break

            elapsed = time.time() - loop_start
            sleep_time = period - elapsed
            if sleep_time > 0:
                time.sleep(sleep_time)

    except KeyboardInterrupt:
        print("\nStopped.")
    finally:
        picam2.stop()
        if ser is not None:
            ser.close()
        if DEBUG_VISUAL:
            cv2.destroyAllWindows()


if __name__ == "__main__":
    main()
