import cv2
import numpy as np
from picamera2 import Picamera2
import time

# get the camera
picam2 = Picamera2()

# Set the window size, make BGR format
video_config = picam2.create_video_configuration(
    main={"size": (640, 480), "format": "BGR888"}
)
# imply the configurations
picam2.configure(video_config)


picam2.start() # start the camera
time.sleep(1)  # give the camera time to warm up

# will create a 5X5 array of 1's, which is the minimum required to consider as shape
kernel = np.ones((5, 5), np.uint8)

print("Press 'q' to quit.")

while True:

    frame_pil = picam2.capture_image("main")  # capture the photo
    frame = np.array(frame_pil.convert("RGB"))  # convert the photo to RGB formate
    frame = cv2.cvtColor(frame, cv2.COLOR_RGB2BGR)  # Convert from RGB to OpenCV format (BGR)

    # Convert to HSV format
    hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)

    # HSV range for red
    lower_red1 = np.array([0, 100, 100])
    upper_red1 = np.array([10, 255, 255])
    lower_red2 = np.array([160, 100, 100])
    upper_red2 = np.array([179, 255, 255])

    # HSV range for green
    lower_green = np.array([35, 40, 40])
    upper_green = np.array([85, 255, 255])

    # Masks for green and red (consider red/green white - other bits as black)
    mask1 = cv2.inRange(hsv, lower_red1, upper_red1)
    mask2 = cv2.inRange(hsv, lower_red2, upper_red2)
    red_mask = cv2.bitwise_or(mask1, mask2)
    green_mask = cv2.inRange(hsv, lower_green, upper_green)

    # ignore the shapes that are not in 5x5 range
    red_mask = cv2.morphologyEx(red_mask, cv2.MORPH_OPEN, kernel)
    green_mask = cv2.morphologyEx(green_mask, cv2.MORPH_OPEN, kernel)

    # count white pixels
    red_pixels = cv2.countNonZero(red_mask)
    green_pixels = cv2.countNonZero(green_mask)

    if red_pixels > 3000:
        print("🔴 Red detected!")
    if green_pixels > 3000:
        print("🟢 Green detected!")

    # pop up a window to show the masks
    cv2.imshow("Camera View", frame)
    cv2.imshow("Red Mask", red_mask)
    cv2.imshow("Green Mask", green_mask)

    # when 'q', exit
    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

# clean after finish
cv2.destroyAllWindows()
