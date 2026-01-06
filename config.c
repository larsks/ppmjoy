#include "config.h"
#include "cJSON.h"
#include "must.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <wordexp.h>

typedef struct {
  const char *name;
  int value;
} code_map_t;

// Axis codes from linux/input.h
const code_map_t axis_codes[] = {
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
const code_map_t button_codes[] = {
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

// Read entire file into malloc'd string
// Returns NULL if file doesn't exist or can't be read
static char *read_file(const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f) {
    return NULL; // Silent failure for missing config file
  }

  // Get file size
  fseek(f, 0, SEEK_END);
  long fsize = ftell(f);
  fseek(f, 0, SEEK_SET);

  // Allocate buffer
  char *content = malloc(fsize + 1);
  if (!content) {
    fclose(f);
    return NULL;
  }

  // Read file
  size_t read_size = fread(content, 1, fsize, f);
  fclose(f);

  if (read_size != fsize) {
    free(content);
    return NULL;
  }

  content[fsize] = '\0';
  return content;
}

// Parse an axis channel from JSON
static channel parse_axis_channel(cJSON *json) {
  channel ch;
  memset(&ch, 0, sizeof(ch));
  ch.type = CTL_AXIS;

  cJSON *code_json = cJSON_GetObjectItem(json, "code");
  if (!code_json || !cJSON_IsString(code_json)) {
    fprintf(stderr, "error: axis channel missing 'code' field\n");
    exit(1);
  }

  int code = lookup_code(code_json->valuestring, axis_codes);
  if (code == -1) {
    fprintf(stderr, "error: unknown axis code '%s'\n", code_json->valuestring);
    exit(1);
  }

  ch.code = code;
  return ch;
}

// Parse a button channel from JSON
static channel parse_button_channel(cJSON *json) {
  channel ch;
  memset(&ch, 0, sizeof(ch));
  ch.type = CTL_BUTTON;

  cJSON *code_json = cJSON_GetObjectItem(json, "code");
  if (!code_json || !cJSON_IsString(code_json)) {
    fprintf(stderr, "error: button channel missing 'code' field\n");
    exit(1);
  }

  int code = lookup_code(code_json->valuestring, button_codes);
  if (code == -1) {
    fprintf(stderr, "error: unknown button code '%s'\n",
            code_json->valuestring);
    exit(1);
  }

  ch.code = code;

  cJSON *threshold_json = cJSON_GetObjectItem(json, "threshold");
  if (!threshold_json || !cJSON_IsNumber(threshold_json)) {
    fprintf(stderr, "error: button channel missing 'threshold' field\n");
    exit(1);
  }

  ch.threshold = threshold_json->valueint;
  return ch;
}

// Parse a multi-position channel from JSON
static channel parse_multi_channel(cJSON *json) {
  channel ch;
  memset(&ch, 0, sizeof(ch));
  ch.type = CTL_MULTI;

  // Parse num_positions
  cJSON *num_pos_json = cJSON_GetObjectItem(json, "num_positions");
  if (!num_pos_json || !cJSON_IsNumber(num_pos_json)) {
    fprintf(stderr, "error: multi channel missing 'num_positions' field\n");
    exit(1);
  }
  ch.num_positions = num_pos_json->valueint;

  if (ch.num_positions < 2 || ch.num_positions > 4) {
    fprintf(stderr, "error: num_positions must be 2-4, got %d\n",
            ch.num_positions);
    exit(1);
  }

  // Parse thresholds array
  cJSON *thresholds_json = cJSON_GetObjectItem(json, "thresholds");
  if (!thresholds_json || !cJSON_IsArray(thresholds_json)) {
    fprintf(stderr, "error: multi channel missing 'thresholds' array\n");
    exit(1);
  }

  int threshold_count = cJSON_GetArraySize(thresholds_json);
  if (threshold_count != ch.num_positions - 1) {
    fprintf(stderr, "error: expected %d thresholds for %d positions, got %d\n",
            ch.num_positions - 1, ch.num_positions, threshold_count);
    exit(1);
  }

  for (int i = 0; i < threshold_count; i++) {
    cJSON *item = cJSON_GetArrayItem(thresholds_json, i);
    if (!cJSON_IsNumber(item)) {
      fprintf(stderr, "error: threshold[%d] is not a number\n", i);
      exit(1);
    }
    ch.thresholds[i] = item->valueint;
  }

  // Parse codes array
  cJSON *codes_json = cJSON_GetObjectItem(json, "codes");
  if (!codes_json || !cJSON_IsArray(codes_json)) {
    fprintf(stderr, "error: multi channel missing 'codes' array\n");
    exit(1);
  }

  int codes_count = cJSON_GetArraySize(codes_json);
  if (codes_count != ch.num_positions) {
    fprintf(stderr, "error: expected %d codes for %d positions, got %d\n",
            ch.num_positions, ch.num_positions, codes_count);
    exit(1);
  }

  for (int i = 0; i < codes_count; i++) {
    cJSON *item = cJSON_GetArrayItem(codes_json, i);
    if (!cJSON_IsString(item)) {
      fprintf(stderr, "error: codes[%d] is not a string\n", i);
      exit(1);
    }

    int code = lookup_code(item->valuestring, button_codes);
    if (code == -1) {
      fprintf(stderr, "error: unknown button code '%s' in codes[%d]\n",
              item->valuestring, i);
      exit(1);
    }
    ch.codes[i] = code;
  }

  // Parse optional hysteresis (default to 0)
  cJSON *hysteresis_json = cJSON_GetObjectItem(json, "hysteresis");
  if (hysteresis_json && cJSON_IsNumber(hysteresis_json)) {
    ch.hysteresis = hysteresis_json->valueint;
  } else {
    ch.hysteresis = 0;
  }

  return ch;
}

// Main configuration loading function
channel *load_config(const char *config_path, int *num_channels) {
  if (!config_path || !num_channels) {
    return NULL;
  }

  // Expand path (handles ~ and environment variables)
  char *expanded_path = expand_path(config_path);
  if (!expanded_path) {
    return NULL;
  }

  // Read file
  char *json_str = read_file(expanded_path);
  free(expanded_path);

  if (!json_str) {
    // File doesn't exist - silent fallback to defaults
    return NULL;
  }

  // Parse JSON
  cJSON *root = cJSON_Parse(json_str);
  free(json_str);

  if (!root) {
    const char *error_ptr = cJSON_GetErrorPtr();
    if (error_ptr) {
      fprintf(stderr, "JSON parse error before: %s\n", error_ptr);
    } else {
      fprintf(stderr, "JSON parse error\n");
    }
    exit(1);
  }

  // Extract channels array
  cJSON *channels_json = cJSON_GetObjectItem(root, "channels");
  if (!channels_json || !cJSON_IsArray(channels_json)) {
    fprintf(stderr, "error: 'channels' field must be an array\n");
    cJSON_Delete(root);
    exit(1);
  }

  int count = cJSON_GetArraySize(channels_json);
  if (count == 0) {
    fprintf(stderr, "error: 'channels' array is empty\n");
    cJSON_Delete(root);
    exit(1);
  }

  // Allocate channels array
  channel *channels = calloc(count, sizeof(channel));
  if (!channels) {
    fprintf(stderr, "error: failed to allocate memory for channels\n");
    cJSON_Delete(root);
    exit(1);
  }

  // Parse each channel
  for (int i = 0; i < count; i++) {
    cJSON *ch_json = cJSON_GetArrayItem(channels_json, i);
    if (!cJSON_IsObject(ch_json)) {
      fprintf(stderr, "error: channel[%d] is not an object\n", i);
      free(channels);
      cJSON_Delete(root);
      exit(1);
    }

    cJSON *type_json = cJSON_GetObjectItem(ch_json, "type");
    if (!type_json || !cJSON_IsString(type_json)) {
      fprintf(stderr, "error: channel[%d] missing 'type' field\n", i);
      free(channels);
      cJSON_Delete(root);
      exit(1);
    }

    const char *type = type_json->valuestring;

    if (strcmp(type, "axis") == 0) {
      channels[i] = parse_axis_channel(ch_json);
    } else if (strcmp(type, "button") == 0) {
      channels[i] = parse_button_channel(ch_json);
    } else if (strcmp(type, "multi") == 0) {
      channels[i] = parse_multi_channel(ch_json);
    } else {
      fprintf(stderr, "error: unknown channel type '%s' at channel[%d]\n", type,
              i);
      free(channels);
      cJSON_Delete(root);
      exit(1);
    }
  }

  *num_channels = count;
  cJSON_Delete(root);
  return channels;
}

// Free configuration memory
void free_config(channel *channels) {
  if (channels) {
    free(channels);
  }
}
