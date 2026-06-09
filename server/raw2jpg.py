import sys
import numpy as np
from PIL import Image

raw_path = sys.argv[1]
out_path = sys.argv[2]
w, h = int(sys.argv[3]), int(sys.argv[4])

with open(raw_path, 'rb') as f:
    data = f.read()

arr = np.frombuffer(data, dtype='>u2').reshape(h, w)

r = ((arr & 0xF800) >> 8).astype(np.uint8)
g = ((arr & 0x07E0) >> 3).astype(np.uint8)
b = ((arr & 0x001F) << 3).astype(np.uint8)

img = np.stack([r, g, b], axis=-1)
Image.fromarray(img).save(out_path)
print(f"Saved {out_path}")
