#ifndef _EVENT_H
#define _EVENT_H

#include "config.h"
#include <alsa/asoundlib.h>
#include <linux/input.h>
#include <stdint.h>
#include <time.h>

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
  pulse_params_t params;
  size_t samples;
  pulse_t pulse;
  int16_t (*buffer)[2];
  size_t offset;
  snd_pcm_t *handle;
} alsa_state_t;

typedef struct {
  int pressed_button_code;
  struct timespec press_time;
} button_state_t;

// Initialize a pulse structure
void init_pulse(pulse_t *p);

// Read a pulse from ALSA
void read_pulse_alsa(alsa_state_t *state);

// Wait for initial sync pulse
void wait_for_sync(alsa_state_t *state);

// Check all channels for auto-release timeout (CTL_MULTI)
void check_auto_release(int uinput_fd, channel *channels, int num_channels,
                        button_state_t *button_states);

// Read pulse pair for a channel (high + low)
// Returns 0 on success, -1 if sync lost
int read_channel_pulse(alsa_state_t *state, int *value_out);

// Generate input event from raw pulse value
// Returns 1 if event should be sent, 0 otherwise
int generate_channel_event(channel *ch, int value, int channel_idx,
                           button_state_t *btn_state, int *last_position,
                           int uinput_fd, struct input_event *ev);

// Validate frame end (trailing high + sync pulses)
// Returns 0 on success, -1 if sync lost
int validate_frame_end(alsa_state_t *state);

#endif // _EVENT_H
