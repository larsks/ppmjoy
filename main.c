#include <alsa/asoundlib.h>
#include <limits.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/time.h>
#include <time.h>

#include <getopt.h>
#include <unistd.h>

#include "args.h"
#include "config.h"
#include "event.h"
#include "log.h"
#include "must.h"

#define CLEAR() printf("\033[H\033[J")
#define HOME() printf("\033[H")

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#define BUTTON_RELEASE_TIME_MS 100

#define PPMJOY_ID_VENDOR 0x1209
#define PPMJOY_ID_PRODUCT 0x2640

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
  alsa_state_t alsa_state;
  int uinput_fd;
} device_context_t;

channel *channels = NULL;
int num_channels = 0;

// This is the default mapping of channels to input events,
// used if the configuration file does not exist.
static channel default_channels[] = {
    // clang-format off
  {CTL_AXIS,  ABS_RX},
  {CTL_AXIS,  ABS_RY},
  {CTL_AXIS,  ABS_Y},
  {CTL_AXIS,  ABS_X},
    // clang-format on
};
app_config_t app_config;

// Used to produce a list of symbolic key names
// in the --monitor mode output.
static char *list_multi_keys(channel *c) {
  static char buf[128];
  char *ptr = buf;
  char *end = buf + sizeof(buf) - 1;

  for (int j = 0; j < c->num_positions; j++) {
    const char *name = button2str(c->codes[j]);
    if (!name)
      name = "UNKNOWN";

    // Add space before all buttons except the first
    if (j > 0 && ptr < end) {
      *ptr++ = ' ';
    }

    // Append button name
    int written = snprintf(ptr, end - ptr, "%s", name);
    if (written > 0) {
      ptr += MIN(written, end - ptr);
    }
  }

  *ptr = '\0'; // Ensure null termination
  return buf;
}

int init_uinput(alsa_state_t *state) {
  /* initialize uinput joystick stuff */
  int uinput_fd;

  logmsg(LOG_INFO, "configuring uinput");

  MUST(uinput_fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK),
       "failed to open /dev/uinput");

  for (int i = 0; i < num_channels; i++) {
    switch (channels[i].type) {
    case CTL_AXIS:
      logmsg(LOG_DEBUG, "  channel %i -> %7s %s", i, "axis",
             axis2str(channels[i].code));
      MUST(ioctl(uinput_fd, UI_SET_EVBIT, EV_ABS), "failed to configure axis");
      MUST(ioctl(uinput_fd, UI_SET_ABSBIT, channels[i].code),
           "failed to configure axis");
      break;
    case CTL_BUTTON:
      logmsg(LOG_DEBUG, "  channel %i -> %7s %s", i, "button",
             button2str(channels[i].code));
      MUST(ioctl(uinput_fd, UI_SET_EVBIT, EV_KEY),
           "failed to configure button");
      MUST(ioctl(uinput_fd, UI_SET_KEYBIT, channels[i].code),
           "failed to configure button");
      break;
    case CTL_MULTI:
      logmsg(LOG_DEBUG, "  channel %i -> %7s %s", i, "multi",
             list_multi_keys(&channels[i]));
      MUST(ioctl(uinput_fd, UI_SET_EVBIT, EV_KEY),
           "failed to configure multi-key control");
      // Register all button codes for this multi-position switch
      for (int j = 0; j < channels[i].num_positions; j++) {
        MUST(ioctl(uinput_fd, UI_SET_KEYBIT, channels[i].codes[j]),
             "failed to configure multi-key control");
      }
      break;
    default:
      logmsg(LOG_ERROR, "invalid control type: %d", channels[i].type);
      exit(1);
    }
  }

  struct uinput_user_dev uidev;
  char dev_name[128];
  memset(&uidev, 0, sizeof(uidev));
  snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "ppmjoy");
  uidev.id.bustype = BUS_USB;
  uidev.id.vendor = PPMJOY_ID_VENDOR;
  uidev.id.product = PPMJOY_ID_PRODUCT;
  uidev.id.version = 1;
  for (int i = 0; i < 6; i++) {
    uidev.absmax[i] = (2500 * state->params.rate) / 1000000;
    // set maximum values to a pulse length of 2.5ms
  }
  MUST(write(uinput_fd, &uidev, sizeof(uidev)), "failed to configure uinput");
  MUST(ioctl(uinput_fd, UI_DEV_CREATE), "failed to create uinput device");

  if (app_config.verbose) {
    if ((ioctl(uinput_fd, UI_GET_SYSNAME(sizeof(dev_name)), dev_name)) >= 0) {
      logmsg(LOG_INFO, "created new input device /sys/devices/virtual/input/%s",
             dev_name);
    }
  }

  return uinput_fd;
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

  const char *name =
      ev->type == EV_ABS ? axis2str(ev->code) : button2str(ev->code);
  if (name) {
    printf("[%d] %7s %d -> %s\n", channel_idx, controller2str(ch->type), value,
           name);
  } else {
    printf("\n");
  }
}

static void run_event_loop(device_context_t *devices) {
  int last_position[num_channels];
  int last_value[num_channels];
  button_state_t button_states[num_channels];

  init_channel_state(num_channels, last_position, button_states);
  memset(last_value, 0, sizeof(last_value));

  logmsg(LOG_INFO, "waiting for initial sync");

  // Wait for initial sync
  wait_for_sync(&devices->alsa_state);

  logmsg(LOG_INFO, "received initial sync");

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

      if (abs(value - last_value[i]) < 2) {
        value = last_value[i];
      } else {
        last_value[i] = value;
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

    // Send sync event to flush input events for this frame
    send_sync_event(devices->uinput_fd);

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
  set_log_level(LOG_WARNING);

  // Parse arguments
  args_result_t parse_result =
      parse_arguments(argc, argv, &app_config, show_usage);
  switch (parse_result) {
  case ARGS_HELP_REQUESTED:
    exit(0);
  case ARGS_ERROR:
    exit(2);
  default:
    break;
  }

  if (app_config.verbose > 0) {
    set_log_level(MIN(app_config.verbose + 1, LOG_DEBUG));
  }

  // Load configuration
  logmsg(LOG_INFO, "loading configuration from %s", app_config.config_path);
  channels = load_config(app_config.config_path, &num_channels);
  if (!channels) {
    const char *error = load_config_error();
    if (error) {
      // Hard error - config exists but is invalid
      logmsg(LOG_ERROR, "failed to load configuration: %s", error);
      exit(1);
    } else {
      // Soft error - config file doesn't exist, use defaults
      logmsg(LOG_WARNING,
             "config file %s does not exist; using default channel "
             "configuration",
             app_config.config_path);
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
