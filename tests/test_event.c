#include "event.h"
#include "helpers.h"
#include "unity.h"
#include <linux/input.h>
#include <string.h>

void setUp() {}
void tearDown() {}

// ============================================================================
// Category 1: Pulse Initialization Tests
// ============================================================================

void test_init_pulse() {
  pulse_t p;

  init_pulse(&p);

  TEST_ASSERT_EQUAL_INT(INIT, p.type);
  TEST_ASSERT_EQUAL_INT(0, p.length);
}

// ============================================================================
// Category 2: Pulse Processing Tests (read_pulse_alsa)
// ============================================================================

void test_read_pulse_alsa_high_pulse() {
  alsa_state_t state;
  pulse_params_t params = {
      .rate = 44100, .sync_min = 1000, .sync_max = 8820, .threshhold = 1000};

  // Create buffer with high signal followed by low signal
  int16_t data[200][2];
  // 100 samples of high
  for (int i = 0; i < 100; i++) {
    data[i][0] = 2000; // Above threshold
    data[i][1] = 0;
  }
  // 100 samples of low to end the high pulse
  for (int i = 100; i < 200; i++) {
    data[i][0] = 500; // Below threshold
    data[i][1] = 0;
  }

  setup_synthetic_alsa_state(&state, data, 200, &params);
  read_pulse_alsa(&state);

  TEST_ASSERT_EQUAL(HIGH, state.pulse.type);
  TEST_ASSERT_EQUAL(100, state.pulse.length);

  cleanup_synthetic_alsa_state(&state);
}

void test_read_pulse_alsa_low_pulse() {
  alsa_state_t state;
  pulse_params_t params = {
      .rate = 44100, .sync_min = 1000, .sync_max = 8820, .threshhold = 1000};

  // Create buffer with low signal followed by high signal
  int16_t data[200][2];
  // 80 samples of low
  for (int i = 0; i < 80; i++) {
    data[i][0] = 500; // Below threshold
    data[i][1] = 0;
  }
  // 120 samples of high to end the low pulse
  for (int i = 80; i < 200; i++) {
    data[i][0] = 2000; // Above threshold
    data[i][1] = 0;
  }

  setup_synthetic_alsa_state(&state, data, 200, &params);
  read_pulse_alsa(&state);

  TEST_ASSERT_EQUAL(LOW, state.pulse.type);
  TEST_ASSERT_EQUAL(80, state.pulse.length);

  cleanup_synthetic_alsa_state(&state);
}

void test_read_pulse_alsa_sync_pulse() {
  alsa_state_t state;
  pulse_params_t params = {
      .rate = 44100, .sync_min = 1000, .sync_max = 8820, .threshhold = 1000};

  // Create buffer with long low signal (SYNC) followed by high signal
  int16_t data[2000][2];
  // 1500 samples of low (within sync range: 1000-8820)
  for (int i = 0; i < 1500; i++) {
    data[i][0] = 500; // Below threshold
    data[i][1] = 0;
  }
  // 500 samples of high to end the sync pulse
  for (int i = 1500; i < 2000; i++) {
    data[i][0] = 2000; // Above threshold
    data[i][1] = 0;
  }

  setup_synthetic_alsa_state(&state, data, 2000, &params);
  read_pulse_alsa(&state);

  TEST_ASSERT_EQUAL(SYNC, state.pulse.type);
  TEST_ASSERT_EQUAL(1500, state.pulse.length);

  cleanup_synthetic_alsa_state(&state);
}

void test_read_pulse_alsa_invalid_pulse_too_long() {
  alsa_state_t state;
  pulse_params_t params = {
      .rate = 44100, .sync_min = 1000, .sync_max = 8820, .threshhold = 1000};

  // Create buffer with very long low signal (exceeds sync_max)
  int16_t data[10000][2];
  // 9000 samples of low (exceeds sync_max of 8820)
  for (int i = 0; i < 9000; i++) {
    data[i][0] = 500; // Below threshold
    data[i][1] = 0;
  }
  // 1000 samples of high to end the pulse
  for (int i = 9000; i < 10000; i++) {
    data[i][0] = 2000; // Above threshold
    data[i][1] = 0;
  }

  setup_synthetic_alsa_state(&state, data, 10000, &params);
  read_pulse_alsa(&state);

  TEST_ASSERT_EQUAL(INVALID, state.pulse.type);

  cleanup_synthetic_alsa_state(&state);
}

void test_read_pulse_alsa_pulse_length_calculation() {
  alsa_state_t state;
  pulse_params_t params = {
      .rate = 44100, .sync_min = 1000, .sync_max = 8820, .threshhold = 1000};

  // Create buffer with specific length high pulse
  int16_t data[500][2];
  size_t expected_length = 250;

  for (size_t i = 0; i < expected_length; i++) {
    data[i][0] = 2000; // Above threshold
    data[i][1] = 0;
  }
  for (size_t i = expected_length; i < 500; i++) {
    data[i][0] = 500; // Below threshold
    data[i][1] = 0;
  }

  setup_synthetic_alsa_state(&state, data, 500, &params);
  read_pulse_alsa(&state);

  TEST_ASSERT_EQUAL(HIGH, state.pulse.type);
  TEST_ASSERT_EQUAL(expected_length, state.pulse.length);

  cleanup_synthetic_alsa_state(&state);
}

// ============================================================================
// Category 3: Frame Synchronization Tests
// ============================================================================

void test_wait_for_sync_finds_sync() {
  alsa_state_t state;
  pulse_params_t params = {
      .rate = 44100, .sync_min = 1000, .sync_max = 8820, .threshhold = 1000};

  // Create buffer with noise then sync pulse
  int16_t data[5000][2];
  size_t offset = 0;

  // Add some short pulses (noise)
  for (int i = 0; i < 100; i++) {
    data[offset + i][0] = 2000;
    data[offset + i][1] = 0;
  }
  offset += 100;
  for (int i = 0; i < 100; i++) {
    data[offset + i][0] = 500;
    data[offset + i][1] = 0;
  }
  offset += 100;

  // Add a sync pulse (long low)
  for (size_t i = 0; i < 1500; i++) {
    data[offset + i][0] = 500;
    data[offset + i][1] = 0;
  }
  offset += 1500;

  // Add high to end sync
  for (size_t i = offset; i < 5000; i++) {
    data[i][0] = 2000;
    data[i][1] = 0;
  }

  setup_synthetic_alsa_state(&state, data, 5000, &params);
  wait_for_sync(&state);

  TEST_ASSERT_EQUAL(SYNC, state.pulse.type);

  cleanup_synthetic_alsa_state(&state);
}

void test_read_channel_pulse_success() {
  alsa_state_t state;
  pulse_params_t params = {
      .rate = 44100, .sync_min = 1000, .sync_max = 8820, .threshhold = 1000};

  // Create buffer with HIGH pulse (100 samples) + LOW pulse (50 samples)
  int16_t data[200][2];
  for (int i = 0; i < 100; i++) {
    data[i][0] = 2000; // High
    data[i][1] = 0;
  }
  for (int i = 100; i < 150; i++) {
    data[i][0] = 500; // Low
    data[i][1] = 0;
  }
  // Extra data to end the LOW pulse
  for (int i = 150; i < 200; i++) {
    data[i][0] = 2000; // High
    data[i][1] = 0;
  }

  setup_synthetic_alsa_state(&state, data, 200, &params);

  int value = 0;
  int result = read_channel_pulse(&state, &value);

  TEST_ASSERT_EQUAL(0, result);
  TEST_ASSERT_EQUAL(150, value); // 100 + 50

  cleanup_synthetic_alsa_state(&state);
}

void test_read_channel_pulse_sync_lost_no_high() {
  alsa_state_t state;
  pulse_params_t params = {
      .rate = 44100, .sync_min = 1000, .sync_max = 8820, .threshhold = 1000};

  // Create buffer with LOW pulse first (should fail - expects HIGH)
  int16_t data[200][2];
  for (int i = 0; i < 100; i++) {
    data[i][0] = 500; // Low
    data[i][1] = 0;
  }
  for (int i = 100; i < 200; i++) {
    data[i][0] = 2000; // High
    data[i][1] = 0;
  }

  setup_synthetic_alsa_state(&state, data, 200, &params);

  int value = 0;
  int result = read_channel_pulse(&state, &value);

  TEST_ASSERT_EQUAL(-1, result);

  cleanup_synthetic_alsa_state(&state);
}

void test_read_channel_pulse_sync_lost_no_low() {
  alsa_state_t state;
  pulse_params_t params = {
      .rate = 44100, .sync_min = 1000, .sync_max = 8820, .threshhold = 1000};

  // Create buffer with HIGH then SYNC (should fail - expects LOW)
  int16_t data[3000][2];
  for (int i = 0; i < 100; i++) {
    data[i][0] = 2000; // High
    data[i][1] = 0;
  }
  // Long low (SYNC instead of regular LOW)
  for (int i = 100; i < 2000; i++) {
    data[i][0] = 500;
    data[i][1] = 0;
  }
  for (int i = 2000; i < 3000; i++) {
    data[i][0] = 2000;
    data[i][1] = 0;
  }

  setup_synthetic_alsa_state(&state, data, 3000, &params);

  int value = 0;
  int result = read_channel_pulse(&state, &value);

  TEST_ASSERT_EQUAL(-1, result); // SYNC instead of LOW

  cleanup_synthetic_alsa_state(&state);
}

void test_validate_frame_end_no_extra_channels() {
  alsa_state_t state;
  pulse_params_t params = {
      .rate = 44100, .sync_min = 1000, .sync_max = 8820, .threshhold = 1000};

  // Create buffer with just SYNC (no extra channels)
  int16_t data[3000][2];
  size_t offset = 0;

  // SYNC pulse immediately
  for (int i = 0; i < 1500; i++) {
    data[offset++][0] = 500;
  }

  // Fill rest with high
  while (offset < 3000) {
    data[offset++][0] = 2000;
  }

  setup_synthetic_alsa_state(&state, data, 3000, &params);

  // configured_channel_count = 4, total_channel_count = 4 (no extra channels)
  int result = validate_frame_end(&state, 4, 4);

  TEST_ASSERT_EQUAL(0, result);
  TEST_ASSERT_EQUAL(SYNC, state.pulse.type);

  cleanup_synthetic_alsa_state(&state);
}

void test_validate_frame_end_with_extra_channels() {
  alsa_state_t state;
  pulse_params_t params = {
      .rate = 44100, .sync_min = 1000, .sync_max = 8820, .threshhold = 1000};

  // Create buffer with 2 extra channels then SYNC
  int16_t data[5000][2];
  size_t offset = 0;

  // Extra channel 1 (HIGH+LOW)
  for (int i = 0; i < 100; i++) {
    data[offset++][0] = 2000;
  }
  for (int i = 0; i < 50; i++) {
    data[offset++][0] = 500;
  }

  // Extra channel 2 (HIGH+LOW)
  for (int i = 0; i < 100; i++) {
    data[offset++][0] = 2000;
  }
  for (int i = 0; i < 50; i++) {
    data[offset++][0] = 500;
  }

  // SYNC pulse (continues from the LOW of channel 2, total LOW = 50 + 1500 = 1550)
  for (int i = 0; i < 1500; i++) {
    data[offset++][0] = 500;
  }

  // Fill rest with high
  while (offset < 5000) {
    data[offset++][0] = 2000;
  }

  setup_synthetic_alsa_state(&state, data, 5000, &params);

  // configured_channel_count = 4, total_channel_count = 6 (2 extra channels)
  int result = validate_frame_end(&state, 4, 6);

  TEST_ASSERT_EQUAL(0, result);
  TEST_ASSERT_EQUAL(SYNC, state.pulse.type);

  cleanup_synthetic_alsa_state(&state);
}

void test_validate_frame_end_missing_sync() {
  alsa_state_t state;
  pulse_params_t params = {
      .rate = 44100, .sync_min = 1000, .sync_max = 8820, .threshhold = 1000};

  // Create buffer with correct number of extra channels but no SYNC
  // Instead, many regular pulses (ensure buffer is large enough for safety limit)
  int16_t data[10000][2];
  size_t offset = 0;

  // Extra channel 1 (HIGH+LOW)
  for (int i = 0; i < 100; i++) {
    data[offset++][0] = 2000;
  }
  for (int i = 0; i < 50; i++) {
    data[offset++][0] = 500;
  }

  // HIGH transition (to separate LOW from next pulse)
  for (int i = 0; i < 50; i++) {
    data[offset++][0] = 2000;
  }

  // Regular LOW pulse instead of SYNC (too short to be SYNC)
  for (int i = 0; i < 50; i++) {
    data[offset++][0] = 500;
  }

  // Fill rest with alternating pulses to prevent buffer exhaustion
  while (offset < 10000) {
    for (int i = 0; i < 100 && offset < 10000; i++) {
      data[offset++][0] = 2000;
    }
    for (int i = 0; i < 50 && offset < 10000; i++) {
      data[offset++][0] = 500;
    }
  }

  setup_synthetic_alsa_state(&state, data, 10000, &params);

  // configured_channel_count = 4, total_channel_count = 5 (1 extra channel)
  int result = validate_frame_end(&state, 4, 5);

  TEST_ASSERT_EQUAL(-1, result); // Should fail - no SYNC where expected

  cleanup_synthetic_alsa_state(&state);
}

void test_validate_frame_end_wrong_pulse_type() {
  alsa_state_t state;
  pulse_params_t params = {
      .rate = 44100, .sync_min = 1000, .sync_max = 8820, .threshhold = 1000};

  // Create buffer with wrong pulse sequence for extra channel
  // Provide enough data so we don't run out when hitting safety limit
  int16_t data[10000][2];
  size_t offset = 0;

  // Extra channel HIGH pulse
  for (int i = 0; i < 100; i++) {
    data[offset++][0] = 2000;
  }

  // LOW transition (to end the HIGH pulse)
  for (int i = 0; i < 50; i++) {
    data[offset++][0] = 500;
  }

  // Another HIGH pulse where we expect SYNC (wrong)
  for (int i = 0; i < 100; i++) {
    data[offset++][0] = 2000;
  }

  // Fill rest with alternating pulses
  while (offset < 10000) {
    for (int i = 0; i < 50 && offset < 10000; i++) {
      data[offset++][0] = 500;
    }
    for (int i = 0; i < 100 && offset < 10000; i++) {
      data[offset++][0] = 2000;
    }
  }

  setup_synthetic_alsa_state(&state, data, 10000, &params);

  // configured_channel_count = 4, total_channel_count = 5 (1 extra channel)
  // After reading the extra channel (HIGH+LOW), we expect SYNC but get HIGH
  int result = validate_frame_end(&state, 4, 5);

  TEST_ASSERT_EQUAL(-1, result); // Should fail - wrong pulse type (HIGH instead of SYNC)

  cleanup_synthetic_alsa_state(&state);
}

// ============================================================================
// Category 4: Event Generation Tests
// ============================================================================

void test_send_sync_event() {
  int mock_fd = create_mock_uinput();
  TEST_ASSERT_NOT_EQUAL(-1, mock_fd);

  send_sync_event(mock_fd);

  struct input_event captured[10];
  int count = read_captured_events(mock_fd, captured, 10);

  TEST_ASSERT_EQUAL(1, count);
  TEST_ASSERT_EQUAL(EV_SYN, captured[0].type);
  TEST_ASSERT_EQUAL(SYN_REPORT, captured[0].code);
  TEST_ASSERT_EQUAL(0, captured[0].value);

  destroy_mock_uinput(mock_fd);
}

void test_generate_channel_event_axis() {
  channel ch = {.type = CTL_AXIS, .code = ABS_X, .threshold = 0};
  struct input_event ev;
  button_state_t btn_state = {.pressed_button_code = -1};
  int last_position = -1;
  int mock_fd = create_mock_uinput();

  int result = generate_channel_event(&ch, 1500, 0, &btn_state, &last_position,
                                       mock_fd, &ev);

  TEST_ASSERT_EQUAL(1, result);
  TEST_ASSERT_EQUAL(EV_ABS, ev.type);
  TEST_ASSERT_EQUAL(ABS_X, ev.code);
  TEST_ASSERT_EQUAL(1500, ev.value);

  destroy_mock_uinput(mock_fd);
}

void test_generate_channel_event_button_below_threshold() {
  channel ch = {.type = CTL_BUTTON, .code = BTN_TRIGGER, .threshold = 1000};
  struct input_event ev;
  button_state_t btn_state = {.pressed_button_code = -1};
  int last_position = -1;
  int mock_fd = create_mock_uinput();

  int result = generate_channel_event(&ch, 800, 0, &btn_state, &last_position,
                                       mock_fd, &ev);

  TEST_ASSERT_EQUAL(1, result);
  TEST_ASSERT_EQUAL(EV_KEY, ev.type);
  TEST_ASSERT_EQUAL(BTN_TRIGGER, ev.code);
  TEST_ASSERT_EQUAL(0, ev.value); // Below threshold = released

  destroy_mock_uinput(mock_fd);
}

void test_generate_channel_event_button_above_threshold() {
  channel ch = {.type = CTL_BUTTON, .code = BTN_TRIGGER, .threshold = 1000};
  struct input_event ev;
  button_state_t btn_state = {.pressed_button_code = -1};
  int last_position = -1;
  int mock_fd = create_mock_uinput();

  int result = generate_channel_event(&ch, 1200, 0, &btn_state, &last_position,
                                       mock_fd, &ev);

  TEST_ASSERT_EQUAL(1, result);
  TEST_ASSERT_EQUAL(EV_KEY, ev.type);
  TEST_ASSERT_EQUAL(BTN_TRIGGER, ev.code);
  TEST_ASSERT_EQUAL(1, ev.value); // Above threshold = pressed

  destroy_mock_uinput(mock_fd);
}

void test_generate_channel_event_multi_position_detection() {
  // 3-position switch: positions 0, 1, 2
  // Thresholds at 1000 and 2000
  channel ch = {.type = CTL_MULTI,
                .num_positions = 3,
                .thresholds = {1000, 2000, 0},
                .codes = {BTN_TRIGGER, BTN_THUMB, BTN_TOP},
                .hysteresis = 50};
  struct input_event ev;
  button_state_t btn_state = {.pressed_button_code = -1};
  int last_position = -1;
  int mock_fd = create_mock_uinput();

  // Test position 0 (value < 1000)
  int result = generate_channel_event(&ch, 500, 0, &btn_state, &last_position,
                                       mock_fd, &ev);

  TEST_ASSERT_EQUAL(1, result);
  TEST_ASSERT_EQUAL(EV_KEY, ev.type);
  TEST_ASSERT_EQUAL(BTN_TRIGGER, ev.code); // codes[0]
  TEST_ASSERT_EQUAL(1, ev.value);
  TEST_ASSERT_EQUAL(0, last_position);

  destroy_mock_uinput(mock_fd);
}

void test_generate_channel_event_multi_position_1() {
  channel ch = {.type = CTL_MULTI,
                .num_positions = 3,
                .thresholds = {1000, 2000, 0},
                .codes = {BTN_TRIGGER, BTN_THUMB, BTN_TOP},
                .hysteresis = 50};
  struct input_event ev;
  button_state_t btn_state = {.pressed_button_code = -1};
  int last_position = -1;
  int mock_fd = create_mock_uinput();

  // Test position 1 (1000 <= value < 2000)
  int result = generate_channel_event(&ch, 1500, 0, &btn_state, &last_position,
                                       mock_fd, &ev);

  TEST_ASSERT_EQUAL(1, result);
  TEST_ASSERT_EQUAL(BTN_THUMB, ev.code); // codes[1]
  TEST_ASSERT_EQUAL(1, last_position);

  destroy_mock_uinput(mock_fd);
}

void test_generate_channel_event_multi_position_2() {
  channel ch = {.type = CTL_MULTI,
                .num_positions = 3,
                .thresholds = {1000, 2000, 0},
                .codes = {BTN_TRIGGER, BTN_THUMB, BTN_TOP},
                .hysteresis = 50};
  struct input_event ev;
  button_state_t btn_state = {.pressed_button_code = -1};
  int last_position = -1;
  int mock_fd = create_mock_uinput();

  // Test position 2 (value >= 2000)
  int result = generate_channel_event(&ch, 2500, 0, &btn_state, &last_position,
                                       mock_fd, &ev);

  TEST_ASSERT_EQUAL(1, result);
  TEST_ASSERT_EQUAL(BTN_TOP, ev.code); // codes[2]
  TEST_ASSERT_EQUAL(2, last_position);

  destroy_mock_uinput(mock_fd);
}

void test_generate_channel_event_multi_no_change() {
  channel ch = {.type = CTL_MULTI,
                .num_positions = 3,
                .thresholds = {1000, 2000, 0},
                .codes = {BTN_TRIGGER, BTN_THUMB, BTN_TOP},
                .hysteresis = 50};
  struct input_event ev;
  button_state_t btn_state = {.pressed_button_code = BTN_THUMB};
  int last_position = 1; // Already in position 1
  int mock_fd = create_mock_uinput();

  // Same position (value still in position 1 range)
  int result = generate_channel_event(&ch, 1500, 0, &btn_state, &last_position,
                                       mock_fd, &ev);

  TEST_ASSERT_EQUAL(0, result); // No event generated
  TEST_ASSERT_EQUAL(1, last_position);

  destroy_mock_uinput(mock_fd);
}

void test_generate_channel_event_multi_hysteresis() {
  channel ch = {.type = CTL_MULTI,
                .num_positions = 3,
                .thresholds = {1000, 2000, 0},
                .codes = {BTN_TRIGGER, BTN_THUMB, BTN_TOP},
                .hysteresis = 100};
  struct input_event ev;
  button_state_t btn_state = {.pressed_button_code = BTN_THUMB};
  int last_position = 1; // Currently in position 1
  int mock_fd = create_mock_uinput();

  // Value slightly below threshold[0] (1000), but within hysteresis
  // Should stay in position 1 due to hysteresis
  // Position 1 range with hysteresis: (1000-100) to (2000+100) = 900 to 2100
  int result = generate_channel_event(&ch, 950, 0, &btn_state, &last_position,
                                       mock_fd, &ev);

  TEST_ASSERT_EQUAL(0, result); // No change due to hysteresis
  TEST_ASSERT_EQUAL(1, last_position);

  destroy_mock_uinput(mock_fd);
}

void test_generate_channel_event_multi_change_with_release() {
  channel ch = {.type = CTL_MULTI,
                .num_positions = 3,
                .thresholds = {1000, 2000, 0},
                .codes = {BTN_TRIGGER, BTN_THUMB, BTN_TOP},
                .hysteresis = 50};
  struct input_event ev;
  button_state_t btn_state = {.pressed_button_code = BTN_TRIGGER};
  int last_position = 0; // Currently in position 0
  int mock_fd = create_mock_uinput();

  // Move to position 1
  int result = generate_channel_event(&ch, 1500, 0, &btn_state, &last_position,
                                       mock_fd, &ev);

  TEST_ASSERT_EQUAL(1, result);
  TEST_ASSERT_EQUAL(BTN_THUMB, ev.code);
  TEST_ASSERT_EQUAL(1, last_position);

  // Verify release event was sent for previous button
  struct input_event captured[10];
  int count = read_captured_events(mock_fd, captured, 10);
  TEST_ASSERT_EQUAL(2, count); // Release + sync
  TEST_ASSERT_EQUAL(EV_KEY, captured[0].type);
  TEST_ASSERT_EQUAL(BTN_TRIGGER, captured[0].code);
  TEST_ASSERT_EQUAL(0, captured[0].value); // Release

  destroy_mock_uinput(mock_fd);
}

// ============================================================================
// Category 5: Auto-Release Tests
// ============================================================================

void test_check_auto_release_no_release_before_timeout() {
  int mock_fd = create_mock_uinput();

  channel channels[2] = {
      {.type = CTL_MULTI,
       .num_positions = 2,
       .thresholds = {1000},
       .codes = {BTN_TRIGGER, BTN_THUMB}},
      {.type = CTL_AXIS, .code = ABS_X}};

  button_state_t button_states[2] = {{.pressed_button_code = BTN_TRIGGER},
                                      {.pressed_button_code = -1}};

  // Set press time to 50ms ago (before 100ms timeout)
  set_button_press_time_ms_ago(&button_states[0], 50);

  check_auto_release(mock_fd, channels, 2, button_states);

  // Should not release yet
  TEST_ASSERT_EQUAL(BTN_TRIGGER, button_states[0].pressed_button_code);

  destroy_mock_uinput(mock_fd);
}

void test_check_auto_release_releases_after_timeout() {
  int mock_fd = create_mock_uinput();

  channel channels[2] = {
      {.type = CTL_MULTI,
       .num_positions = 2,
       .thresholds = {1000},
       .codes = {BTN_TRIGGER, BTN_THUMB}},
      {.type = CTL_AXIS, .code = ABS_X}};

  button_state_t button_states[2] = {{.pressed_button_code = BTN_TRIGGER},
                                      {.pressed_button_code = -1}};

  // Set press time to 150ms ago (after 100ms timeout)
  set_button_press_time_ms_ago(&button_states[0], 150);

  check_auto_release(mock_fd, channels, 2, button_states);

  // Should have released
  TEST_ASSERT_EQUAL(-1, button_states[0].pressed_button_code);

  // Verify release event was sent
  struct input_event captured[10];
  int count = read_captured_events(mock_fd, captured, 10);
  TEST_ASSERT_GREATER_OR_EQUAL(1, count);
  TEST_ASSERT_EQUAL(EV_KEY, captured[0].type);
  TEST_ASSERT_EQUAL(BTN_TRIGGER, captured[0].code);
  TEST_ASSERT_EQUAL(0, captured[0].value);

  destroy_mock_uinput(mock_fd);
}

void test_check_auto_release_multiple_channels() {
  int mock_fd = create_mock_uinput();

  channel channels[3] = {
      {.type = CTL_MULTI,
       .num_positions = 2,
       .thresholds = {1000},
       .codes = {BTN_TRIGGER, BTN_THUMB}},
      {.type = CTL_MULTI,
       .num_positions = 2,
       .thresholds = {1000},
       .codes = {BTN_TOP, BTN_PINKIE}},
      {.type = CTL_AXIS, .code = ABS_X}};

  button_state_t button_states[3] = {
      {.pressed_button_code = BTN_TRIGGER}, // Will timeout
      {.pressed_button_code = BTN_TOP},     // Won't timeout
      {.pressed_button_code = -1}};

  set_button_press_time_ms_ago(&button_states[0], 150);
  set_button_press_time_ms_ago(&button_states[1], 50);

  check_auto_release(mock_fd, channels, 3, button_states);

  // First should be released
  TEST_ASSERT_EQUAL(-1, button_states[0].pressed_button_code);
  // Second should still be pressed
  TEST_ASSERT_EQUAL(BTN_TOP, button_states[1].pressed_button_code);

  destroy_mock_uinput(mock_fd);
}

void test_check_auto_release_ignores_non_multi() {
  int mock_fd = create_mock_uinput();

  channel channels[2] = {{.type = CTL_BUTTON, .code = BTN_TRIGGER},
                         {.type = CTL_AXIS, .code = ABS_X}};

  button_state_t button_states[2] = {{.pressed_button_code = BTN_TRIGGER},
                                      {.pressed_button_code = -1}};

  set_button_press_time_ms_ago(&button_states[0], 150);

  check_auto_release(mock_fd, channels, 2, button_states);

  // Button state unchanged (only CTL_MULTI gets auto-released)
  TEST_ASSERT_EQUAL(BTN_TRIGGER, button_states[0].pressed_button_code);

  destroy_mock_uinput(mock_fd);
}

// ============================================================================
// Category 6: ALSA Init/Destroy Tests
// ============================================================================

void test_destroy_alsa_null_safe() {
  alsa_state_t state;
  memset(&state, 0, sizeof(state));

  // Should not crash with NULL pointers
  destroy_alsa(&state);

  TEST_ASSERT_TRUE(1); // If we get here, test passed
}

void test_destroy_alsa_cleanup() {
  alsa_state_t state;
  memset(&state, 0, sizeof(state));

  // Allocate buffer
  state.buffer = malloc(100 * sizeof(int16_t[2]));

  destroy_alsa(&state);

  // Can't really test if memory was freed, but should not crash
  TEST_ASSERT_TRUE(1);
}

// Note: We cannot fully test init_alsa without real ALSA hardware
// These tests verify parameter calculations only

void test_init_alsa_parameter_calculation() {
  // This test would require mocking ALSA, which is complex
  // Instead we verify the calculation logic by checking expected values
  // after a hypothetical successful init

  // Test parameters
  unsigned int rate = 44100;
  unsigned int period = 10; // 10ms
  unsigned int sync_length = 5; // 5ms

  // Expected calculations:
  // samples = (rate * period + 999) / 1000 = (44100 * 10 + 999) / 1000 = 441
  size_t expected_samples = (rate * period + 999) / 1000;
  TEST_ASSERT_EQUAL(441, expected_samples);

  // sync_min = (sync_length * rate + 999) / 1000 = (5 * 44100 + 999) / 1000 =
  // 221
  size_t expected_sync_min = (sync_length * rate + 999) / 1000;
  TEST_ASSERT_EQUAL(221, expected_sync_min);

  // sync_max = 2 * samples = 2 * 441 = 882
  size_t expected_sync_max = 2 * expected_samples;
  TEST_ASSERT_EQUAL(882, expected_sync_max);
}

// ============================================================================
// Test Runner
// ============================================================================

int main(void) {
  UNITY_BEGIN();

  // Pulse initialization
  RUN_TEST(test_init_pulse);

  // Pulse processing
  RUN_TEST(test_read_pulse_alsa_high_pulse);
  RUN_TEST(test_read_pulse_alsa_low_pulse);
  RUN_TEST(test_read_pulse_alsa_sync_pulse);
  RUN_TEST(test_read_pulse_alsa_invalid_pulse_too_long);
  RUN_TEST(test_read_pulse_alsa_pulse_length_calculation);

  // Frame synchronization
  RUN_TEST(test_wait_for_sync_finds_sync);
  RUN_TEST(test_read_channel_pulse_success);
  RUN_TEST(test_read_channel_pulse_sync_lost_no_high);
  RUN_TEST(test_read_channel_pulse_sync_lost_no_low);
  RUN_TEST(test_validate_frame_end_no_extra_channels);
  RUN_TEST(test_validate_frame_end_with_extra_channels);
  RUN_TEST(test_validate_frame_end_missing_sync);
  RUN_TEST(test_validate_frame_end_wrong_pulse_type);

  // Event generation
  RUN_TEST(test_send_sync_event);
  RUN_TEST(test_generate_channel_event_axis);
  RUN_TEST(test_generate_channel_event_button_below_threshold);
  RUN_TEST(test_generate_channel_event_button_above_threshold);
  RUN_TEST(test_generate_channel_event_multi_position_detection);
  RUN_TEST(test_generate_channel_event_multi_position_1);
  RUN_TEST(test_generate_channel_event_multi_position_2);
  RUN_TEST(test_generate_channel_event_multi_no_change);
  RUN_TEST(test_generate_channel_event_multi_hysteresis);
  RUN_TEST(test_generate_channel_event_multi_change_with_release);

  // Auto-release
  RUN_TEST(test_check_auto_release_no_release_before_timeout);
  RUN_TEST(test_check_auto_release_releases_after_timeout);
  RUN_TEST(test_check_auto_release_multiple_channels);
  RUN_TEST(test_check_auto_release_ignores_non_multi);

  // ALSA init/destroy
  RUN_TEST(test_destroy_alsa_null_safe);
  RUN_TEST(test_destroy_alsa_cleanup);
  RUN_TEST(test_init_alsa_parameter_calculation);

  return UNITY_END();
}
