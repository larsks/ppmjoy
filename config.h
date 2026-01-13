#ifndef _CONFIG_H
#define _CONFIG_H

#include <linux/input.h>
#include <unistd.h>

typedef enum {
  CTL_AXIS,
  CTL_BUTTON,
  CTL_MULTI,
} control_type;

typedef struct {
  control_type type;
  int code;
  int threshold;
  // CTL_MULTI specific fields:
  int num_positions; // 2, 3, or 4
  int thresholds[3]; // up to 3 thresholds for 4 positions
  int codes[4];      // button code for each position
  int hysteresis;    // hysteresis margin (e.g., 10)
} channel;

// Main configuration loading function
// Returns: pointer to dynamically allocated channel array, or NULL on error
// Sets *configured_channel_count to the number of channels loaded
// Sets *total_channel_count to the total number of channels in the PPM signal
//   (defaults to configured_channel_count if not specified in config)
// Note: Not thread-safe due to global error state
channel *load_config(const char *config_path, int *configured_channel_count,
                     int *total_channel_count);

// Get the last error message from load_config
// Returns: NULL if no error, or error message string
// Note: The returned string is valid until the next call to load_config
const char *load_config_error(void);

// convert an axis code to a symbolic name
const char *axis2str(const int code);

// convert a button code to a symbolic name
const char *button2str(const int code);

// Free configuration memory
void free_config(channel *channels);

#endif // _CONFIG_H
