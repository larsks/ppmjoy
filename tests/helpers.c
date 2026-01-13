#include "helpers.h"
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// Setup alsa_state with synthetic pulse data
void setup_synthetic_alsa_state(alsa_state_t *state, int16_t (*data)[2],
                                 size_t data_len, pulse_params_t *params) {
  memset(state, 0, sizeof(*state));

  // Copy parameters
  if (params) {
    state->params = *params;
  } else {
    // Default parameters
    state->params.rate = 44100;
    state->params.sync_min = 1000;
    state->params.sync_max = 8820;
    state->params.threshhold = 1000;
  }

  // Allocate and copy buffer
  state->buffer = malloc(data_len * sizeof(int16_t[2]));
  memcpy(state->buffer, data, data_len * sizeof(int16_t[2]));

  // Set samples equal to data length
  // Tests must provide enough data to avoid buffer exhaustion
  state->samples = data_len;

  // Set offset to 0 so reading starts from beginning
  state->offset = 0;

  // Initialize pulse
  init_pulse(&state->pulse);

  // Note: handle is left NULL since we're not using real ALSA
  state->handle = NULL;
}

// Cleanup synthetic alsa state
void cleanup_synthetic_alsa_state(alsa_state_t *state) {
  if (state->buffer) {
    free(state->buffer);
    state->buffer = NULL;
  }
}

// Helper to create a pulse pattern in a buffer
size_t create_pulse_pattern(int16_t (*buffer)[2], size_t buffer_capacity,
                             int pulse_type, size_t pulse_len,
                             int16_t threshold) {
  if (pulse_len > buffer_capacity) {
    pulse_len = buffer_capacity;
  }

  int16_t value;
  switch (pulse_type) {
  case HIGH:
    value = threshold + 500; // Above threshold
    break;
  case LOW:
    value = threshold - 500; // Below threshold
    break;
  case SYNC:
    // SYNC is detected as a long LOW pulse, so use low value
    value = threshold - 500;
    break;
  default:
    value = 0;
    break;
  }

  for (size_t i = 0; i < pulse_len; i++) {
    buffer[i][0] = value; // Left channel for pulse detection
    buffer[i][1] = 0;     // Right channel unused
  }

  return pulse_len;
}

// Static storage for mock uinput pipe fds
#define MAX_MOCK_PIPES 4
static struct {
  int read_fd;
  int write_fd;
  int in_use;
} mock_pipes[MAX_MOCK_PIPES];

static int find_free_pipe_slot(void) {
  for (int i = 0; i < MAX_MOCK_PIPES; i++) {
    if (!mock_pipes[i].in_use) {
      return i;
    }
  }
  return -1;
}

static int find_pipe_by_write_fd(int fd) {
  for (int i = 0; i < MAX_MOCK_PIPES; i++) {
    if (mock_pipes[i].in_use && mock_pipes[i].write_fd == fd) {
      return i;
    }
  }
  return -1;
}

// Create mock uinput file descriptor
int create_mock_uinput(void) {
  int slot = find_free_pipe_slot();
  if (slot < 0) {
    return -1; // No free slots
  }

  int pipefd[2];
  if (pipe(pipefd) == -1) {
    return -1;
  }

  // Make read end non-blocking so read_captured_events doesn't hang
  int flags = fcntl(pipefd[0], F_GETFL, 0);
  fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK);

  mock_pipes[slot].read_fd = pipefd[0];
  mock_pipes[slot].write_fd = pipefd[1];
  mock_pipes[slot].in_use = 1;

  return pipefd[1]; // Return write end for use as "uinput_fd"
}

// Read events captured by mock uinput
int read_captured_events(int mock_fd, struct input_event *events,
                         size_t max_events) {
  int slot = find_pipe_by_write_fd(mock_fd);
  if (slot < 0) {
    return -1;
  }

  int read_fd = mock_pipes[slot].read_fd;
  size_t count = 0;

  while (count < max_events) {
    struct input_event ev;
    ssize_t n = read(read_fd, &ev, sizeof(ev));

    if (n == sizeof(ev)) {
      events[count++] = ev;
    } else {
      break; // No more data or error
    }
  }

  return count;
}

// Destroy mock uinput
void destroy_mock_uinput(int mock_fd) {
  int slot = find_pipe_by_write_fd(mock_fd);
  if (slot < 0) {
    return;
  }

  close(mock_pipes[slot].read_fd);
  close(mock_pipes[slot].write_fd);
  mock_pipes[slot].in_use = 0;
}

// Set button press time to N milliseconds ago
void set_button_press_time_ms_ago(button_state_t *state, long ms_ago) {
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);

  // Subtract milliseconds
  long sec_ago = ms_ago / 1000;
  long nsec_ago = (ms_ago % 1000) * 1000000;

  state->press_time.tv_sec = now.tv_sec - sec_ago;
  state->press_time.tv_nsec = now.tv_nsec - nsec_ago;

  // Handle negative nanoseconds
  if (state->press_time.tv_nsec < 0) {
    state->press_time.tv_sec -= 1;
    state->press_time.tv_nsec += 1000000000;
  }
}
