"""
Color detection test with visual overlay.
Run over SSH with X11 forwarding: ssh -X abd@<PI_IP>
Shows a live window with a colored dot where red/green is detected.
Press 'q' to quit.
"""

import cv2
import numpy as np
from picamera2 import Picamera2
import time

picam2 = Picamera2()
video_config = picam2.create_video_configuration(
    main={"size": (640, 480), "format": "BGR888"}
)
picam2.configure(video_config)
picam2.start()
time.sleep(1)

kernel = np.ones((5, 5), np.uint8)

print("Running. Press 'q' to quit.")


def detect_colors(frame):
    hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)

    lower_red1 = np.array([0, 100, 100])
    upper_red1 = np.array([10, 255, 255])
    lower_red2 = np.array([160, 100, 100])
    upper_red2 = np.array([179, 255, 255])
    lower_green = np.array([40, 70, 70])
    upper_green = np.array([85, 255, 255])

    mask1 = cv2.inRange(hsv, lower_red1, upper_red1)
    mask2 = cv2.inRange(hsv, lower_red2, upper_red2)
    red_mask = cv2.bitwise_or(mask1, mask2)
    green_mask = cv2.inRange(hsv, lower_green, upper_green)

    red_mask = cv2.morphologyEx(red_mask, cv2.MORPH_OPEN, kernel)
    green_mask = cv2.morphologyEx(green_mask, cv2.MORPH_OPEN, kernel)

    detections = []

    for mask, color, label in [
        (red_mask, (0, 0, 255), "RED"),
        (green_mask, (0, 255, 0), "GREEN"),
    ]:
        contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        for cnt in contours:
            area = cv2.contourArea(cnt)
            if area < 2000:
                continue
            approx = cv2.approxPolyDP(cnt, 0.04 * cv2.arcLength(cnt, True), True)
            if len(approx) >= 4:
                x, y, w, h = cv2.boundingRect(cnt)
                aspect_ratio = w / float(h)
                if 0.6 < aspect_ratio < 1.4:
                    cx = x + w // 2
                    cy = y + h // 2
                    detections.append((label, color, cx, cy, x, y, w, h, area))

    return detections


def draw_overlay(frame, detections):
    for label, color, cx, cy, x, y, w, h, area in detections:
        # bounding box
        cv2.rectangle(frame, (x, y), (x + w, y + h), color, 2)
        # center dot
        cv2.circle(frame, (cx, cy), 8, color, -1)
        # label + area
        cv2.putText(
            frame,
            f"{label} ({int(area)}px)",
            (x, y - 8),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.6,
            color,
            2,
        )

    if not detections:
        cv2.putText(
            frame, "NONE", (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 1.0, (200, 200, 200), 2
        )

    return frame


def main():
    while True:
        frame_pil = picam2.capture_image("main")
        frame = np.array(frame_pil.convert("RGB"))
        frame = cv2.cvtColor(frame, cv2.COLOR_RGB2BGR)

        detections = detect_colors(frame)
        frame = draw_overlay(frame, detections)

        cv2.imshow("Detection", frame)

        if cv2.waitKey(1) & 0xFF == ord("q"):
            break


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nStopped.")
    finally:
        picam2.stop()
        cv2.destroyAllWindows()
