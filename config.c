#include "config.h"
#include "toml.h"
#include "must.h"
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <wordexp.h>

// Global error context for config loading
static struct {
  char message[256];
  int has_error;
} config_error = {0};

// Clear error state
static void clear_config_error(void) {
  config_error.has_error = 0;
  config_error.message[0] = '\0';
}

// Set error with printf-style formatting
static void set_config_error(const char *format, ...) {
  va_list args;
  va_start(args, format);
  vsnprintf(config_error.message, sizeof(config_error.message), format, args);
  va_end(args);
  config_error.has_error = 1;
}

// Public function to get last error
const char *load_config_error(void) {
  return config_error.has_error ? config_error.message : NULL;
}

typedef struct {
  const char *name;
  int value;
} code_map_t;

// Axis codes from linux/input.h
static const code_map_t axis_codes[] = {
    {"ABS_X", ABS_X},
    {"ABS_Y", ABS_Y},
    {"ABS_Z", ABS_Z},
    {"ABS_RX", ABS_RX},
    {"ABS_RY", ABS_RY},
    {"ABS_RZ", ABS_RZ},
    {"ABS_THROTTLE", ABS_THROTTLE},
    {"ABS_RUDDER", ABS_RUDDER},
    {"ABS_WHEEL", ABS_WHEEL},
    {"ABS_GAS", ABS_GAS},
    {"ABS_BRAKE", ABS_BRAKE},
    {"ABS_HAT0X", ABS_HAT0X},
    {"ABS_HAT0Y", ABS_HAT0Y},
    {"ABS_HAT1X", ABS_HAT1X},
    {"ABS_HAT1Y", ABS_HAT1Y},
    {"ABS_HAT2X", ABS_HAT2X},
    {"ABS_HAT2Y", ABS_HAT2Y},
    {"ABS_HAT3X", ABS_HAT3X},
    {"ABS_HAT3Y", ABS_HAT3Y},
    {NULL, 0},
};

// Button codes from linux/input.h
static const code_map_t button_codes[] = {
    {"BTN_0", BTN_0},
    {"BTN_1", BTN_1},
    {"BTN_2", BTN_2},
    {"BTN_3", BTN_3},
    {"BTN_4", BTN_4},
    {"BTN_5", BTN_5},
    {"BTN_6", BTN_6},
    {"BTN_7", BTN_7},
    {"BTN_8", BTN_8},
    {"BTN_9", BTN_9},
    {"BTN_TRIGGER", BTN_TRIGGER},
    {"BTN_THUMB", BTN_THUMB},
    {"BTN_THUMB2", BTN_THUMB2},
    {"BTN_TOP", BTN_TOP},
    {"BTN_TOP2", BTN_TOP2},
    {"BTN_PINKIE", BTN_PINKIE},
    {"BTN_BASE", BTN_BASE},
    {"BTN_BASE2", BTN_BASE2},
    {"BTN_BASE3", BTN_BASE3},
    {"BTN_BASE4", BTN_BASE4},
    {"BTN_BASE5", BTN_BASE5},
    {"BTN_BASE6", BTN_BASE6},
    {"BTN_DEAD", BTN_DEAD},
    {"BTN_A", BTN_A},
    {"BTN_B", BTN_B},
    {"BTN_C", BTN_C},
    {"BTN_X", BTN_X},
    {"BTN_Y", BTN_Y},
    {"BTN_Z", BTN_Z},
    {"BTN_TL", BTN_TL},
    {"BTN_TR", BTN_TR},
    {"BTN_TL2", BTN_TL2},
    {"BTN_TR2", BTN_TR2},
    {"BTN_SELECT", BTN_SELECT},
    {"BTN_START", BTN_START},
    {"BTN_MODE", BTN_MODE},
    {"BTN_THUMBL", BTN_THUMBL},
    {"BTN_THUMBR", BTN_THUMBR},
    {NULL, 0},
};

// Lookup a code by name in a code map
// Returns -1 on failure, code value on success
static int lookup_code(const char *name, const code_map_t *map) {
  if (!name)
    return -1;

  for (int i = 0; map[i].name != NULL; i++) {
    if (strcmp(name, map[i].name) == 0) {
      return map[i].value;
    }
  }
  return -1;
}

const char *code2str(const int code, const code_map_t *map) {
  for (int i = 0; map[i].name != NULL; i++) {
    if (map[i].value == code) {
      return map[i].name;
    }
  }

  return NULL;
}

const char *axis2str(const int code) { return code2str(code, axis_codes); }
const char *button2str(const int code) { return code2str(code, button_codes); }

// Expand path with tilde and environment variables
// Returns malloc'd string, caller must free
static char *expand_path(const char *path) {
  wordexp_t exp_result;
  char *expanded = NULL;

  if (wordexp(path, &exp_result, 0) == 0) {
    if (exp_result.we_wordc > 0) {
      expanded = strdup(exp_result.we_wordv[0]);
    }
    wordfree(&exp_result);
  }

  if (!expanded) {
    // Fallback: just duplicate the path
    expanded = strdup(path);
  }

  return expanded;
}

// Parse an axis channel from TOML
// Returns 0 on success, -1 on error
static int parse_axis_channel(toml_table_t *tbl, channel *out) {
  if (!out)
    return -1;

  memset(out, 0, sizeof(*out));
  out->type = CTL_AXIS;

  toml_datum_t code_datum = toml_string_in(tbl, "code");
  if (!code_datum.ok) {
    set_config_error("axis channel missing 'code' field");
    return -1;
  }

  int code = lookup_code(code_datum.u.s, axis_codes);
  if (code == -1) {
    set_config_error("unknown axis code '%.100s'", code_datum.u.s);
    free(code_datum.u.s);
    return -1;
  }

  free(code_datum.u.s);
  out->code = code;
  return 0;
}

// Parse a button channel from TOML
// Returns 0 on success, -1 on error
static int parse_button_channel(toml_table_t *tbl, channel *out) {
  if (!out)
    return -1;

  memset(out, 0, sizeof(*out));
  out->type = CTL_BUTTON;

  toml_datum_t code_datum = toml_string_in(tbl, "code");
  if (!code_datum.ok) {
    set_config_error("button channel missing 'code' field");
    return -1;
  }

  int code = lookup_code(code_datum.u.s, button_codes);
  if (code == -1) {
    set_config_error("unknown button code '%.100s'", code_datum.u.s);
    free(code_datum.u.s);
    return -1;
  }

  free(code_datum.u.s);
  out->code = code;

  toml_datum_t threshold_datum = toml_int_in(tbl, "threshold");
  if (!threshold_datum.ok) {
    set_config_error("button channel missing 'threshold' field");
    return -1;
  }

  out->threshold = (int)threshold_datum.u.i;
  return 0;
}

// Parse a multi-position channel from TOML
// Returns 0 on success, -1 on error
static int parse_multi_channel(toml_table_t *tbl, channel *out) {
  if (!out)
    return -1;

  memset(out, 0, sizeof(*out));
  out->type = CTL_MULTI;

  // Parse num_positions
  toml_datum_t num_pos_datum = toml_int_in(tbl, "num_positions");
  if (!num_pos_datum.ok) {
    set_config_error("multi channel missing 'num_positions' field");
    return -1;
  }
  out->num_positions = (int)num_pos_datum.u.i;

  if (out->num_positions < 2 || out->num_positions > 4) {
    set_config_error("num_positions must be 2-4, got %d", out->num_positions);
    return -1;
  }

  // Parse thresholds array
  toml_array_t *thresholds_arr = toml_array_in(tbl, "thresholds");
  if (!thresholds_arr) {
    set_config_error("multi channel missing 'thresholds' array");
    return -1;
  }

  int threshold_count = toml_array_nelem(thresholds_arr);
  if (threshold_count != out->num_positions - 1) {
    set_config_error("expected %d thresholds for %d positions, got %d",
                     out->num_positions - 1, out->num_positions,
                     threshold_count);
    return -1;
  }

  for (int i = 0; i < threshold_count; i++) {
    toml_datum_t item = toml_int_at(thresholds_arr, i);
    if (!item.ok) {
      set_config_error("threshold[%d] is not a number", i);
      return -1;
    }
    out->thresholds[i] = (int)item.u.i;
  }

  // Parse codes array
  toml_array_t *codes_arr = toml_array_in(tbl, "codes");
  if (!codes_arr) {
    set_config_error("multi channel missing 'codes' array");
    return -1;
  }

  int codes_count = toml_array_nelem(codes_arr);
  if (codes_count != out->num_positions) {
    set_config_error("expected %d codes for %d positions, got %d",
                     out->num_positions, out->num_positions, codes_count);
    return -1;
  }

  for (int i = 0; i < codes_count; i++) {
    toml_datum_t item = toml_string_at(codes_arr, i);
    if (!item.ok) {
      set_config_error("codes[%d] is not a string", i);
      return -1;
    }

    int code = lookup_code(item.u.s, button_codes);
    if (code == -1) {
      set_config_error("unknown button code '%.100s' in codes[%d]",
                       item.u.s, i);
      free(item.u.s);
      return -1;
    }
    out->codes[i] = code;
    free(item.u.s);
  }

  // Parse optional hysteresis (default to 0)
  toml_datum_t hysteresis_datum = toml_int_in(tbl, "hysteresis");
  if (hysteresis_datum.ok) {
    out->hysteresis = (int)hysteresis_datum.u.i;
  } else {
    out->hysteresis = 0;
  }

  return 0;
}

// Main configuration loading function
channel *load_config(const char *config_path, int *num_channels) {
  clear_config_error();

  if (!config_path || !num_channels) {
    return NULL;
  }

  // Expand path (handles ~ and environment variables)
  char *expanded_path = expand_path(config_path);
  if (!expanded_path) {
    return NULL;
  }

  // Open file
  FILE *fp = fopen(expanded_path, "r");
  free(expanded_path);

  if (!fp) {
    // File doesn't exist - silent fallback to defaults
    return NULL;
  }

  // Parse TOML
  char errbuf[200];
  toml_table_t *root = toml_parse_file(fp, errbuf, sizeof(errbuf));
  fclose(fp);

  if (!root) {
    set_config_error("TOML parse error: %.100s", errbuf);
    return NULL;
  }

  // Extract channels array
  toml_array_t *channels_arr = toml_array_in(root, "channels");
  if (!channels_arr) {
    set_config_error("'channels' field must be an array");
    toml_free(root);
    return NULL;
  }

  int count = toml_array_nelem(channels_arr);
  if (count == 0) {
    set_config_error("'channels' array is empty");
    toml_free(root);
    return NULL;
  }

  // Allocate channels array
  channel *channels = calloc(count, sizeof(channel));
  if (!channels) {
    set_config_error("failed to allocate memory for channels");
    toml_free(root);
    return NULL;
  }

  // Parse each channel
  for (int i = 0; i < count; i++) {
    toml_table_t *ch_tbl = toml_table_at(channels_arr, i);
    if (!ch_tbl) {
      set_config_error("channel[%d] is not an object", i);
      free(channels);
      toml_free(root);
      return NULL;
    }

    toml_datum_t type_datum = toml_string_in(ch_tbl, "type");
    if (!type_datum.ok) {
      set_config_error("channel[%d] missing 'type' field", i);
      free(channels);
      toml_free(root);
      return NULL;
    }

    const char *type = type_datum.u.s;

    if (strcmp(type, "axis") == 0) {
      free(type_datum.u.s);
      if (parse_axis_channel(ch_tbl, &channels[i]) < 0) {
        // Error already set by helper
        free(channels);
        toml_free(root);
        return NULL;
      }
    } else if (strcmp(type, "button") == 0) {
      free(type_datum.u.s);
      if (parse_button_channel(ch_tbl, &channels[i]) < 0) {
        // Error already set by helper
        free(channels);
        toml_free(root);
        return NULL;
      }
    } else if (strcmp(type, "multi") == 0) {
      free(type_datum.u.s);
      if (parse_multi_channel(ch_tbl, &channels[i]) < 0) {
        // Error already set by helper
        free(channels);
        toml_free(root);
        return NULL;
      }
    } else {
      set_config_error("unknown channel type '%.100s' at channel[%d]", type, i);
      free(type_datum.u.s);
      free(channels);
      toml_free(root);
      return NULL;
    }
  }

  *num_channels = count;
  toml_free(root);
  return channels;
}

// Free configuration memory
void free_config(channel *channels) {
  if (channels) {
    free(channels);
  }
}
