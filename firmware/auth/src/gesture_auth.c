#include <stdio.h>
#include <stdint.h>
#include "mxc.h"
#include "camera.h"
#include "cnn_4.h"
#include "gesture_auth.h"
#include "utils.h"
#include "../../../synthesis/gesture/gesture/sampledata.h"

#define GESTURE_NUM_CLASSES 5
#define GESTURE_THRESHOLD   80  // softmax 출력 임계값 (0~127)

// 클래스 이름
static const char *gesture_names[GESTURE_NUM_CLASSES] = {
    "fist", "open", "victory", "index", "thumbsdown"
};

// CNN 출력 버퍼
static uint32_t gesture_output[40];
static const uint32_t sample_in0[]  = SAMPLE_INPUT_0;
static const uint32_t sample_in16[] = SAMPLE_INPUT_16;
static const uint32_t sample_in32[] = SAMPLE_INPUT_32;

// 카메라 → CNN 메모리 직접 배치
static void gesture_load_input(void)
{
    /*uint8_t *raw;
    uint32_t imglen, w, h;
    camera_start_capture_image();
    while (!camera_is_image_rcv()) {}
    camera_get_image(&raw, &imglen, &w, &h);
    
    volatile uint32_t *r_addr = (volatile uint32_t *) 0x50400000;
    volatile uint32_t *g_addr = (volatile uint32_t *) 0x50800000;
    volatile uint32_t *b_addr = (volatile uint32_t *) 0x50c00000;
    
    uint8_t *ptr = raw;
    for (int i = 0; i < 1024; i++) {
        uint32_t r32 = 0, g32 = 0, b32 = 0;
        for (int k = 0; k < 4; k++) {
            uint16_t rgb565 = (ptr[1] << 8) | ptr[0];
            ptr += 2;
            uint8_t r = (uint8_t)((((rgb565 & 0xF800) >> 11) * 8) - 128);
            uint8_t g = (uint8_t)((((rgb565 & 0x07E0) >> 5) * 4) - 128);
            uint8_t b = (uint8_t)(((rgb565 & 0x001F) * 8) - 128);
            r32 |= ((uint32_t)r) << (k * 8);
            g32 |= ((uint32_t)g) << (k * 8);
            b32 |= ((uint32_t)b) << (k * 8);
        }
        *r_addr++ = r32;
        *g_addr++ = g32;
        *b_addr++ = b32;
    }*/
    for (int i = 0; i < 1024; i++) {
        ((volatile uint32_t *) 0x50400000)[i] = sample_in0[i];
        ((volatile uint32_t *) 0x50800000)[i] = sample_in16[i];
        ((volatile uint32_t *) 0x50c00000)[i] = sample_in32[i];
    }
}

// 제스처 인식 실행, 인식된 클래스 인덱스 반환 (-1: 실패)
int gesture_run(void)
{
    cnn_enable(MXC_S_GCR_PCLKDIV_CNNCLKSEL_PCLK, MXC_S_GCR_PCLKDIV_CNNCLKDIV_DIV1);
    cnn_4_init();
    cnn_4_load_weights();
    cnn_4_load_bias();
    cnn_4_configure();
    gesture_load_input();
    cnn_4_start();
    while (cnn_time == 0) {}
    cnn_4_unload(gesture_output);
    cnn_disable();

    uint16_t *out16 = (uint16_t *)gesture_output;
    int32_t class_score[GESTURE_NUM_CLASSES] = {0};
    for (int c = 0; c < GESTURE_NUM_CLASSES; c++) {
        int32_t sum = 0;
        for (int j = 0; j < 16; j++) {
            sum += (int8_t)(out16[c * 16 + j] & 0xFF);
        }
        class_score[c] = sum;
    }
    for (int i = 0; i < GESTURE_NUM_CLASSES; i++) {
        printf("  [%s]=%ld", gesture_names[i], class_score[i]);
    }
    printf("\n");
    int max_idx = 0;
    int32_t max_val = class_score[0];
    for (int i = 1; i < GESTURE_NUM_CLASSES; i++) {
        if (class_score[i] > max_val) {
            max_val = class_score[i];
            max_idx = i;
        }
    }
    printf("Gesture: %s (score=%ld)\n", gesture_names[max_idx], max_val);
    if (max_val <= -2048) return -1;
    return max_idx;
}

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
