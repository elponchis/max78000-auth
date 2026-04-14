#define S_MODULE_NAME "main"

#include <stdio.h>
#include <stdint.h>
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
    MXC_UART_WriteTXFIFO(CommUart, (uint8_t*)result, strlen(result));
    MXC_UART_WriteTXFIFO(CommUart, (uint8_t*)"\n", 1);
}

void auth_lockout(void)
{
    PR_INFO("AUTH LOCKOUT - waiting %dms", LOCK_DURATION_MS);
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
    if ((error = MXC_UART_Init(ConsoleUart, CONSOLE_BAUD, MXC_UART_IBRO_CLK)) != E_NO_ERROR) {
        PR_ERR("UART Init Error: %d\n", error);
        return error;
    }

    // 인증 결과 전송용 UART (라즈베리파이 연결)
    CommUart = MXC_UART_GET_UART(1);
    MXC_UART_Init(CommUart, CONSOLE_BAUD, MXC_UART_IBRO_CLK);

    PR_INFO("Edge-Auth System Starting...");
    init_names();

    MXC_RTC_Init(0, 0);
    MXC_RTC_Start();
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

    SD_Init();

    PR_INFO("Edge-Auth Ready. Auth Level: %d", DEFAULT_AUTH_LEVEL);

    int fail_count = 0;

    while (1) {
        PR_INFO("--- Waiting for face ---");
        LED_On(0);

        // ① 얼굴 감지
        face_detection();
        LED_Off(0);

        if (!face_detected) {
            PR_INFO("No face detected.");
            continue;
        }

        PR_INFO("Face detected! Running FaceID...");
        LED_On(1);

        // ② 얼굴 인식
        face_id();
        face_detected = 0;

        // 레벨 1 이상: 제스처 인증
        if (DEFAULT_AUTH_LEVEL >= 1) {
            PR_INFO("Running gesture auth (mission: %d)...", mission_class);

            // 미션 전송 (라즈베리파이 → 앱에서 표시)
            char mission_msg[32];
            snprintf(mission_msg, sizeof(mission_msg), "MISSION:%d", mission_class);
            auth_send_result(mission_msg);

            // 카메라 해상도 64x64로 변경
            camera_setup(64, 64, PIXFORMAT_RGB565, FIFO_FOUR_BYTE, USE_DMA, dma_channel);

            if (!gesture_auth(mission_class)) {
                fail_count++;
                PR_INFO("Gesture FAIL (%d/3)", fail_count);
                auth_send_result("AUTH_FAIL");

                if (fail_count >= MAX_FAIL_COUNT) {
                    auth_lockout();
                    fail_count = 0;
                }

                // 원래 해상도로 복구
                camera_setup(IMAGE_XRES, IMAGE_YRES, PIXFORMAT_RGB565,
                             FIFO_FOUR_BYTE, USE_DMA, dma_channel);
                continue;
            }

            // 원래 해상도로 복구
            camera_setup(IMAGE_XRES, IMAGE_YRES, PIXFORMAT_RGB565,
                         FIFO_FOUR_BYTE, USE_DMA, dma_channel);
        }

        // 인증 성공
        fail_count = 0;
        PR_INFO("AUTH SUCCESS!");
        auth_send_result("AUTH_SUCCESS");
        LED_Off(1);

        // 다음 미션 클래스 변경 (간단한 순환)
        mission_class = (mission_class + 1) % 5;
    }

    return 0;
}
