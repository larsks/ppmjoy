#include <alsa/asoundlib.h>
#include <limits.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

#include <getopt.h>
#include <unistd.h>

#include "config.h"
#include "event.h"
#include "must.h"

#define CLEAR() printf("\033[H\033[J")
#define HOME() printf("\033[H")

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#define BUTTON_RELEASE_TIME_MS 100

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
  state_t alsa_state;
  int uinput_fd;
} device_context_t;

typedef struct {
  char *alsa_device;
  char *config_path;
  int verbose;
  int monitor;
} app_config_t;

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
  {"help", 0, NULL, 'h'},
  {"device", 1, NULL, 'd'},
  {"config", 1, NULL, 'f'},
  {"verbose", 0, NULL, 'v'},
  {"monitor", 0, NULL, 'm'},
  {NULL, 0, NULL, 0},
    // clang-format on
};

app_config_t app_config;

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

  if (app_config.verbose)
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

int init_uinput(state_t *state) {
  /* initialize uinput joystick stuff */
  int uinput_fd;

  if (app_config.verbose)
    fprintf(stderr, "configuring uinput\n");

  MUST(uinput_fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK),
       "failed to open /dev/uinput");

  for (int i = 0; i < num_channels; i++) {
    switch (channels[i].type) {
    case CTL_AXIS:
      if (app_config.verbose > 1)
        fprintf(stderr, "  channel %i -> %7s %s\n", i, "axis",
                axis2str(channels[i].code));
      MUST(ioctl(uinput_fd, UI_SET_EVBIT, EV_ABS), "failed to configure axis");
      MUST(ioctl(uinput_fd, UI_SET_ABSBIT, channels[i].code),
           "failed to configure axis");
      break;
    case CTL_BUTTON:
      if (app_config.verbose > 1)
        fprintf(stderr, "  channel %i -> %7s %s\n", i, "button",
                button2str(channels[i].code));
      MUST(ioctl(uinput_fd, UI_SET_EVBIT, EV_KEY),
           "failed to configure button");
      MUST(ioctl(uinput_fd, UI_SET_KEYBIT, channels[i].code),
           "failed to configure button");
      break;
    case CTL_MULTI:
      if (app_config.verbose > 1)
        fprintf(stderr, "  channel %i -> %7s ", i, "multi");
      MUST(ioctl(uinput_fd, UI_SET_EVBIT, EV_KEY),
           "failed to configure multi-key control");
      // Register all button codes for this multi-position switch
      for (int j = 0; j < channels[i].num_positions; j++) {
        if (app_config.verbose > 1)
          fprintf(stderr, "%s ", button2str(channels[i].codes[j]));
        MUST(ioctl(uinput_fd, UI_SET_KEYBIT, channels[i].codes[j]),
             "failed to configure multi-key control");
      }
      if (app_config.verbose > 1)
        fprintf(stderr, "\n");
      break;
    default:
      fprintf(stderr, "invalid control type: %d\n", channels[i].type);
      exit(1);
    }
  }

  struct uinput_user_dev uidev;
  char dev_name[128];
  memset(&uidev, 0, sizeof(uidev));
  snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "ppmjoy");
  uidev.id.bustype = BUS_USB;
  uidev.id.vendor = 0x1209;
  uidev.id.product = 0x2640;
  uidev.id.version = 1;
  for (int i = 0; i < 6; i++) {
    uidev.absmax[i] = (2500 * state->params.rate) / 1000000;
    // set maximum values to a pulse length of 2.5ms
  }
  MUST(write(uinput_fd, &uidev, sizeof(uidev)), "failed to configure uinput");
  MUST(ioctl(uinput_fd, UI_DEV_CREATE), "failed to create uinput device");

  if (app_config.verbose) {
    if ((ioctl(uinput_fd, UI_GET_SYSNAME(sizeof(dev_name)), dev_name)) >= 0) {
      fprintf(stderr,
              "created new input device /sys/devices/virtual/input/%s\n",
              dev_name);
    }
  }

  return uinput_fd;
}

void show_usage(FILE *out) {
  fprintf(out, "ppmjoy: usage: ppmjoy [--device|-d alsa_device]"
               " [--config|-f ppmjoy_config]"
               " [--monitor|-m]"
               " [--verbose|-v]"
               "\n");
}

const char *controller2str(int type) {
  char *name = "unknown";

  switch (type) {
  case CTL_AXIS:
    name = "axis";
    break;
  case CTL_BUTTON:
    name = "button";
    break;
  case CTL_MULTI:
    name = "multi";
    break;
  }

  return name;
}

static void parse_arguments(int argc, char *argv[]) {
  int c;

  // Initialize with defaults from environment or hardcoded
  app_config.alsa_device = getenv("PPMJOY_ALSA_DEVICE");
  if (!app_config.alsa_device)
    app_config.alsa_device = "default";

  app_config.config_path = getenv("PPMJOY_CONFIG");
  if (!app_config.config_path)
    app_config.config_path = "~/.config/ppmjoy.json";

  app_config.verbose = 0;
  app_config.monitor = 0;

  while (-1 != (c = getopt_long(argc, argv, "d:f:hvm", options, NULL))) {
    switch (c) {
    case 'h':
      show_usage(stdout);
      exit(0);
    case 'd':
      app_config.alsa_device = strdup(optarg);
      break;
    case 'f':
      app_config.config_path = strdup(optarg);
      break;
    case 'v':
      app_config.verbose++;
      break;
    case 'm':
      app_config.monitor = 1;
      break;
    case '?':
      show_usage(stderr);
      exit(2);
    }
  }
}

static void init_channel_state(int num_channels, int last_position[],
                               button_state_t button_states[]) {
  for (int i = 0; i < num_channels; i++) {
    if (channels[i].type == CTL_MULTI) {
      last_position[i] = -1; // indicates uninitialized state
    } else {
      last_position[i] = 0; // not used for other channel types
    }
  }

  for (int i = 0; i < num_channels; i++) {
    button_states[i].pressed_button_code = -1;
  }
}

static device_context_t init_devices(char *alsa_device) {
  device_context_t ctx;

  // Initialize ALSA
  memset(&ctx.alsa_state, 0, sizeof(ctx.alsa_state));
  init_alsa(&ctx.alsa_state, alsa_device, 1000000, 10, 5, 32700);

  // Initialize uinput
  ctx.uinput_fd = init_uinput(&ctx.alsa_state);

  return ctx;
}

static void display_channel_event(int channel_idx, channel *ch, int value,
                                  struct input_event *ev) {
  if (channel_idx == 0)
    HOME();

  printf("[%d] %7s %d -> %d:%d\n", channel_idx, controller2str(ch->type), value,
         ev->type, ev->code);
}

static void run_event_loop(device_context_t *devices) {
  int last_position[num_channels];
  button_state_t button_states[num_channels];

  init_channel_state(num_channels, last_position, button_states);

  // Wait for initial sync
  wait_for_sync(&devices->alsa_state);

  if (app_config.monitor)
    CLEAR();

  // Main processing loop
  for (;;) {
    // Check for auto-release timeouts
    check_auto_release(devices->uinput_fd, channels, num_channels,
                       button_states);

    // Process all channels in this frame
    for (int i = 0; i < num_channels; i++) {
      struct input_event ev;
      int value;

      // Read pulse pair for this channel
      if (read_channel_pulse(&devices->alsa_state, &value) < 0) {
        // Sync lost, re-synchronize
        wait_for_sync(&devices->alsa_state);
        break; // restart channel loop after sync
      }

      // Generate event from pulse value
      int should_send =
          generate_channel_event(&channels[i], value, i, &button_states[i],
                                 &last_position[i], devices->uinput_fd, &ev);

      // Display in monitor mode
      if (app_config.monitor) {
        display_channel_event(i, &channels[i], value, &ev);
      }

      // Send event to uinput if needed
      if (should_send) {
        MUST(write(devices->uinput_fd, &ev, sizeof(ev)),
             "failed to write uinput event");
      }
    }

    // Validate frame end
    if (validate_frame_end(&devices->alsa_state) < 0) {
      wait_for_sync(&devices->alsa_state);
    }
  }
}

static void cleanup_devices(device_context_t *devices) {
  destroy_alsa(&devices->alsa_state);
  ioctl(devices->uinput_fd, UI_DEV_DESTROY);
  close(devices->uinput_fd);
}

static void cleanup_config(void) {
  if (channels != default_channels) {
    free_config(channels);
  }
}

int main(int argc, char *argv[]) {
  // Parse arguments
  parse_arguments(argc, argv);

  // Load configuration
  if (app_config.verbose)
    fprintf(stderr, "loading configuration from %s\n", app_config.config_path);
  channels = load_config(app_config.config_path, &num_channels);
  if (!channels) {
    const char *error = load_config_error();
    fprintf(stderr, "error: failed to load configuration: %s\n", error);

    if (error) {
      // Hard error - config exists but is invalid
      exit(1);
    } else {
      // Soft error - config file doesn't exist, use defaults
      fprintf(stderr, "       using default channel configuration\n");
      channels = default_channels;
      num_channels = ARRAY_SIZE(default_channels);
    }
  }

  // Initialize devices
  device_context_t devices = init_devices(app_config.alsa_device);

  // Run event loop (never returns)
  run_event_loop(&devices);

  // Cleanup (unreachable but good practice)
  cleanup_devices(&devices);
  cleanup_config();

  return 0;
}
