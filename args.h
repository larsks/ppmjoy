#ifndef _ARGS_H
#define _ARGS_H

#include <stdio.h>

#define DEFAULT_ALSA_DEVICE "default"
#define DEFAULT_CONFIG_PATH "~/.config/ppmjoy.toml"

// Configuration structure returned by argument parsing
typedef struct {
  char *alsa_device;
  char *config_path;
  int verbose;
  int monitor;
} app_config_t;

// Return codes for parse_arguments
typedef enum {
  ARGS_OK = 0,         // Successfully parsed
  ARGS_HELP_REQUESTED, // User requested help
  ARGS_ERROR           // Parse error occurred
} args_result_t;

// Parse command line arguments
// Parameters:
//   argc, argv: command line arguments
//   config: pointer to config structure to populate
//   show_usage_fn: function pointer to display usage (allows testing)
// Returns: args_result_t indicating success/failure/help
args_result_t parse_arguments(int argc, char *argv[], app_config_t *config,
                              void (*show_usage_fn)(FILE *out));

// Display usage information
void show_usage(FILE *out);

#endif // _ARGS_H
