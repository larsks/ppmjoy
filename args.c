#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "args.h"

// Options table (moved from main.c)
static struct option options[] = {
    // clang-format off
  {"help", 0, NULL, 'h'},
  {"device", 1, NULL, 'd'},
  {"config", 1, NULL, 'f'},
  {"verbose", 0, NULL, 'v'},
  {"monitor", 0, NULL, 'm'},
  {NULL, 0, NULL, 0},
    // clang-format on
};

void show_usage(FILE *out) {
  fprintf(out, "ppmjoy: usage: ppmjoy [--device|-d alsa_device]"
               " [--config|-f ppmjoy_config]"
               " [--monitor|-m]"
               " [--verbose|-v]"
               "\n");
}

args_result_t parse_arguments(int argc, char *argv[], app_config_t *config,
                              void (*show_usage_fn)(FILE *out)) {
  int c;

  // Initialize with defaults from environment or hardcoded
  config->alsa_device = getenv("PPMJOY_ALSA_DEVICE");
  if (!config->alsa_device || strlen(config->alsa_device) == 0)
    config->alsa_device = DEFAULT_ALSA_DEVICE;

  config->config_path = getenv("PPMJOY_CONFIG");
  if (!config->config_path || strlen(config->config_path) == 0)
    config->config_path = DEFAULT_CONFIG_PATH;

  config->verbose = 0;
  config->monitor = 0;

  while (-1 != (c = getopt_long(argc, argv, "d:f:hvm", options, NULL))) {
    switch (c) {
    case 'h':
      if (show_usage_fn)
        show_usage_fn(stdout);
      return ARGS_HELP_REQUESTED;
    case 'd':
      config->alsa_device = strdup(optarg);
      break;
    case 'f':
      config->config_path = strdup(optarg);
      break;
    case 'v':
      config->verbose++;
      break;
    case 'm':
      config->monitor = 1;
      break;
    case '?':
      if (show_usage_fn)
        show_usage_fn(stderr);
      return ARGS_ERROR;
    }
  }

  return ARGS_OK;
}
