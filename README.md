# MAX78000 Multimodal Auth

MAX78000 기반 멀티모달 엣지 AI 본인 인증 시스템

## 구조
- `firmware/` : MAX78000 펌웨어 (MSDK 기반 C 코드)
- `training/` : ai8x-training 기반 모델 학습
- `synthesis/` : ai8x-synthesis 기반 모델 변환
- `app/` : React Native (Expo Go) 등록 인터페이스
- `server/` : 라즈베리파이 Flask 서버 (UART 브릿지 + REST API)

## 개발 환경
- MAX78000FTHR
- Ubuntu 22.04 (WSL2)
- Python 3.10 / PyTorch 2.3
- MSDK / ai8x-training / ai8x-synthesis
- React Native (Expo Go)
- Raspberry Pi 4B 2GB
