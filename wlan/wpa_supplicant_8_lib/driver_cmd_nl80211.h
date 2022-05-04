#ifndef __LIBCMD_WPA_SUPPLICANT_8_H__
#define __LIBCMD_WPA_SUPPLICANT_8_H__

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "wpa_supplicant_i.h"

#define LOG_TAG "LibCmdWpaSupplicant8Driver"

#include "log/log.h"

#define LIBCMD_WPA_SUPPLICANT_DEBUG 0

#if GCE_WPA_SUPPLICANT_DEBUG
#  define D(...) ALOGW(__VA_ARGS__)
#else
#  define D(...) ((void)0)
#endif

typedef struct android_wifi_priv_cmd {
  char* buf;
  int used_len;
  int total_len;
} android_wifi_priv_cmd;

#endif // __LIBCMD_WPA_SUPPLICANT_8_H__
