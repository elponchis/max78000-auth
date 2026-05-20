import serial
import threading
import time

UART_PORT     = '/dev/ttyAMA0'  # 라즈베리파이 기본 UART
UART_BAUDRATE = 115200

class UARTBridge:
    def __init__(self, port=UART_PORT, baudrate=UART_BAUDRATE):
        self.port     = port
        self.baudrate = baudrate
        self.ser      = None
        self.running  = False
        self.callback = None  # 메시지 수신 콜백

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
        while self.running:
            try:
                if self.ser and self.ser.in_waiting:
                    line = self.ser.readline().decode('utf-8', errors='ignore').strip()
                    if line:
                        print(f"UART RX: {line}")
                        if self.callback:
                            self.callback(line)
                else:
                    time.sleep(0.01)
            except Exception as e:
                print(f"UART read error: {e}")
                break

    def set_callback(self, callback):
        self.callback = callback

# 싱글톤
uart = UARTBridge()
