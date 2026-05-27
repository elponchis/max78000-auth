#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "mxc.h"
#include "i2s.h"
#include "i2s_regs.h"
#include "cnn_5.h"
#include "mel_features.h"
#include "speaker_db.h"
#include "speaker_auth.h"

#define SAMPLE_RATE         16000
#define I2S_RX_BUFFER_SIZE  512
#define VAD_THRESHOLD_HIGH  350
#define VAD_THRESHOLD_LOW   100

static int16_t  g_audio[MEL_AUDIO_SAMPLES];
static uint32_t g_features_u32[MEL_N_MELS * MEL_N_FRAMES / 4];
static int32_t  g_i2s_rx[I2S_RX_BUFFER_SIZE];
static volatile int g_i2s_flag = 0;
static int32_t  ml_data[32];
static int8_t   spk_embedding[64];

static void i2s_isr(void) { g_i2s_flag = 1; }

static void i2s_setup(void)
{
    mxc_i2s_req_t req;
    memset(g_i2s_rx, 0, sizeof(g_i2s_rx));
    req.wordSize    = MXC_I2S_DATASIZE_WORD;
    req.sampleSize  = MXC_I2S_SAMPLESIZE_THIRTYTWO;
    req.justify     = MXC_I2S_MSB_JUSTIFY;
    req.wsPolarity  = MXC_I2S_POL_NORMAL;
    req.channelMode = MXC_I2S_INTERNAL_SCK_WS_0;
    req.stereoMode  = MXC_I2S_MONO_LEFT_CH;
    req.bitOrder    = MXC_I2S_MSB_FIRST;
    req.clkdiv      = 5;
    req.rawData     = NULL;
    req.txData      = NULL;
    req.rxData      = g_i2s_rx;
    req.length      = I2S_RX_BUFFER_SIZE;
    MXC_I2S_Init(&req);
    MXC_NVIC_SetVector(I2S_IRQn, i2s_isr);
    NVIC_EnableIRQ(I2S_IRQn);
    MXC_I2S_RXEnable();
}

static int drain_fifo(int16_t *buf, int *idx, int max)
{
    if (!g_i2s_flag) return 0;
    g_i2s_flag = 0;
    for (int i = 0; i < I2S_RX_BUFFER_SIZE && *idx < max; i++)
        buf[(*idx)++] = (int16_t)(g_i2s_rx[i] >> 16);
    MXC_I2S_RXEnable();
    return 1;
}

static void normalize_embedding(const int32_t *packed, int8_t *dst)
{
    const int16_t *src = (const int16_t *)packed;
    float sum = 0.0f, scale;
    for (int i = 0; i < 64; i++)
        sum += (float)src[i] * (float)src[i];
    if (sum <= 0.0f) { memset(dst, 0, 64); return; }
    scale = 128.0f / sqrtf(sum);
    for (int i = 0; i < 64; i++) {
        float v = (float)src[i] * scale;
        dst[i] = v > 127.0f ? 127 : v < -128.0f ? -128 :
                 (int8_t)(v >= 0.0f ? v + 0.5f : v - 0.5f);
    }
}

static float cosine_sim(const int8_t *a, const int8_t *b)
{
    int32_t dot = 0, na = 0, nb = 0;
    for (int i = 0; i < 64; i++) {
        dot += (int32_t)a[i] * b[i];
        na  += (int32_t)a[i] * a[i];
        nb  += (int32_t)b[i] * b[i];
    }
    if (na == 0 || nb == 0) return 0.0f;
    return (float)dot / sqrtf((float)na * (float)nb);
}

static void capture_and_infer(void)
{
    int idx = 0, avg;

    printf("  [VAD] Listening... (speak!)\n");
    while (1) {
        if (!drain_fifo(g_audio, &idx, I2S_RX_BUFFER_SIZE)) continue;
        avg = 0;
        int n = idx < I2S_RX_BUFFER_SIZE ? idx : I2S_RX_BUFFER_SIZE;
        for (int i = 0; i < n; i++) {
            int16_t s = g_audio[i] < 0 ? -g_audio[i] : g_audio[i];
            if (s > avg) avg = s;
        }
        if (avg >= VAD_THRESHOLD_HIGH) break;
        idx = 0;
    }

    printf("  [REC] Capturing...\n");
    while (idx < MEL_AUDIO_SAMPLES)
        drain_fifo(g_audio, &idx, MEL_AUDIO_SAMPLES);

    printf("  [MEL] Computing log-mel...\n");
    mel_compute(g_audio, (int8_t *)g_features_u32);

    printf("  [CNN] Running inference...\n");
    cnn_enable(MXC_S_GCR_PCLKDIV_CNNCLKSEL_PCLK,
               MXC_S_GCR_PCLKDIV_CNNCLKDIV_DIV1);
    cnn_5_init();
    cnn_5_load_weights();
    cnn_5_load_bias();
    cnn_5_configure();
    memcpy32((uint32_t *)0x50400000, g_features_u32,
             MEL_N_MELS * MEL_N_FRAMES / 4);
    cnn_5_start();
    while (cnn_time == 0) {}
    cnn_5_unload((uint32_t *)ml_data);
    cnn_disable();
    normalize_embedding(ml_data, spk_embedding);
}

void speaker_auth_init(void)
{
    mel_init();
    i2s_setup();
    printf("[Speaker] Init done. DB: %d entries\n", DB_NUM_ENTRIES);
}

int speaker_auth(void)
{
    capture_and_infer();

#if DB_NUM_ENTRIES > 0
    const char   *names[] = DB_SPEAKER_NAMES;
    const int8_t  db[][DB_EMBEDDING_DIM] = DB_EMBEDDINGS;
    float best_sim = -1.0f;
    int   best_idx = -1;

    for (int i = 0; i < DB_NUM_ENTRIES; i++) {
        float sim = cosine_sim(spk_embedding, db[i]);
        if (sim > best_sim) { best_sim = sim; best_idx = i; }
    }

    printf("  Sim: %.4f | Speaker: %s\n",
           best_sim, best_idx >= 0 ? names[best_idx] : "?");

    if (best_sim >= 0.75f) {
        printf("  [Speaker] ACCEPTED\n");
        return 1;
    }
    printf("  [Speaker] REJECTED\n");
    return 0;
#else
    printf("  [Speaker] No DB\n");
    return 1;
#endif
}
