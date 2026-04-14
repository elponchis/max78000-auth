"""
weights.h의 KERNELS 데이터를 SD카드용 .bin 파일로 변환
"""
import re
import struct
import sys

def weights_h_to_bin(input_h, output_bin):
    with open(input_h, 'r') as f:
        content = f.read()

    # KERNELS { ... } 추출
    match = re.search(r'#define KERNELS \{(.*?)\}', content, re.DOTALL)
    if not match:
        print("ERROR: KERNELS not found!")
        return

    data_str = match.group(1)
    # 16진수 값들 추출
    values = re.findall(r'0x[0-9a-fA-F]+', data_str)
    print(f"Found {len(values)} values")

    with open(output_bin, 'wb') as f:
        for v in values:
            f.write(struct.pack('<I', int(v, 16)))

    print(f"Saved to {output_bin} ({len(values)*4} bytes)")

if __name__ == "__main__":
    weights_h_to_bin(
        '/home/max78000/max78000-auth/firmware/auth/include/weights_4.h',
        '/home/max78000/max78000-auth/training/weights_4.bin'
    )
