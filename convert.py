'''
Downscale video feed to 1-bit grayscale and compress with run-length encoding.
'''

import ffmpeg
import struct
import os.path
import os
import shutil
from multiprocessing import Pool
from PIL import Image

CORES = 12
INPUT_FILE = 'badapple.webm'
FRAMES_FILE = 'BadApple.bin'
FRAMES_DIR = 'frames'
FRAME_WIDTH = 480
FRAME_HEIGHT = 360

def extract_compressed_frame(frame_filename):
    # Extract pixel data from each frame.
    frame = list(
        1 if (r > 127 or g  > 127 or b > 127 ) else 0 for (r,g,b) in
        Image.open(os.path.join(FRAMES_DIR, frame_filename), mode='r').getdata()
    )

    # Perform basic run-length code (1 byte value, 4 bytes length)
    runs = 0
    max_run_length = 65535
    compressed_frame = bytearray()
    pixels = FRAME_WIDTH * FRAME_HEIGHT
    run_length = 1
    index = 1
    while index < pixels:
        if frame[index] != frame[index-1]:
            compressed_frame.extend(struct.pack('<BH', frame[index-1], run_length))
            run_length = 1
            runs += 1
        elif run_length == max_run_length:
            compressed_frame.extend(struct.pack('<BH', frame[index], run_length))
            run_length = 1
            runs += 1
        else:
            run_length += 1
        index += 1
    if run_length > 0:
        compressed_frame.extend(struct.pack('<BH', frame[index-1], run_length))
        runs += 1

    # Encode number of runs in frame.
    full_frame = bytearray()
    full_frame.extend(struct.pack('<H', runs))
    full_frame.extend(compressed_frame)
    return full_frame

# Extract frame data.
if os.path.exists(FRAMES_DIR):
    shutil.rmtree(FRAMES_DIR)
os.mkdir(FRAMES_DIR)
(
    ffmpeg
    .input(INPUT_FILE)
    .filter('fps', fps=30)
    .output(f'{FRAMES_DIR}/%04d.jpg')
    .run()
)

# Compress frame data.
with Pool(CORES) as p:
    frame_paths = sorted(os.listdir(FRAMES_DIR))
    frames = p.map(extract_compressed_frame, frame_paths)

# Save compressed frame data.
with open(FRAMES_FILE, 'wb') as frames_file:
    for frame_data in frames:
        frames_file.write(frame_data)

# Clean up.
shutil.rmtree(FRAMES_DIR)
