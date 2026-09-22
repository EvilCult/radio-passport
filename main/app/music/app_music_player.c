#include "app_music_player.h"

#include "bsp_audio.h"

#include "esp_audio_dec_default.h"
#include "esp_audio_simple_dec.h"
#include "esp_audio_simple_dec_default.h"
#include "esp_http_client.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define APP_MUSIC_PLAYER_INPUT_SIZE 512
#define APP_MUSIC_PLAYER_OUTPUT_SIZE 4096
#define APP_MUSIC_PLAYER_TASK_STACK_SIZE (20 * 1024)
#define APP_MUSIC_PLAYER_PCM_WRITE_SIZE 1024

typedef struct {
    char *url;
} app_music_player_task_arg_t;

static const char *TAG = "music_player";

static TaskHandle_t player_task;

static esp_http_client_handle_t http_client;
static esp_audio_simple_dec_handle_t decoder;

static uint8_t *input_buffer;
static uint8_t *output_buffer;

static int16_t *mono_buffer;
static size_t mono_buffer_size;

static bool decoder_registered;
static bool simple_decoder_registered;

static esp_err_t player_http_init(const char *url)
{
    esp_http_client_config_t config = {
        .url = url,
        .timeout_ms = 10000,
        .buffer_size = APP_MUSIC_PLAYER_INPUT_SIZE,
        .buffer_size_tx = 1024,
    };

    http_client = esp_http_client_init(&config);

    if (http_client == NULL) {
        ESP_LOGE(TAG, "HTTP client init failed");
        return ESP_FAIL;
    }

    esp_err_t ret = esp_http_client_open(http_client, 0);

    if (ret != ESP_OK) {
        ESP_LOGE(
            TAG,
            "HTTP open failed: %s",
            esp_err_to_name(ret)
        );

        esp_http_client_cleanup(http_client);
        http_client = NULL;

        return ret;
    }

    int content_length =
        esp_http_client_fetch_headers(http_client);

    if (content_length < 0) {
        ESP_LOGE(TAG, "HTTP fetch headers failed");

        esp_http_client_close(http_client);
        esp_http_client_cleanup(http_client);
        http_client = NULL;

        return ESP_FAIL;
    }

    ESP_LOGI(
        TAG,
        "HTTP connected, content_length=%d",
        content_length
    );

    return ESP_OK;
}

static int player_http_read(uint8_t *buffer, int size)
{
    if (http_client == NULL) {
        return -1;
    }

    return esp_http_client_read(
        http_client,
        (char *)buffer,
        size
    );
}

static void player_http_deinit(void)
{
    if (http_client == NULL) {
        return;
    }

    esp_http_client_close(http_client);
    esp_http_client_cleanup(http_client);
    http_client = NULL;
}

static esp_err_t player_decoder_init(void)
{
    esp_audio_err_t ret;

    ret = esp_audio_dec_register_default();

    if (ret != ESP_AUDIO_ERR_OK) {
        ESP_LOGE(
            TAG,
            "esp_audio_dec_register_default failed: %d",
            ret
        );

        return ESP_FAIL;
    }

    decoder_registered = true;

    ret = esp_audio_simple_dec_register_default();

    if (ret != ESP_AUDIO_ERR_OK) {
        ESP_LOGE(
            TAG,
            "esp_audio_simple_dec_register_default failed: %d",
            ret
        );

        esp_audio_dec_unregister_default();
        decoder_registered = false;

        return ESP_FAIL;
    }

    simple_decoder_registered = true;

    esp_audio_simple_dec_cfg_t config = {
        .dec_type = ESP_AUDIO_SIMPLE_DEC_TYPE_MP3,
        .dec_cfg = NULL,
        .cfg_size = 0,
        .use_frame_dec = false,
    };

    ret = esp_audio_simple_dec_open(
        &config,
        &decoder
    );

    if (ret != ESP_AUDIO_ERR_OK) {
        ESP_LOGE(
            TAG,
            "decoder open failed: %d",
            ret
        );

        decoder = NULL;

        esp_audio_simple_dec_unregister_default();
        simple_decoder_registered = false;

        esp_audio_dec_unregister_default();
        decoder_registered = false;

        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "MP3 decoder ready");

    return ESP_OK;
}

static void player_decoder_deinit(void)
{
    if (decoder != NULL) {
        esp_audio_simple_dec_close(decoder);
        decoder = NULL;
    }

    if (simple_decoder_registered) {
        esp_audio_simple_dec_unregister_default();
        simple_decoder_registered = false;
    }

    if (decoder_registered) {
        esp_audio_dec_unregister_default();
        decoder_registered = false;
    }
}

static esp_err_t player_write_pcm(
    const uint8_t *pcm,
    size_t bytes
)
{
    while (bytes > 0) {
        if (ulTaskNotifyTake(pdTRUE, 0) > 0) {
            ESP_LOGI(
                TAG,
                "playback stopped during audio write"
            );

            return ESP_OK;
        }

        size_t write_size = bytes;

        if (write_size > APP_MUSIC_PLAYER_PCM_WRITE_SIZE) {
            write_size = APP_MUSIC_PLAYER_PCM_WRITE_SIZE;
        }

        esp_err_t ret = bsp_audio_write(
            pcm,
            write_size
        );

        if (ret != ESP_OK) {
            ESP_LOGE(
                TAG,
                "audio write failed: %s",
                esp_err_to_name(ret)
            );

            return ret;
        }

        pcm += write_size;
        bytes -= write_size;
    }

    return ESP_OK;
}

static esp_err_t player_write_stereo_as_mono(
    const uint8_t *pcm,
    size_t bytes
)
{
    size_t sample_count =
        bytes / (sizeof(int16_t) * 2);

    if (sample_count == 0) {
        return ESP_OK;
    }

    size_t required_bytes =
        sample_count * sizeof(int16_t);

    if (required_bytes > mono_buffer_size) {
        int16_t *new_buffer = realloc(
            mono_buffer,
            required_bytes
        );

        if (new_buffer == NULL) {
            ESP_LOGE(
                TAG,
                "mono buffer allocation failed"
            );

            return ESP_ERR_NO_MEM;
        }

        mono_buffer = new_buffer;
        mono_buffer_size = required_bytes;
    }

    const int16_t *stereo =
        (const int16_t *)pcm;

    for (size_t i = 0; i < sample_count; i++) {
        int32_t left = stereo[i * 2];
        int32_t right = stereo[i * 2 + 1];

        mono_buffer[i] =
            (int16_t)((left + right) / 2);
    }

    return player_write_pcm(
        (const uint8_t *)mono_buffer,
        required_bytes
    );
}

static esp_err_t player_play(const char *url)
{
    esp_err_t ret;

    input_buffer =
        malloc(APP_MUSIC_PLAYER_INPUT_SIZE);

    output_buffer =
        malloc(APP_MUSIC_PLAYER_OUTPUT_SIZE);

    if (input_buffer == NULL ||
        output_buffer == NULL) {
        ESP_LOGE(
            TAG,
            "decoder buffer allocation failed"
        );

        ret = ESP_ERR_NO_MEM;
        goto cleanup;
    }

    size_t output_buffer_size =
        APP_MUSIC_PLAYER_OUTPUT_SIZE;

    ret = player_http_init(url);

    if (ret != ESP_OK) {
        goto cleanup;
    }

    ret = player_decoder_init();

    if (ret != ESP_OK) {
        goto cleanup;
    }

    bool format_set = false;

    while (true) {
        if (ulTaskNotifyTake(pdTRUE, 0) > 0) {
            ESP_LOGI(TAG, "playback stopped");
            break;
        }

        int read_size = player_http_read(
            input_buffer,
            APP_MUSIC_PLAYER_INPUT_SIZE
        );

        if (read_size <= 0) {
            ESP_LOGI(TAG, "HTTP stream ended");
            break;
        }

        esp_audio_simple_dec_raw_t raw = {
            .buffer = input_buffer,
            .len = read_size,
            .eos = false,
            .consumed = 0,
        };

        while (raw.len > 0) {
            if (ulTaskNotifyTake(pdTRUE, 0) > 0) {
                ESP_LOGI(TAG, "playback stopped");
                goto cleanup;
            }

            esp_audio_simple_dec_out_t output = {
                .buffer = output_buffer,
                .len = output_buffer_size,
                .needed_size = 0,
                .decoded_size = 0,
            };

            esp_audio_err_t decode_ret =
                esp_audio_simple_dec_process(
                    decoder,
                    &raw,
                    &output
                );

            if (decode_ret == ESP_AUDIO_ERR_BUFF_NOT_ENOUGH) {
                if (output.needed_size <= output_buffer_size) {
                    ESP_LOGE(
                        TAG,
                        "invalid decoder buffer size: "
                        "needed=%lu current=%lu",
                        (unsigned long)output.needed_size,
                        (unsigned long)output_buffer_size
                    );

                    ret = ESP_ERR_INVALID_SIZE;
                    goto cleanup;
                }

                uint8_t *new_buffer = realloc(
                    output_buffer,
                    output.needed_size
                );

                if (new_buffer == NULL) {
                    ESP_LOGE(
                        TAG,
                        "output buffer realloc failed"
                    );

                    ret = ESP_ERR_NO_MEM;
                    goto cleanup;
                }

                output_buffer = new_buffer;
                output_buffer_size =
                    output.needed_size;

                continue;
            }

            if (decode_ret != ESP_AUDIO_ERR_OK) {
                ESP_LOGE(
                    TAG,
                    "decode failed: %d",
                    decode_ret
                );

                ret = ESP_FAIL;
                goto cleanup;
            }

            if (output.decoded_size > 0) {
                if (!format_set) {
                    esp_audio_simple_dec_info_t info = {};

                    ret = esp_audio_simple_dec_get_info(
                        decoder,
                        &info
                    );

                    if (ret != ESP_AUDIO_ERR_OK) {
                        ESP_LOGE(
                            TAG,
                            "get decoder info failed: %d",
                            ret
                        );

                        goto cleanup;
                    }

                    ESP_LOGI(
                        TAG,
                        "MP3: %luHz %ubit %uch bitrate=%lu",
                        (unsigned long)info.sample_rate,
                        info.bits_per_sample,
                        info.channel,
                        (unsigned long)info.bitrate
                    );

                    ret = bsp_audio_set_format(
                        info.sample_rate,
                        info.bits_per_sample,
                        1
                    );

                    if (ret != ESP_OK) {
                        ESP_LOGE(
                            TAG,
                            "audio format failed: %s",
                            esp_err_to_name(ret)
                        );

                        goto cleanup;
                    }

                    bsp_audio_set_volume(60);

                    format_set = true;

                    ESP_LOGI(
                        TAG,
                        "audio format configured: mono"
                    );
                }

                ret = player_write_stereo_as_mono(
                    output.buffer,
                    output.decoded_size
                );

                if (ret != ESP_OK) {
                    goto cleanup;
                }
            }

            if (raw.consumed == 0) {
                ESP_LOGE(
                    TAG,
                    "decoder consumed 0 bytes"
                );

                ret = ESP_FAIL;
                goto cleanup;
            }

            raw.buffer += raw.consumed;
            raw.len -= raw.consumed;
        }
    }

    ret = ESP_OK;

cleanup:

    player_decoder_deinit();
    player_http_deinit();

    free(input_buffer);
    free(output_buffer);
    free(mono_buffer);

    input_buffer = NULL;
    output_buffer = NULL;
    mono_buffer = NULL;
    mono_buffer_size = 0;

    return ret;
}

static void player_task_func(void *arg)
{
    app_music_player_task_arg_t *task_arg = arg;

    esp_err_t ret = player_play(task_arg->url);

    if (ret != ESP_OK) {
        ESP_LOGE(
            TAG,
            "playback failed: %s",
            esp_err_to_name(ret)
        );
    }

    free(task_arg->url);
    free(task_arg);

    player_task = NULL;

    vTaskDelete(NULL);
}

esp_err_t app_music_player_init(void)
{
    return bsp_audio_init();
}

esp_err_t app_music_player_play(const char *url)
{
    if (url == NULL || url[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    if (player_task != NULL) {
        app_music_player_stop();

        while (player_task != NULL) {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    app_music_player_task_arg_t *arg =
        malloc(sizeof(*arg));

    if (arg == NULL) {
        return ESP_ERR_NO_MEM;
    }

    arg->url = strdup(url);

    if (arg->url == NULL) {
        free(arg);
        return ESP_ERR_NO_MEM;
    }

    BaseType_t ret = xTaskCreate(
        player_task_func,
        "music_player",
        APP_MUSIC_PLAYER_TASK_STACK_SIZE,
        arg,
        5,
        &player_task
    );

    if (ret != pdPASS) {
        free(arg->url);
        free(arg);

        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

void app_music_player_stop(void)
{
    if (player_task != NULL) {
        xTaskNotify(
            player_task,
            1,
            eSetValueWithOverwrite
        );
    }
}

bool app_music_player_is_playing(void)
{
    return player_task != NULL;
}

void app_music_player_deinit(void)
{
    app_music_player_stop();
}
