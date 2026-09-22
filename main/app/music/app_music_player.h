#pragma once

#include <stdbool.h>

#include "esp_err.h"

esp_err_t app_music_player_init(void);

esp_err_t app_music_player_play(const char *url);

void app_music_player_stop(void);

bool app_music_player_is_playing(void);

void app_music_player_deinit(void);
