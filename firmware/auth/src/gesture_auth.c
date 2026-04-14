#include <stdio.h>
#include <stdint.h>
#include "mxc.h"
#include "camera.h"
#include "cnn_4.h"
#include "gesture_auth.h"
#include "utils.h"

#define GESTURE_NUM_CLASSES 5
#define GESTURE_THRESHOLD   80  // softmax 출력 임계값 (0~127)

// 클래스 이름
static const char *gesture_names[GESTURE_NUM_CLASSES] = {
    "fist", "open", "index", "victory", "thumbsdown"
};

// CNN 출력 버퍼
static uint32_t gesture_output[GESTURE_NUM_CLASSES];

// 카메라에서 64x64 이미지를 CNN에 로드
static void gesture_load_input(void)
{
    uint8_t *raw;
    uint32_t imglen, w, h;

    camera_start_capture_image();
    while (!camera_is_image_rcv()) {}
    camera_get_image(&raw, &imglen, &w, &h);

    // 64x64 RGB565 → CNN 입력 (정규화: 0~255 → -128~127)
    uint8_t *ptr = raw;
    for (int i = 0; i < 64 * 64; i++) {
        uint16_t rgb565 = (ptr[1] << 8) | ptr[0];
        ptr += 2;

        int8_t r = ((rgb565 & 0xF800) >> 8) - 128;
        int8_t g = ((rgb565 & 0x07E0) >> 3) - 128;
        int8_t b = ((rgb565 & 0x001F) << 3) - 128;

        // CNN FIFO에 RGB 데이터 로드
        uint32_t rgb = (uint8_t)r | ((uint8_t)g << 8) | ((uint8_t)b << 16);
        while (((*((volatile uint32_t *)0x50000004) & 1)) != 0);
        *((volatile uint32_t *)0x50000008) = rgb;
    }
}

// 제스처 인식 실행, 인식된 클래스 인덱스 반환 (-1: 실패)
int gesture_run(void)
{
    // CNN 초기화 및 가중치 로드
    cnn_enable(MXC_S_GCR_PCLKDIV_CNNCLKSEL_PCLK, MXC_S_GCR_PCLKDIV_CNNCLKDIV_DIV1);
    cnn_4_init();
    cnn_4_load_weights();
    cnn_4_load_bias();
    cnn_4_configure();

    // 이미지 로드 및 추론
    gesture_load_input();
    cnn_4_start();

    // 결과 대기
    while (cnn_time == 0) {}

    // 결과 언로드
    cnn_4_unload(gesture_output);
    cnn_disable();

    // 최대값 클래스 찾기
    int max_idx = 0;
    uint32_t max_val = gesture_output[0];
    for (int i = 1; i < GESTURE_NUM_CLASSES; i++) {
        if (gesture_output[i] > max_val) {
            max_val = gesture_output[i];
            max_idx = i;
        }
    }

    printf("Gesture: %s (%lu)\n", gesture_names[max_idx], max_val);

    if (max_val > GESTURE_THRESHOLD) {
        return max_idx;
    }
    return -1;
}

// 미션 제스처와 일치하는지 확인
int gesture_auth(int mission_class)
{
    int result = gesture_run();
    if (result == mission_class) {
        printf("Gesture AUTH OK: %s\n", gesture_names[mission_class]);
        return 1;
    }
    printf("Gesture AUTH FAIL\n");
    return 0;
}
