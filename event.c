#include <alsa/asoundlib.h>
#include <limits.h>
#include <linux/input.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "config.h"
#include "event.h"
#include "log.h"

#define BUTTON_RELEASE_TIME_MS 100

// Forward declarations for static helpers
static bool datum_to_pulse(int16_t datum, pulse_params_t *params, pulse_t *p);
static bool data_to_pulse(int16_t (*data)[2], size_t *offset, size_t samples,
                          pulse_params_t *params, pulse_t *p);
static int should_auto_release(struct timespec *press_time);
static void send_release_event(int uinput, int button_code);
static int determine_position(int value, channel *ch);
static int determine_position_with_hysteresis(int value, int last_pos,
                                              channel *ch);
static void process_axis_channel(channel *ch, int value,
                                 struct input_event *ev);
static void process_button_channel(channel *ch, int value,
                                   struct input_event *ev);
static int process_multi_channel(channel *ch, int value, int channel_idx,
                                 button_state_t *btn_state, int *last_position,
                                 int uinput_fd, struct input_event *ev);

// Pulse processing functions
void init_pulse(pulse_t *p) {
  if (p) {
    p->type = INIT;
    p->length = 0;
  }
}

// return true if datum starts a new pulse, false otherwise
static bool datum_to_pulse(int16_t datum, pulse_params_t *params, pulse_t *p) {
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
      logmsg(LOG_ERROR, "unknown pulse type");
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
      logmsg(LOG_ERROR, "unknown pulse type");
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
  }

  return complete;
}

static bool data_to_pulse(int16_t (*data)[2], size_t *offset, size_t samples,
                          pulse_params_t *params, pulse_t *p) {
  while (*offset < samples) {
    if (datum_to_pulse(data[*offset][0], params, p))
      return true; // datum starts a new pulse, so don't update offset
    else
      (*offset)++;
  }
  return false;
}

void read_pulse_alsa(alsa_state_t *state) {
  int err;
  init_pulse(&state->pulse);
  for (;;) {
    if (state->offset == state->samples) { // refill buffer
      if ((err = snd_pcm_readi(state->handle, state->buffer, state->samples)) !=
          state->samples) {
        logmsg(LOG_ERROR, "read from audio interface failed (%s)\n",
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

// Check if BUTTON_RELEASE_TIME_MS (100ms by default) has elapsed since
// press_time.
static int should_auto_release(struct timespec *press_time) {
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);

  long elapsed_ms = (now.tv_sec - press_time->tv_sec) * 1000 +
                    (now.tv_nsec - press_time->tv_nsec) / 1000000;

  return elapsed_ms >= BUTTON_RELEASE_TIME_MS;
}

// Send a synchronization event to flush the input buffer
void send_sync_event(int uinput_fd) {
  struct input_event sync_ev;
  memset(&sync_ev, 0, sizeof(sync_ev));
  sync_ev.type = EV_SYN;
  sync_ev.code = SYN_REPORT;
  sync_ev.value = 0;
  write(uinput_fd, &sync_ev, sizeof(sync_ev));
}

// Send a button release event. This is used to generate synthetic press/release
// pairs for "multi" type controls.
static void send_release_event(int uinput, int button_code) {
  struct input_event ev;
  memset(&ev, 0, sizeof(ev));
  ev.type = EV_KEY;
  ev.code = button_code;
  ev.value = 0;
  write(uinput, &ev, sizeof(ev));
  send_sync_event(uinput);
}

// Determine which position a value maps to for a multi-position switch
static int determine_position(int value, channel *ch) {
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
static int determine_position_with_hysteresis(int value, int last_pos,
                                              channel *ch) {
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

// Generate uinput event for an axis channel
static void process_axis_channel(channel *ch, int value,
                                 struct input_event *ev) {
  ev->type = EV_ABS;
  ev->code = ch->code;
  ev->value = value;
}

// Generate uinput event for a button channel
static void process_button_channel(channel *ch, int value,
                                   struct input_event *ev) {
  ev->type = EV_KEY;
  ev->code = ch->code;
  if (value < ch->threshold)
    ev->value = 0;
  else
    ev->value = 1;
}

// Process multi-position switch channel
// Returns 1 if an event was generated, 0 otherwise
static int process_multi_channel(channel *ch, int value, int channel_idx,
                                 button_state_t *btn_state, int *last_position,
                                 int uinput_fd, struct input_event *ev) {
  ev->type = EV_KEY;

  // Determine which position the switch is in
  int new_pos = determine_position_with_hysteresis(value, *last_position, ch);

  if (new_pos != *last_position) {
    // Release the previously pressed button immediately
    if (btn_state->pressed_button_code != -1) {
      send_release_event(uinput_fd, btn_state->pressed_button_code);
    }

    // Send new button press
    ev->code = ch->codes[new_pos];
    ev->value = 1;
    *last_position = new_pos;

    // Record button press state and timestamp
    btn_state->pressed_button_code = ev->code;
    clock_gettime(CLOCK_MONOTONIC, &btn_state->press_time);

    return 1; // event should be sent
  }

  return 0; // no event to send
}

// Wait for initial sync pulse
void wait_for_sync(alsa_state_t *state) {
  for (;;) {
    read_pulse_alsa(state);
    if (state->pulse.type == SYNC)
      break; // found a complete sync pulse
  }
}

// Check all channels for auto-release timeout (CTL_MULTI)
void check_auto_release(int uinput_fd, channel *channels, int num_channels,
                        button_state_t *button_states) {
  for (int i = 0; i < num_channels; i++) {
    if (channels[i].type == CTL_MULTI &&
        button_states[i].pressed_button_code != -1) {
      if (should_auto_release(&button_states[i].press_time)) {
        send_release_event(uinput_fd, button_states[i].pressed_button_code);
        button_states[i].pressed_button_code = -1;
      }
    }
  }
}

// Read pulse pair for a channel (high + low)
// Returns 0 on success, -1 if sync lost
int read_channel_pulse(alsa_state_t *state, int *value_out) {
  int value;

  // Look for a high pulse
  read_pulse_alsa(state);
  if (state->pulse.type != HIGH)
    return -1;
  value = state->pulse.length;

  // Followed by a low pulse
  read_pulse_alsa(state);
  if (state->pulse.type != LOW)
    return -1;
  value += state->pulse.length;

  *value_out = value;
  return 0;
}

// Generate input event from raw pulse value
// Returns 1 if event should be sent, 0 otherwise
int generate_channel_event(channel *ch, int value, int channel_idx,
                           button_state_t *btn_state, int *last_position,
                           int uinput_fd, struct input_event *ev) {
  memset(ev, 0, sizeof(*ev));

  switch (ch->type) {
  case CTL_AXIS:
    process_axis_channel(ch, value, ev);
    return 1;
  case CTL_BUTTON:
    process_button_channel(ch, value, ev);
    return 1;
  case CTL_MULTI:
    return process_multi_channel(ch, value, channel_idx, btn_state,
                                 last_position, uinput_fd, ev);
  default:
    return 0;
  }
}

// Validate frame end (trailing high + sync pulses)
// Returns 0 on success, -1 if sync lost
int validate_frame_end(alsa_state_t *state) {
  // Skip high pulse
  read_pulse_alsa(state);
  if (state->pulse.type != HIGH)
    return -1;

  // Followed by sync pulse
  read_pulse_alsa(state);
  if (state->pulse.type != SYNC)
    return -1;

  return 0;
}
