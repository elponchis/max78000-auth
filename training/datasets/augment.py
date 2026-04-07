import os
import random
from PIL import Image, ImageEnhance
import numpy as np

CLASSES = ["fist", "open", "victory", "index", "thumbsdown"]
BASE = os.path.expanduser("~/max78000-auth/training/datasets/gesture/real")

def augment_image(img):
    # 밝기 (0.5 ~ 1.5)
    img = ImageEnhance.Brightness(img).enhance(random.uniform(0.5, 1.5))
    # 대비 (0.7 ~ 1.3)
    img = ImageEnhance.Contrast(img).enhance(random.uniform(0.7, 1.3))
    # 회전 (-20 ~ +20도)
    img = img.rotate(random.uniform(-20, 20))
    # 좌우 반전 (50% 확률)
    if random.random() > 0.5:
        img = img.transpose(Image.FLIP_LEFT_RIGHT)
    # 노이즈
    arr = np.array(img).astype(np.int16)
    arr = np.clip(arr + np.random.randint(-20, 20, arr.shape), 0, 255).astype(np.uint8)
    return Image.fromarray(arr)

def augment_class(class_name, target=200):
    save_dir = os.path.join(BASE, class_name)
    existing = sorted([f for f in os.listdir(save_dir) if f.endswith(".png")])
    current = len(existing)

    if current >= target:
        print(f"{class_name}: 이미 {current}장, 스킵")
        return

    need = target - current
    print(f"{class_name}: {current}장 -> {target}장 ({need}장 증강 중...)")

    idx = current
    for i in range(need):
        src = os.path.join(save_dir, random.choice(existing))
        img = Image.open(src).convert("RGB")
        aug = augment_image(img)
        aug.save(os.path.join(save_dir, f"{class_name}_{idx:04d}.png"))
        idx += 1

    print(f"{class_name}: 완료! ({target}장)")

if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("--class", dest="class_name", choices=CLASSES + ["all"], default="all")
    parser.add_argument("--target", type=int, default=200)
    args = parser.parse_args()

    classes = CLASSES if args.class_name == "all" else [args.class_name]
    for c in classes:
        augment_class(c, args.target)
    print("\n전체 완료!")
