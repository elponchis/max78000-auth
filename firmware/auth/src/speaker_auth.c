#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "mxc.h"
#include "cnn_5.h"
#include "speaker_auth.h"

#define SPEAKER_EMBEDDING_DIM 64
#define SPEAKER_THRESHOLD     90  // cosine similarity 임계값 (Q7, 0~127)

static int32_t ml_data[32];  // CNN_NUM_OUTPUTS = 32 (두 개의 int16을 하나의 int32로 패킹)
static int8_t  spk_embedding[SPEAKER_EMBEDDING_DIM];

// voxsv main.c에서 가져온 정규화 함수
static float sv_sqrt(float v)
{
    float x = v > 1.0f ? v : 1.0f;
    int i;
    if (v <= 0.0f) return 0.0f;
    for (i = 0; i < 8; i++) x = 0.5f * (x + v / x);
    return x;
}

static int8_t sv_clamp_q7(float v)
{
    if (v >  127.0f) return  127;
    if (v < -128.0f) return -128;
    return (int8_t)(v >= 0.0f ? v + 0.5f : v - 0.5f);
}

static void normalize_embedding(const int32_t *packed, int8_t *dst)
{
    const int16_t *src = (const int16_t *)packed;
    float sum = 0.0f, scale;
    int i;
    for (i = 0; i < SPEAKER_EMBEDDING_DIM; i++)
        sum += (float)src[i] * (float)src[i];
    if (sum <= 0.0f) { memset(dst, 0, SPEAKER_EMBEDDING_DIM); return; }
    scale = 128.0f / sv_sqrt(sum);
    for (i = 0; i < SPEAKER_EMBEDDING_DIM; i++)
        dst[i] = sv_clamp_q7((float)src[i] * scale);
}

// 등록된 화자 임베딩 (Flash에 저장, 임시 하드코딩)
static int8_t enrolled_embedding[SPEAKER_EMBEDDING_DIM] = {0};
static int enrolled = 0;

// cosine similarity (Q7 정수 내적)
static int32_t cosine_similarity(const int8_t *a, const int8_t *b)
{
    int32_t dot = 0;
    int i;
    for (i = 0; i < SPEAKER_EMBEDDING_DIM; i++)
        dot += (int32_t)a[i] * (int32_t)b[i];
    return dot >> 7;  // Q7 스케일 조정
}

// FBank 입력 로드 (추후 실제 마이크 파이프라인으로 교체)
static void speaker_load_input(void)
{
    // TODO: 실제 마이크 → FBank 변환 파이프라인 연결
    // 현재는 샘플 데이터로 테스트
    memset((uint32_t *)0x50400000, 0, 2560 * 4);
}

// 화자 임베딩 추출 실행
int speaker_run(void)
{
    cnn_enable(MXC_S_GCR_PCLKDIV_CNNCLKSEL_PCLK,
               MXC_S_GCR_PCLKDIV_CNNCLKDIV_DIV1);
    cnn_5_init();
    cnn_5_load_weights();
    cnn_5_load_bias();
    cnn_5_configure();
    speaker_load_input();
    cnn_5_start();
    while (cnn_time == 0) {}
    cnn_5_unload((uint32_t *)ml_data);
    cnn_disable();
    normalize_embedding(ml_data, spk_embedding);
    return 0;
}

// 화자 등록
void speaker_enroll(void)
{
    speaker_run();
    memcpy(enrolled_embedding, spk_embedding, SPEAKER_EMBEDDING_DIM);
    enrolled = 1;
    printf("Speaker enrolled.\n");
}

// 화자 인증
int speaker_auth(void)
{
    int32_t sim;
    if (!enrolled) {
        printf("No enrolled speaker.\n");
        return 0;
    }
    speaker_run();
    sim = cosine_similarity(spk_embedding, enrolled_embedding);
    printf("Speaker similarity: %ld\n", sim);
    if (sim > SPEAKER_THRESHOLD) {
        printf("Speaker AUTH OK\n");
        return 1;
    }
    printf("Speaker AUTH FAIL\n");
    return 0;
}
