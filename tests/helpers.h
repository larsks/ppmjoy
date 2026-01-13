#ifndef _HELPERS_H
#define _HELPERS_H

#include "event.h"
#include <linux/input.h>
#include <stdint.h>
#include <stdlib.h>

// Setup alsa_state with synthetic pulse data
// The data array should contain stereo samples [left][right]
// Only the left channel (data[][0]) is used for pulse detection
void setup_synthetic_alsa_state(alsa_state_t *state, int16_t (*data)[2],
                                 size_t data_len, pulse_params_t *params);

// Cleanup synthetic alsa state
void cleanup_synthetic_alsa_state(alsa_state_t *state);

// Helper to create a pulse pattern in a buffer
// Fills buffer with samples that represent a pulse of given type and length
// Returns the number of samples written
size_t create_pulse_pattern(int16_t (*buffer)[2], size_t buffer_capacity,
                             int pulse_type, size_t pulse_len,
                             int16_t threshold);

// Mock uinput file descriptor that captures events
// Returns a file descriptor that can be used like a real uinput device
// Events written to this fd can be read back with read_captured_events()
int create_mock_uinput(void);

// Read events captured by mock uinput
// Returns the number of events read
// The mock_fd should be from create_mock_uinput()
int read_captured_events(int mock_fd, struct input_event *events,
                         size_t max_events);

// Close and cleanup mock uinput
void destroy_mock_uinput(int mock_fd);

// Set button press time to N milliseconds ago
// Useful for testing auto-release timeout logic
void set_button_press_time_ms_ago(button_state_t *state, long ms_ago);

#endif // _HELPERS_H
