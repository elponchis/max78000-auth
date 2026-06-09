import serial
import threading
import time
import os
from datetime import datetime

UART_PORT_A   = '/dev/serial/by-id/usb-ARM_DAPLink_CMSIS-DAP_044417019977ca2d00000000000000000000000097969906-if01'
UART_PORT_B   = '/dev/serial/by-id/usb-ARM_DAPLink_CMSIS-DAP_042317022187ca1b00000000000000000000000097969906-if01'
UART_BAUDRATE = 115200

class UARTBridge:
    def __init__(self, port=UART_PORT_A, baudrate=UART_BAUDRATE):
        self.port     = port
        self.baudrate = baudrate
        self.ser      = None
        self.running  = False
        self.callback = None

    def connect(self):
        try:
            self.ser = serial.Serial(self.port, self.baudrate, timeout=1)
            self.running = True
            self.thread = threading.Thread(target=self._read_loop, daemon=True)
            self.thread.start()
            print(f"UART connected: {self.port} @ {self.baudrate}")
            return True
        except Exception as e:
            print(f"UART connect error: {e}")
            return False

    def disconnect(self):
        self.running = False
        if self.ser and self.ser.is_open:
            self.ser.close()
        print("UART disconnected.")

    def send(self, message):
        if self.ser and self.ser.is_open:
            self.ser.write((message + '\n').encode())
            print(f"UART TX: {message}")
        else:
            print("UART not connected.")

    def _read_loop(self):
        receiving_image = False
        img_bytes = b''
        img_total = 0
        img_w = img_h = 0

        while self.running:
            try:
                if not (self.ser and self.ser.in_waiting):
                    time.sleep(0.01)
                    continue

                if receiving_image:
                    chunk = self.ser.read(self.ser.in_waiting)
                    img_bytes += chunk
                    print(f"  Receiving... {len(img_bytes)}/{img_total}")
                    if len(img_bytes) >= img_total:
                        filename = f"/tmp/capture_{datetime.now().strftime('%Y%m%d_%H%M%S')}.raw"
                        with open(filename, 'wb') as f:
                            f.write(img_bytes[:img_total])
                        print(f"Image saved: {filename} ({img_total} bytes, {img_w}x{img_h})")
                        if self.callback:
                            self.callback(f'IMAGE_SAVED:{filename}:{img_w}:{img_h}')
                        receiving_image = False
                        img_bytes = b''
                    continue

                line = self.ser.readline().decode('utf-8', errors='ignore').strip()
                if not line:
                    continue
                print(f"RAW[{len(line)}]: {repr(line)}")
                print(f"UART RX: {line}")

                if '###IMG###' in line:
                    idx = line.find('###IMG###')
                    after = line[idx + len('###IMG###'):].strip()
                    parts = after.split()
                    if len(parts) >= 3:
                        img_total = int(parts[0])
                        img_w = int(parts[1])
                        img_h = int(parts[2])
                        receiving_image = True
                        img_bytes = b''
                        print(f"Receiving image: {img_total} bytes, {img_w}x{img_h}")
                        continue

                if self.callback:
                    self.callback(line)
            except Exception as e:
                print(f"UART read error: {e}")
                break

    def set_callback(self, callback):
        self.callback = callback

# 싱글톤
uart_a = UARTBridge(port=UART_PORT_A)
uart_b = UARTBridge(port=UART_PORT_B)
uart = uart_a
