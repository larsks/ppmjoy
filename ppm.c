#include <alsa/asoundlib.h>
#include <limits.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <getopt.h>
#include <unistd.h>

#include "config.h"
#include "must.h"

#define CLEAR() printf("\033[H\033[J")
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#define BUTTON_RELEASE_TIME_MS 100

typedef struct {
  unsigned int rate;         // soundcard sampling rate in Hz
  size_t sync_min, sync_max; // allowed length of sync pulse in samples
  int16_t threshhold;        // threshhold to be a high pulse
} pulse_params_t;

typedef struct {
  enum { INIT, LOW, HIGH, SYNC, INVALID } type; // type of pulse
  size_t length;                                // length of pulse in samples
} pulse_t;

typedef struct {
  unsigned int period; // length of a full cycle in samples
  size_t sync_min;     // minimum length of sync pulse in samples
  size_t sync_max;     // maximum length of sync pulse in samples
  size_t low_min;      // minimum length of low pulse in samples
  size_t low_max;      // maximum length of low pulse in samples
  size_t high_min;     // minimum length of high pulse in samples
  size_t high_max;     // maximum length of high pulse in samples
} tx_parms;

typedef struct {
  pulse_params_t params;
  size_t samples;
  pulse_t pulse;
  int16_t (*buffer)[2];
  size_t offset;
  snd_pcm_t *handle;
} state_t;

typedef struct {
  int pressed_button_code;    // Which button is currently pressed (-1 if none)
  struct timespec press_time; // When the button was pressed
} button_state_t;

channel *channels = NULL;
int num_channels = 0;

static channel default_channels[] = {
    // clang-format off
  {CTL_AXIS,  ABS_X},
  {CTL_AXIS,  ABS_Y},
  {CTL_AXIS,  ABS_RX},
  {CTL_AXIS,  ABS_RY},
    // clang-format on
};

struct option options[] = {
    // clang-format off
  {"device", 1, NULL, 'd'},
  {"config", 1, NULL, 'f'},
  {NULL, 0, NULL, 0},
    // clang-format on
};

void init_pulse(pulse_t *p) {
  if (p) {
    p->type = INIT;
    p->length = 0;
  }
}

void destroy_alsa(state_t *state) {
  if (state->buffer)
    free(state->buffer);

  if (state->handle)
    snd_pcm_close(state->handle);
}

int init_alsa(state_t *state, char *dev, unsigned int rate, unsigned int period,
              unsigned int sync_length, int16_t threshhold) {
  int ret = 0;
  int err;
  snd_pcm_hw_params_t *hw_params = 0;

  fprintf(stderr, "opening alsa device %s\n", dev);

  if ((err = snd_pcm_open(&state->handle, dev, SND_PCM_STREAM_CAPTURE, 0)) <
      0) {
    fprintf(stderr, "failed to open audio device %s (%s)\n", dev,
            snd_strerror(err));
    goto error;
  }

  if ((err = snd_pcm_hw_params_malloc(&hw_params)) < 0) {
    fprintf(stderr, "failed to allocate hardware parameter structure (%s)\n",
            snd_strerror(err));
    goto error;
  }

  if ((err = snd_pcm_hw_params_any(state->handle, hw_params)) < 0) {
    fprintf(stderr, "failed to initialize hardware parameter structure (%s)\n",
            snd_strerror(err));
    goto error;
  }

  if ((err = snd_pcm_hw_params_set_access(state->handle, hw_params,
                                          SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
    fprintf(stderr, "failed to set access type (%s)\n", snd_strerror(err));
    goto error;
  }

  if ((err = snd_pcm_hw_params_set_format(state->handle, hw_params,
                                          SND_PCM_FORMAT_S16_LE)) < 0) {
    fprintf(stderr, "failed to set sample format (%s)\n", snd_strerror(err));
    goto error;
  }

  if ((err = snd_pcm_hw_params_set_rate_near(state->handle, hw_params, &rate,
                                             0)) < 0) {
    fprintf(stderr, "failed to set sample rate (%s)\n", snd_strerror(err));
    goto error;
  }

  if ((err = snd_pcm_hw_params_set_channels(state->handle, hw_params, 2)) < 0) {
    fprintf(stderr, "failed to set channel count (%s)\n", snd_strerror(err));
    goto error;
  }

  if ((err = snd_pcm_hw_params(state->handle, hw_params)) < 0) {
    fprintf(stderr, "failed to set parameters (%s)\n", snd_strerror(err));
    goto error;
  }

  if ((err = snd_pcm_prepare(state->handle)) < 0) {
    fprintf(stderr, "failed to prepare audio interface for use (%s)\n",
            snd_strerror(err));
    goto error;
  }

  state->samples = (rate * period + 999) / 1000;
  state->params.rate = rate;
  state->params.sync_min = (sync_length * rate + 999) / 1000;
  state->params.sync_max = 2 * state->samples;
  state->params.threshhold = threshhold;
  state->buffer =
      malloc(state->samples * sizeof(int16_t[2])); // one period worth of buffer
  state->offset = state->samples; // indicate that buffer contains no data
  init_pulse(&state->pulse);

  goto cleanup;

error:
  ret = 1;
  destroy_alsa(state);
  exit(1);

cleanup:
  snd_pcm_hw_params_free(hw_params);
  return ret;
}

// return true if datum starts a new pulse, false otherwise
bool datum_to_pulse(int16_t datum, pulse_params_t *params, pulse_t *p) {
  bool complete = false;
  if (datum > params->threshhold) { // high signal
    switch (p->type) {
    case INIT: // start of high pulse
      p->type = HIGH;
      p->length = 1;
      break;
    case LOW: // end of high pulse
      complete = true;
      break;
    case HIGH: // continue high pulse
      p->length++;
      break;
    default: // error
      exit(1);
    }
  } else { // low signal
    switch (p->type) {
    case INIT: // start of low pulse
      p->type = LOW;
      p->length = 1;
      break;
    case LOW: // continue low pulse
      p->length++;
      break;
    case HIGH: // end of low pulse
      complete = true;
      break;
    default: // error
      exit(1);
    }
  }

  if (complete) {
    if (p->length >= params->sync_min) {
      if (p->type == LOW && p->length <= params->sync_max)
        p->type = SYNC; // this is really a sync pulse
      else
        p->type = INVALID; // pulse is too long
    }

    /*
    if (debug) {
      switch (p->type) {
      case LOW:
        printf("L");
        break;
      case HIGH:
        printf("H");
        break;
      case SYNC:
        printf("S");
        break;
      default:
        printf("I");
      }
      printf("%zu ", p->length);
    }
    */
  }

  return complete;
}

bool data_to_pulse(int16_t (*data)[2], size_t *offset, size_t samples,
                   pulse_params_t *params, pulse_t *p) {
  while (*offset < samples) {
    if (datum_to_pulse(data[*offset][0], params, p))
      return true; // datum starts a new pulse, so don't update offset
    else
      (*offset)++;
  }
  return false;
}

void read_pulse_alsa(state_t *state) {
  int err;
  init_pulse(&state->pulse);
  for (;;) {
    if (state->offset == state->samples) { // refill buffer
      if ((err = snd_pcm_readi(state->handle, state->buffer, state->samples)) !=
          state->samples) {
        fprintf(stderr, "read from audio interface failed (%s)\n",
                snd_strerror(err));
        exit(1);
      }
      state->offset = 0;
    }
    if (data_to_pulse(state->buffer, &state->offset, state->samples,
                      &state->params, &state->pulse))
      break; // found a complete pulse
  }
}

// Determine which position a value maps to for a multi-position switch
int determine_position(int value, channel *ch) {
  // For a channel with N positions, we have N-1 thresholds
  // Position 0: value < threshold[0]
  // Position 1: threshold[0] <= value < threshold[1]
  // Position N-1: value >= threshold[N-2]

  for (int i = 0; i < ch->num_positions - 1; i++) {
    if (value < ch->thresholds[i]) {
      return i;
    }
  }
  return ch->num_positions - 1; // highest position
}

// Determine position with hysteresis to prevent bouncing
int determine_position_with_hysteresis(int value, int last_pos, channel *ch) {
  if (last_pos < 0) {
    // First time: no hysteresis
    return determine_position(value, ch);
  }

  // Check if we should stay in current position
  // We stay if value is within threshold boundaries +/- hysteresis

  int lower_threshold = (last_pos > 0) ? ch->thresholds[last_pos - 1] : INT_MIN;
  int upper_threshold =
      (last_pos < ch->num_positions - 1) ? ch->thresholds[last_pos] : INT_MAX;

  // Apply hysteresis margins
  if (lower_threshold != INT_MIN)
    lower_threshold -= ch->hysteresis;
  if (upper_threshold != INT_MAX)
    upper_threshold += ch->hysteresis;

  if (value >= lower_threshold && value < upper_threshold) {
    return last_pos; // stay in current position
  }

  // Value has moved outside hysteresis zone, determine new position
  return determine_position(value, ch);
}

// Returns true if 100ms has elapsed since press_time
bool should_auto_release(struct timespec *press_time) {
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);

  long elapsed_ms = (now.tv_sec - press_time->tv_sec) * 1000 +
                    (now.tv_nsec - press_time->tv_nsec) / 1000000;

  return elapsed_ms >= BUTTON_RELEASE_TIME_MS;
}

void send_release_event(int uinput, int button_code) {
  struct input_event ev;
  memset(&ev, 0, sizeof(ev));
  ev.type = EV_KEY;
  ev.code = button_code;
  ev.value = 0;
  write(uinput, &ev, sizeof(ev));
}

int init_uinput(state_t *state) {
  /* initialize uinput joystick stuff */
  int uinput_fd;
  uinput_fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
  if (uinput_fd < 0) {
    fprintf(stderr, "/dev/uinput: %s\n", strerror(errno));
    exit(1);
  }

  for (int i = 0; i < num_channels; i++) {
    switch (channels[i].type) {
    case CTL_AXIS:
      MUST(ioctl(uinput_fd, UI_SET_EVBIT, EV_ABS), "failed to configure axis");
      MUST(ioctl(uinput_fd, UI_SET_ABSBIT, channels[i].code),
           "failed to configure axis");
      break;
    case CTL_BUTTON:
      MUST(ioctl(uinput_fd, UI_SET_EVBIT, EV_KEY),
           "failed to configure button");
      MUST(ioctl(uinput_fd, UI_SET_KEYBIT, channels[i].code),
           "failed to configure button");
      break;
    case CTL_MULTI:
      MUST(ioctl(uinput_fd, UI_SET_EVBIT, EV_KEY),
           "failed to configure multi-key control");
      // Register all button codes for this multi-position switch
      for (int j = 0; j < channels[i].num_positions; j++) {
        MUST(ioctl(uinput_fd, UI_SET_KEYBIT, channels[i].codes[j]),
             "failed to configure multi-key control");
      }
      break;
    default:
      fprintf(stderr, "invalid control type: %d\n", channels[i].type);
      exit(1);
    }
  }

  struct uinput_user_dev uidev;
  memset(&uidev, 0, sizeof(uidev));
  snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "ppmjoy");
  uidev.id.bustype = BUS_USB;
  uidev.id.vendor = 0x1234;
  uidev.id.product = 0xfedc;
  uidev.id.version = 1;
  for (int i = 0; i < 6; i++) {
    uidev.absmax[i] = (2500 * state->params.rate) / 1000000;
    // set maximum values to a pulse length of 2.5ms
  }
  MUST(write(uinput_fd, &uidev, sizeof(uidev)), "failed to configure uinput");
  MUST(ioctl(uinput_fd, UI_DEV_CREATE), "failed to create uinput device");

  return uinput_fd;
}

int main(int argc, char *argv[]) {
  int i;
  int c;
  int uinput_fd;
  char *alsa_device;
  char *config_path = NULL;

  if ((alsa_device = getenv("PPMJOY_ALSA_DEVICE")) == NULL) {
    alsa_device = "default";
  }

  if ((config_path = getenv("PPMJOY_CONFIG")) == NULL) {
    config_path = "~/.config/ppmjoy.json";
  }

  while (-1 != (c = getopt_long(argc, argv, "d:f:", options, NULL))) {
    switch (c) {
    case 'd':
      alsa_device = strdup(optarg);
      break;
    case 'f':
      config_path = strdup(optarg);
      break;
    }
  }

  // Attempt to load config
  channels = load_config(config_path, &num_channels);

  // Fallback to defaults
  if (!channels) {
    fprintf(stderr, "using default channel configuration\n");
    channels = default_channels;
    num_channels = ARRAY_SIZE(default_channels);
  }

  /* initialize alsa stuff */
  state_t state;
  memset(&state, 0, sizeof(state));
  init_alsa(&state, alsa_device, 1000000, 10, 5, 32700);

  uinput_fd = init_uinput(&state);

  /* read pulses from TX and forward them to uinput */

init: // look for a sync pulse
  for (;;) {
    read_pulse_alsa(&state);
    if (state.pulse.type == SYNC)
      break; // found a complete sync pulse
  }

  int last_position[num_channels];
  button_state_t button_states[num_channels];

  // Initialize last_position array
  for (int i = 0; i < num_channels; i++) {
    if (channels[i].type == CTL_MULTI) {
      last_position[i] = -1; // indicates uninitialized state
    } else {
      last_position[i] = 0; // not used for other channel types
    }
  }

  // Initialize button_states array
  for (int i = 0; i < num_channels; i++) {
    button_states[i].pressed_button_code = -1;
  }

  for (;;) {
    // Check for auto-release (100ms timeout)
    for (i = 0; i < num_channels; i++) {
      if (channels[i].type == CTL_MULTI &&
          button_states[i].pressed_button_code != -1) {
        if (should_auto_release(&button_states[i].press_time)) {
          send_release_event(uinput_fd, button_states[i].pressed_button_code);
          button_states[i].pressed_button_code = -1;
        }
      }
    }

    for (i = 0; i < num_channels; i++) {
      struct input_event ev;
      int value;
      memset(&ev, 0, sizeof(ev));

      // look for a high pulse
      read_pulse_alsa(&state);
      if (state.pulse.type != HIGH)
        goto init;
      value = state.pulse.length;

      // followed by a low pulse
      read_pulse_alsa(&state);
      if (state.pulse.type != LOW)
        goto init;
      value += state.pulse.length;

      switch (channels[i].type) {
      case CTL_AXIS:
        ev.type = EV_ABS;
        ev.code = channels[i].code;
        ev.value = value;
        break;
      case CTL_BUTTON:
        ev.type = EV_KEY;
        ev.code = channels[i].code;
        if (value < channels[i].threshold)
          ev.value = 0;
        else
          ev.value = 1;
        break;
      case CTL_MULTI: {
        ev.type = EV_KEY;
        // Determine which position the switch is in
        int new_pos = determine_position_with_hysteresis(
            value, last_position[i], &channels[i]);

        if (new_pos != last_position[i]) {
          // Release the previously pressed button immediately
          if (button_states[i].pressed_button_code != -1) {
            send_release_event(uinput_fd, button_states[i].pressed_button_code);
          }

          // Send new button press
          ev.code = channels[i].codes[new_pos];
          ev.value = 1;
          last_position[i] = new_pos;

          // Record button press state and timestamp
          button_states[i].pressed_button_code = ev.code;
          clock_gettime(CLOCK_MONOTONIC, &button_states[i].press_time);
        }
        break;
      }
      }

      // send value to uinput
      MUST(write(uinput_fd, &ev, sizeof(ev)), "failed to write uinput event");
    }

    // skip high pulse and following sync pulse
    read_pulse_alsa(&state);
    if (state.pulse.type != HIGH)
      goto init;

    read_pulse_alsa(&state);
    if (state.pulse.type != SYNC)
      goto init;
  }

  destroy_alsa(&state);
  ioctl(uinput_fd, UI_DEV_DESTROY);
  close(uinput_fd);

  // Free config if dynamically allocated
  if (channels != default_channels) {
    free_config(channels);
  }

  exit(0);
}
