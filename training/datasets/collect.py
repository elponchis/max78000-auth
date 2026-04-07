import serial
import argparse
import os
import sys
import time
sys.path.append(os.path.expanduser("~/msdk/Examples/MAX78000/ImgCapture/utils"))
from imgConverter import convert

CLASSES = ["fist", "open", "victory", "index", "thumbsdown"]

def collect(port, baudrate, class_name, count, fake=False):
    base = os.path.expanduser("~/max78000-auth/training/datasets/gesture")
    split = "fake" if fake else "real"
    save_dir = os.path.join(base, split, class_name)
    os.makedirs(save_dir, exist_ok=True)

    existing = [f for f in os.listdir(save_dir) if f.endswith(".png")]
    start_idx = len(existing)

    print(f"\n[start] class: {class_name} | type: {split} | goal: {count}")
    print(f"save: {save_dir}")
    print(f"existing: {start_idx} -> target: {start_idx + count}\n")

    s = serial.Serial(port, baudrate, timeout=5)
    s.reset_input_buffer()
    s.reset_output_buffer()

    print("connecting...")
    while True:
        line = s.readline().decode("ascii", errors="ignore").strip()
        if line == "*SYNC*":
            s.write(b"*SYNC*")
            s.flush()
            resp = s.readline().decode("ascii", errors="ignore").strip()
            if "Established" in resp:
                print("connected!\n")
                time.sleep(0.5)
                while s.in_waiting:
                    s.readline()
                break

    s.write(b"imgres 64 64\n")
    s.flush()
    time.sleep(0.3)
    while s.in_waiting:
        s.readline()

    saved = 0
    while saved < count:
        input(f"[{saved+1}/{count}] ready? press Enter...")
        s.write(b"capture\n")
        s.flush()

        img_data = None
        fmt = width = height = length = None

        while True:
            line = s.readline().decode("ascii", errors="ignore").strip()
            if not line:
                continue
            print(f"  MCU: {line}")
            if "*IMG*" in line:
                parts = line.split()
                fmt = parts[1]
                length = int(parts[2])
                width = int(parts[3])
                height = int(parts[4])
                raw = b""
                while len(raw) < length:
                    chunk = s.read(length - len(raw))
                    raw += chunk
                img_data = raw
            elif "Done" in line and img_data:
                break

        if img_data:
            filename = f"{class_name}_{start_idx + saved:04d}.png"
            filepath = os.path.join(save_dir, filename)
            convert(img_data, filepath, width, height, fmt)
            print(f"  saved: {filename}\n")
            saved += 1
        else:
            print("  failed, retry...\n")

    s.close()
    print(f"\ndone! {count} images saved -> {save_dir}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="/dev/ttyACM0")
    parser.add_argument("--baudrate", type=int, default=921600)
    parser.add_argument("--class", dest="class_name", choices=CLASSES, required=True)
    parser.add_argument("--count", type=int, default=50)
    parser.add_argument("--fake", action="store_true")
    args = parser.parse_args()
    collect(args.port, args.baudrate, args.class_name, args.count, args.fake)
