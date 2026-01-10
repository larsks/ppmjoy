#include <alsa/asoundlib.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "event.h"
#include "log.h"

#define MAX_CHANNELS 16
#define DISCOVERY_FRAMES 10

// Terminal control
#define CLEAR_SCREEN() printf("\033[2J")
#define MOVE_CURSOR(row, col) printf("\033[%d;%dH", (row), (col))
#define HIDE_CURSOR() printf("\033[?25l")
#define SHOW_CURSOR() printf("\033[?25h")
#define BOLD() printf("\033[1m")
#define RESET() printf("\033[0m")

typedef struct {
  int num_channels;
  int min_value[MAX_CHANNELS];
  int max_value[MAX_CHANNELS];
  int current_value[MAX_CHANNELS];
  unsigned long frame_count;
  bool discovery_complete;
} channel_state_t;

static alsa_state_t *global_alsa_state = NULL;
volatile sig_atomic_t running = 1;

void cleanup_on_exit(void) {
  if (global_alsa_state) {
    SHOW_CURSOR();
    printf("\n\nCleaning up...\n");
    destroy_alsa(global_alsa_state);
    global_alsa_state = NULL;
  }
}

void signal_handler(int signum) {
  (void)signum;
  running = 0;
  exit(0);  // Exit immediately, cleanup_on_exit() will be called
}

// Read all channel pulses until we hit SYNC
// Returns number of channels found, or -1 on error
int read_frame_channels(alsa_state_t *state, int values[], int max_channels) {
  int channel_count = 0;

  while (channel_count < max_channels) {
    int value;

    // Read HIGH pulse
    read_pulse_alsa(state);
    if (state->pulse.type == SYNC) {
      // Found start of next frame
      return channel_count;
    }
    if (state->pulse.type != HIGH) {
      return -1; // Unexpected pulse type
    }
    value = state->pulse.length;

    // Read LOW pulse
    read_pulse_alsa(state);
    if (state->pulse.type == SYNC) {
      // Found start of next frame (no LOW after last HIGH)
      return channel_count;
    }
    if (state->pulse.type != LOW) {
      return -1; // Unexpected pulse type
    }
    value += state->pulse.length;

    values[channel_count++] = value;
  }

  // If we get here, we've read max_channels without finding SYNC
  // Skip to SYNC
  for (int i = 0; i < 20; i++) {
    read_pulse_alsa(state);
    if (state->pulse.type == SYNC) {
      return channel_count;
    }
  }

  return -1; // No SYNC found
}

void update_channel_stats(channel_state_t *cs, int values[]) {
  for (int i = 0; i < cs->num_channels; i++) {
    cs->current_value[i] = values[i];

    if (values[i] < cs->min_value[i]) {
      cs->min_value[i] = values[i];
    }
    if (values[i] > cs->max_value[i]) {
      cs->max_value[i] = values[i];
    }
  }
  cs->frame_count++;
}

void display_header(const char *alsa_device, unsigned int rate) {
  MOVE_CURSOR(1, 1);
  BOLD();
  printf("PPM Signal Analyzer");
  RESET();
  printf(" - Device: %s - Rate: %u Hz - Press Ctrl+C to exit\n\n", alsa_device,
         rate);
}

void display_discovery_progress(int frame_num, int total_frames,
                                int detected_channels) {
  MOVE_CURSOR(3, 1);
  printf("Discovering channels... Frame %d/%d - Detected: %d channels\n",
         frame_num, total_frames, detected_channels);
}

void display_channels(channel_state_t *cs) {
  MOVE_CURSOR(3, 1);
  BOLD();
  printf("Channel Analysis - Frame: %lu", cs->frame_count);
  RESET();
  printf("                                        \n\n");

  // Table header
  BOLD();
  printf("  Ch#  Current    Min      Max      Range    Percent\n");
  printf("  ─────────────────────────────────────────────────────────\n");
  RESET();

  for (int i = 0; i < cs->num_channels; i++) {
    int range = cs->max_value[i] - cs->min_value[i];
    int percent = 0;

    if (range > 0) {
      percent = ((cs->current_value[i] - cs->min_value[i]) * 100) / range;
    }

    printf("  %2d   %5d    %5d    %5d    %5d    ", i + 1, cs->current_value[i],
           cs->min_value[i], cs->max_value[i], range);

    // Visual bar
    printf("[");
    int bar_width = 20;
    int filled = (percent * bar_width) / 100;
    for (int j = 0; j < bar_width; j++) {
      if (j < filled)
        printf("█");
      else
        printf("░");
    }
    printf("] %3d%%\n", percent);
  }

  printf("\n");
  printf(
      "  Move controls to see values update. Min/Max show range detected.\n");
}

int discover_channels(alsa_state_t *state, const char *alsa_device) {
  int channel_counts[DISCOVERY_FRAMES];
  int values[MAX_CHANNELS];

  CLEAR_SCREEN();
  HIDE_CURSOR();
  display_header(alsa_device, state->params.rate);

  // Wait for initial sync
  MOVE_CURSOR(3, 1);
  printf("Waiting for PPM signal...\n");
  wait_for_sync(state);

  // Read multiple frames to determine channel count
  for (int frame = 0; frame < DISCOVERY_FRAMES; frame++) {
    int num_channels = read_frame_channels(state, values, MAX_CHANNELS);
    if (num_channels < 0) {
      SHOW_CURSOR();
      fprintf(stderr, "\nError: Failed to read frame %d\n", frame + 1);
      return -1;
    }
    channel_counts[frame] = num_channels;
    display_discovery_progress(frame + 1, DISCOVERY_FRAMES, num_channels);
    usleep(50000); // Small delay for visual feedback
  }

  // Find the most common channel count
  int detected_channels = channel_counts[0];
  int max_count = 1;

  for (int i = 0; i < DISCOVERY_FRAMES; i++) {
    int count = 0;
    for (int j = 0; j < DISCOVERY_FRAMES; j++) {
      if (channel_counts[j] == channel_counts[i]) {
        count++;
      }
    }
    if (count > max_count) {
      max_count = count;
      detected_channels = channel_counts[i];
    }
  }

  return detected_channels;
}

void run_analyzer(alsa_state_t *state, const char *alsa_device) {
  channel_state_t cs = {0};
  int values[MAX_CHANNELS];

  // Discover number of channels
  int num_channels = discover_channels(state, alsa_device);
  if (num_channels <= 0) {
    fprintf(stderr, "Failed to discover channels\n");
    return;
  }

  // Initialize channel state
  cs.num_channels = num_channels;
  for (int i = 0; i < num_channels; i++) {
    cs.min_value[i] = 999999;
    cs.max_value[i] = 0;
    cs.current_value[i] = 0;
  }
  cs.discovery_complete = true;

  // Display initial screen
  CLEAR_SCREEN();
  display_header(alsa_device, state->params.rate);
  MOVE_CURSOR(3, 1);
  printf("Detected %d channels. Reading values...\n\n", num_channels);
  sleep(1);

  // Main display loop
  while (running) {
    int num_read = read_frame_channels(state, values, MAX_CHANNELS);

    if (num_read < 0) {
      // Error reading frame, try to resync
      wait_for_sync(state);
      continue;
    }

    if (num_read != num_channels) {
      // Channel count changed, this shouldn't happen often
      // Just skip this frame
      continue;
    }

    update_channel_stats(&cs, values);
    display_channels(&cs);

    // Small delay to avoid excessive CPU usage and flickering
    usleep(20000); // 20ms = ~50Hz update rate
  }

  SHOW_CURSOR();
  printf("\n\nExiting...\n");
}

void show_usage(const char *prog_name) {
  printf("PPM Signal Analyzer\n\n");
  printf("Usage: %s [options]\n\n", prog_name);
  printf("Options:\n");
  printf("  -d, --device DEVICE    ALSA device (default: default)\n");
  printf("  -r, --rate RATE        Sample rate in Hz (default: 1000000)\n");
  printf("  -h, --help             Show this help\n\n");
  printf("This tool automatically discovers the number of channels in your "
         "PPM signal\n");
  printf("and displays their current, minimum, and maximum values in "
         "real-time.\n");
}

int main(int argc, char *argv[]) {
  char *alsa_device = "default";
  unsigned int rate = 1000000;
  alsa_state_t state = {0};

  // Parse arguments
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      show_usage(argv[0]);
      return 0;
    } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--device") == 0) {
      if (i + 1 < argc) {
        alsa_device = argv[++i];
      } else {
        fprintf(stderr, "Error: %s requires an argument\n", argv[i]);
        return 1;
      }
    } else if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--rate") == 0) {
      if (i + 1 < argc) {
        rate = atoi(argv[++i]);
      } else {
        fprintf(stderr, "Error: %s requires an argument\n", argv[i]);
        return 1;
      }
    } else {
      fprintf(stderr, "Unknown option: %s\n", argv[i]);
      show_usage(argv[0]);
      return 1;
    }
  }

  // Set up signal handler
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);

  // Initialize logging (quiet mode)
  set_log_level(LOG_ERROR);

  // Register cleanup handler for graceful exit
  atexit(cleanup_on_exit);

  // Initialize ALSA
  // Using standard PPM parameters: 1MHz rate, 10ms period, 5ms sync
  if (init_alsa(&state, alsa_device, rate, 10, 5, 32700) != 0) {
    fprintf(stderr, "Failed to initialize ALSA device %s\n", alsa_device);
    return 1;
  }

  // Store state for cleanup handler
  global_alsa_state = &state;

  // Run the analyzer
  run_analyzer(&state, alsa_device);

  // Normal exit path - clear global state before manual cleanup
  global_alsa_state = NULL;

  // Cleanup
  destroy_alsa(&state);

  return 0;
}
