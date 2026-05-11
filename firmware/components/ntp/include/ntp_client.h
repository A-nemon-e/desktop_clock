#pragma once
#include "esp_err.h"

#define NTP_SERVER_PRIMARY  "ntp.aliyun.com"
#define NTP_SERVER_BACKUP   "pool.ntp.org"

esp_err_t ntp_sync(void);
