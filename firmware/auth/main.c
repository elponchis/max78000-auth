#define S_MODULE_NAME "main"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "board.h"
#include "mxc.h"
#include "mxc_device.h"
#include "mxc_delay.h"
#include "camera.h"
#include "icc.h"
#include "rtc.h"
#include "cnn_1.h"
#include "MAXCAM_Debug.h"
#include "facedetection.h"
#include "post_process.h"
#include "embeddings.h"
#include "faceID.h"
#include "gesture_auth.h"
#include "speaker_auth.h"
#include "utils.h"

#define CONSOLE_BAUD     115200
#define AUTH_UART        MXC_UART1
#define MAX_FAIL_COUNT   3
#define LOCK_DURATION_MS 30000

// 사용자별 보안 레벨 (Flash에서 로드, 임시로 하드코딩)
// 실제 구현 시 Flash에서 읽어야 함
#define DEFAULT_AUTH_LEVEL 1

// 랜덤 미션 클래스 (0~4)
// 실제 구현 시 라즈베리파이에서 UART로 수신
static int mission_class = 0;
static int current_auth_level = 2;  // 기본 level 2, START:N으로 동적 변경

extern void SD_Init(void);
extern volatile uint8_t face_detected;
volatile char names[1024][7];
mxc_uart_regs_t *CommUart;

void init_names(void)
{
    char default_names[DEFAULT_EMBS_NUM][7] = DEFAULT_NAMES;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-truncation"
    for (int i = 0; i < DEFAULT_EMBS_NUM; i++) {
        strncpy((char *)names[i], default_names[i], 7);
    }
#pragma GCC diagnostic pop
}

void auth_send_result(const char *result)
{
    int n = strlen(result);
    for (int i = 0; i < n; i++) {
        while (MXC_UART_GetTXFIFOAvailable(CommUart) == 0) {}
        MXC_UART_WriteCharacter(CommUart, result[i]);
    }
    while (MXC_UART_GetTXFIFOAvailable(CommUart) == 0) {}
    MXC_UART_WriteCharacter(CommUart, '\n');
}

// UART RX에서 1줄 읽기 (non-blocking, '\n' 또는 '\r' 만나면 종료)
static int auth_uart_read_line(char *buf, int max_len)
{
    static char rx_buf[64];
    static int rx_idx = 0;
    
    int available = MXC_UART_GetRXFIFOAvailable(CommUart);
    while (available > 0 && rx_idx < (int)sizeof(rx_buf) - 1) {
        unsigned char c;
        MXC_UART_ReadRXFIFO(CommUart, &c, 1);
        available--;
        if (c == '\n' || c == '\r') {
            if (rx_idx > 0) {
                rx_buf[rx_idx] = '\0';
                int len = rx_idx;
                if (len >= max_len) len = max_len - 1;
                memcpy(buf, rx_buf, len);
                buf[len] = '\0';
                rx_idx = 0;
                return len;
            }
        } else {
            rx_buf[rx_idx++] = c;
        }
    }
    return 0;
}

// 명령 모드 enum
typedef enum { MODE_IDLE, MODE_AUTH, MODE_ENROLL } auth_mode_t;
static auth_mode_t current_mode = MODE_IDLE;

void auth_lockout(void)
{
    auth_send_result("AUTH_LOCKED");
    MXC_Delay(MXC_DELAY_MSEC(LOCK_DURATION_MS));
    auth_send_result("AUTH_UNLOCKED");
}

int main(void)
{
    int error = 0;

    int id;
    int dma_channel;
    mxc_uart_regs_t *ConsoleUart;

    MXC_Delay(MXC_DELAY_SEC(2));

#ifdef BOARD_FTHR_REVA
    MXC_Delay(200000);
    Camera_Power(POWER_ON);
#endif

    MXC_ICC_Enable(MXC_ICC0);
    MXC_SYS_Clock_Select(MXC_SYS_CLOCK_IPO);
    SystemCoreClockUpdate();

    ConsoleUart = MXC_UART_GET_UART(CONSOLE_UART);
    CommUart = ConsoleUart;
    if ((error = MXC_UART_Init(ConsoleUart, CONSOLE_BAUD, MXC_UART_IBRO_CLK)) != E_NO_ERROR) {
        PR_ERR("UART Init Error: %d\n", error);
        return error;
    }

    // 인증 결과 전송용 UART (라즈베리파이 연결)

    init_names();

    MXC_RTC_Init(0, 0);
    MXC_RTC_Start();
    srand(MXC_RTC_GetSecond());
    MXC_DMA_Init();
    dma_channel = MXC_DMA_AcquireChannel();

    error = camera_init(CAMERA_FREQ);
    if (error) {
        PR_ERR("Camera init error: %d", error);
        return error;
    }


    camera_get_product_id(&id);
    camera_get_manufacture_id(&id);

    error = camera_setup(IMAGE_XRES, IMAGE_YRES, PIXFORMAT_RGB565,
                         FIFO_FOUR_BYTE, USE_DMA, dma_channel);
    if (error) {
        PR_ERR("Camera setup error: %d", error);
        return error;
    }

    camera_write_reg(0x11, 0x80);
    camera_set_vflip(0);

    MXC_Delay(MXC_DELAY_SEC(5));
    SD_Init();
    speaker_auth_init();


    int fail_count = 0;

    while (1) {

	char cmd[64];
        if (auth_uart_read_line(cmd, sizeof(cmd)) > 0) {
            if (strncmp(cmd, "START", 5) == 0) {
                current_mode = MODE_AUTH;
                // "START:N" 형식이면 N을 파싱해서 level 설정
                if (cmd[5] == ':') {
                    int lvl = cmd[6] - '0';
                    if (lvl >= 1 && lvl <= 2) current_auth_level = lvl;
                }
            } else if (strncmp(cmd, "CAPTURE", 7) == 0) {
                current_mode = MODE_ENROLL;
            }
        }

/*	if (current_mode == MODE_ENROLL) {
            current_mode = MODE_IDLE;
	    MXC_Delay(MXC_DELAY_MSEC(200));
            // 1장 캡처 → UART로 전송
            uint8_t *raw;
            uint32_t imglen, w, h;
            camera_start_capture_image();
            while (!camera_is_image_rcv()) {}
            camera_get_image(&raw, &imglen, &w, &h);

            // 헤더 송신: "*IMG* %d %d %d\n"
            char hdr[64];
            snprintf(hdr, sizeof(hdr), "###IMG### %lu %lu %lu", imglen, w, h);
            auth_send_result(hdr);
	    MXC_Delay(MXC_DELAY_MSEC(500));

            // raw 데이터 송신
            int len = imglen;
            MXC_UART_Write(CommUart, raw, &len);

            auth_send_result("###IMG_END###");
            continue;
        } */
	if (current_mode == MODE_ENROLL) {
            current_mode = MODE_IDLE;
            MXC_Delay(MXC_DELAY_MSEC(500));

            uint8_t *raw;
            uint32_t imglen, w, h;
            camera_start_capture_image();
            while (!camera_is_image_rcv()) {}
            camera_get_image(&raw, &imglen, &w, &h);
	    
	    char hdr[64];
	    int hdr_len = snprintf(hdr, sizeof(hdr), "###IMG### %lu %lu %lu\n", imglen, w, h);
            for (int i = 0; i < hdr_len; i++) {
                while (MXC_UART_GetTXFIFOAvailable(CommUart) == 0) {}
                MXC_UART_WriteCharacter(CommUart, hdr[i]);
            }
            MXC_Delay(MXC_DELAY_MSEC(500));
            
            // raw 송신
	    for (uint32_t i = 0; i < imglen; i++) {
                while (MXC_UART_GetTXFIFOAvailable(CommUart) == 0) {}
                MXC_UART_WriteCharacter(CommUart, raw[i]);
            }
            continue;
        }

        if (current_mode != MODE_AUTH) {
            MXC_Delay(MXC_DELAY_MSEC(100));
            continue;
        }
        
        // 인증 1회 끝나면 IDLE 복귀
        current_mode = MODE_IDLE;

        LED_On(0);

        // ① 얼굴 감지
        int face_retry;
	for (face_retry = 0; face_retry < 50; face_retry++) {
	    face_detection();
	    if (face_detected) break;
	    MXC_Delay(MXC_DELAY_MSEC(200));
	}

        LED_Off(0);
        if (!face_detected) {
            PR_INFO("No face detected.");
	    auth_send_result("AUTH_FAIL");
            continue;
        }

        LED_On(1);

        // ② 얼굴 인식
        face_id();
	mission_class = rand() % 5;
        face_detected = 0;

        // 레벨 1 이상: 제스처 인증
        if (current_auth_level >= 1) {

            // 미션 전송 (라즈베리파이 → 앱에서 표시)
            char mission_msg[32];
            snprintf(mission_msg, sizeof(mission_msg), "MISSION:%d", mission_class);
            auth_send_result(mission_msg);
            // Target 먼저 알려주고 3초 대기 후 캡처
            MXC_Delay(MXC_DELAY_SEC(5));
            // 카메라 해상도 64x64로 변경
            camera_setup(64, 64, PIXFORMAT_RGB565, FIFO_FOUR_BYTE, USE_DMA, dma_channel);
            // 최대 5회 재시도
            int gesture_done = 0;
            for (int retry = 0; retry < 5; retry++) {
                if (gesture_auth(mission_class)) {
                    gesture_done = 1;
                    break;
                }
                PR_INFO("Gesture retry %d/5", retry + 1);
                MXC_Delay(MXC_DELAY_SEC(5));
            }
            // 원래 해상도로 복구
            camera_setup(IMAGE_XRES, IMAGE_YRES, PIXFORMAT_RGB565,
                         FIFO_FOUR_BYTE, USE_DMA, dma_channel);
            if (!gesture_done) {
                fail_count++;
                PR_INFO("Gesture FAIL (%d/3)", fail_count);
                auth_send_result("AUTH_FAIL");
                if (fail_count >= MAX_FAIL_COUNT) {
                    auth_lockout();
                    fail_count = 0;
                }
                continue;
            }

            // 원래 해상도로 복구
            camera_setup(IMAGE_XRES, IMAGE_YRES, PIXFORMAT_RGB565,
                         FIFO_FOUR_BYTE, USE_DMA, dma_channel);
        }

        // 레벨 2: 화자는 라즈베리파이가 보드B로 처리. 신호만 송신.
        if (current_auth_level >= 2) {
            auth_send_result("SPEAKER_START");
            fail_count = 0;
            LED_Off(1);
            continue;
            // 라즈베리파이가 보드B에 'G' 보내고 결과 비교 후 SUCCESS/FAIL 결정
            // 보드A는 여기서 다음 미션으로 넘어가지 않고 마침
        }

        // 인증 성공
        fail_count = 0;
        PR_INFO("AUTH SUCCESS!");
        auth_send_result("AUTH_SUCCESS");
        LED_Off(1);

        // 다음 미션 클래스 변경 (간단한 순환)
    }

    return 0;
}
