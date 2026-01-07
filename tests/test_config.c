#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "must.h"
#include "unity.h"

static char workdir[128];

void setUp(void) {
  sprintf(workdir, "testXXXXXX");
  mkdtemp(workdir);
}

void tearDown(void) {
  char command[1024];
  snprintf(command, sizeof(command), "rm -rf %s", workdir);
  MUST(system(command), "failed to remove temporary directory");
}

void writeFile(const char *path, const char *data) {
  FILE *fp = fopen(path, "w");
  fprintf(fp, "%s", data);
  fclose(fp);
}

void test_axis2str(void) {
  TEST_ASSERT_EQUAL_STRING("ABS_X", axis2str(ABS_X));
  TEST_ASSERT_EQUAL_STRING("ABS_Y", axis2str(ABS_Y));

  // test that an invalid code returns NULL
  TEST_ASSERT_NULL(axis2str(1024));
}

void test_button2str(void) {
  TEST_ASSERT_EQUAL_STRING("BTN_1", button2str(BTN_1));
  TEST_ASSERT_EQUAL_STRING("BTN_START", button2str(BTN_START));

  // test that an invalid code returns NULL
  TEST_ASSERT_NULL(button2str(1024));
}

void test_load_valid_config(void) {
  char cfgFile[512];
  snprintf(cfgFile, sizeof(cfgFile) - 1, "%s/config.toml", workdir);
  writeFile(cfgFile, "[[channels]]\ntype = \"axis\"\ncode = \"ABS_X\"\n\n"
                     "[[channels]]\ntype = \"axis\"\ncode = \"ABS_Y\"\n\n"
                     "[[channels]]\ntype = \"axis\"\ncode = \"ABS_RX\"\n\n"
                     "[[channels]]\ntype = \"axis\"\ncode = \"ABS_RY\"\n");

  channel *channels;
  int num_channels;

  channels = load_config(cfgFile, &num_channels);
  TEST_ASSERT_NOT_NULL(channels);
  TEST_ASSERT_EQUAL_INT(4, num_channels);

  for (int i = 0; i < num_channels; i++) {
    TEST_ASSERT_EQUAL_INT(CTL_AXIS, channels[i].type);
  }

  TEST_ASSERT_EQUAL_INT(ABS_X, channels[0].code);
  TEST_ASSERT_EQUAL_INT(ABS_Y, channels[1].code);
  TEST_ASSERT_EQUAL_INT(ABS_RX, channels[2].code);
  TEST_ASSERT_EQUAL_INT(ABS_RY, channels[3].code);
}

void test_load_missing_config(void) {
  channel *channels;
  int num_channels;

  channels = load_config("/does-not-exist", &num_channels);
  TEST_ASSERT_NULL(channels);
}

void test_missing_channels_field(void) {
  char path[256];
  snprintf(path, sizeof(path), "%s/no_channels.toml", workdir);
  writeFile(path, "foo = \"bar\"");

  channel *channels;
  int num_channels;

  channels = load_config(path, &num_channels);
  TEST_ASSERT_NULL(channels);
  const char *error = load_config_error();
  TEST_ASSERT_NOT_NULL(error);
  TEST_ASSERT_TRUE(strstr(error, "channels") != NULL);
}

void test_channels_not_array(void) {
  char path[256];
  snprintf(path, sizeof(path), "%s/channels_not_array.toml", workdir);
  writeFile(path, "channels = \"not-an-array\"");

  channel *channels;
  int num_channels;

  channels = load_config(path, &num_channels);
  TEST_ASSERT_NULL(channels);
  const char *error = load_config_error();
  TEST_ASSERT_NOT_NULL(error);
  TEST_ASSERT_TRUE(strstr(error, "array") != NULL);
}

void test_empty_channels_array(void) {
  char path[256];
  snprintf(path, sizeof(path), "%s/empty_channels.toml", workdir);
  writeFile(path, "channels = []");

  channel *channels;
  int num_channels;

  channels = load_config(path, &num_channels);
  TEST_ASSERT_NULL(channels);
  const char *error = load_config_error();
  TEST_ASSERT_NOT_NULL(error);
  TEST_ASSERT_TRUE(strstr(error, "empty") != NULL);
}

void test_channel_not_object(void) {
  char path[256];
  snprintf(path, sizeof(path), "%s/channel_not_object.toml", workdir);
  writeFile(path, "channels = [\"not-an-object\"]");

  channel *channels;
  int num_channels;

  channels = load_config(path, &num_channels);
  TEST_ASSERT_NULL(channels);
  const char *error = load_config_error();
  TEST_ASSERT_NOT_NULL(error);
  TEST_ASSERT_TRUE(strstr(error, "object") != NULL);
}

void test_channel_missing_type(void) {
  char path[256];
  snprintf(path, sizeof(path), "%s/missing_type.toml", workdir);
  writeFile(path, "[[channels]]\ncode = \"ABS_X\"");

  channel *channels;
  int num_channels;

  channels = load_config(path, &num_channels);
  TEST_ASSERT_NULL(channels);
  const char *error = load_config_error();
  TEST_ASSERT_NOT_NULL(error);
  TEST_ASSERT_TRUE(strstr(error, "type") != NULL);
}

void test_unknown_channel_type(void) {
  char path[256];
  snprintf(path, sizeof(path), "%s/unknown_type.toml", workdir);
  writeFile(path, "[[channels]]\ntype = \"invalid\"");

  channel *channels;
  int num_channels;

  channels = load_config(path, &num_channels);
  TEST_ASSERT_NULL(channels);
  const char *error = load_config_error();
  TEST_ASSERT_NOT_NULL(error);
  TEST_ASSERT_TRUE(strstr(error, "unknown") != NULL);
}

void test_axis_missing_code(void) {
  char path[256];
  snprintf(path, sizeof(path), "%s/axis_missing_code.toml", workdir);
  writeFile(path, "[[channels]]\ntype = \"axis\"");

  channel *channels;
  int num_channels;

  channels = load_config(path, &num_channels);
  TEST_ASSERT_NULL(channels);
  const char *error = load_config_error();
  TEST_ASSERT_NOT_NULL(error);
  TEST_ASSERT_TRUE(strstr(error, "code") != NULL);
}

void test_axis_invalid_code(void) {
  char path[256];
  snprintf(path, sizeof(path), "%s/axis_invalid_code.toml", workdir);
  writeFile(path, "[[channels]]\ntype = \"axis\"\ncode = \"INVALID_CODE\"");

  channel *channels;
  int num_channels;

  channels = load_config(path, &num_channels);
  TEST_ASSERT_NULL(channels);
  const char *error = load_config_error();
  TEST_ASSERT_NOT_NULL(error);
  TEST_ASSERT_TRUE(strstr(error, "unknown") != NULL ||
                   strstr(error, "axis") != NULL);
}

void test_button_missing_code(void) {
  char path[256];
  snprintf(path, sizeof(path), "%s/button_missing_code.toml", workdir);
  writeFile(path, "[[channels]]\ntype = \"button\"\nthreshold = 512");

  channel *channels;
  int num_channels;

  channels = load_config(path, &num_channels);
  TEST_ASSERT_NULL(channels);
  const char *error = load_config_error();
  TEST_ASSERT_NOT_NULL(error);
  TEST_ASSERT_TRUE(strstr(error, "code") != NULL);
}

void test_button_invalid_code(void) {
  char path[256];
  snprintf(path, sizeof(path), "%s/button_invalid_code.toml", workdir);
  writeFile(
      path,
      "[[channels]]\ntype = \"button\"\ncode = \"INVALID\"\nthreshold = 512");

  channel *channels;
  int num_channels;

  channels = load_config(path, &num_channels);
  TEST_ASSERT_NULL(channels);
  const char *error = load_config_error();
  TEST_ASSERT_NOT_NULL(error);
  TEST_ASSERT_TRUE(strstr(error, "unknown") != NULL ||
                   strstr(error, "button") != NULL);
}

void test_button_missing_threshold(void) {
  char path[256];
  snprintf(path, sizeof(path), "%s/button_missing_threshold.toml", workdir);
  writeFile(path, "[[channels]]\ntype = \"button\"\ncode = \"BTN_1\"");

  channel *channels;
  int num_channels;

  channels = load_config(path, &num_channels);
  TEST_ASSERT_NULL(channels);
  const char *error = load_config_error();
  TEST_ASSERT_NOT_NULL(error);
  TEST_ASSERT_TRUE(strstr(error, "threshold") != NULL);
}

void test_multi_missing_num_positions(void) {
  char path[256];
  snprintf(path, sizeof(path), "%s/multi_missing_positions.toml", workdir);
  writeFile(path, "[[channels]]\ntype = \"multi\"");

  channel *channels;
  int num_channels;

  channels = load_config(path, &num_channels);
  TEST_ASSERT_NULL(channels);
  const char *error = load_config_error();
  TEST_ASSERT_NOT_NULL(error);
  TEST_ASSERT_TRUE(strstr(error, "num_positions") != NULL);
}

void test_multi_invalid_num_positions(void) {
  char path[256];
  snprintf(path, sizeof(path), "%s/multi_invalid_positions.toml", workdir);
  writeFile(path, "[[channels]]\ntype = \"multi\"\nnum_positions = 5");

  channel *channels;
  int num_channels;

  channels = load_config(path, &num_channels);
  TEST_ASSERT_NULL(channels);
  const char *error = load_config_error();
  TEST_ASSERT_NOT_NULL(error);
  TEST_ASSERT_TRUE(strstr(error, "2-4") != NULL ||
                   strstr(error, "num_positions") != NULL);
}

void test_multi_missing_thresholds(void) {
  char path[256];
  snprintf(path, sizeof(path), "%s/multi_missing_thresholds.toml", workdir);
  writeFile(path, "[[channels]]\ntype = \"multi\"\nnum_positions = 2");

  channel *channels;
  int num_channels;

  channels = load_config(path, &num_channels);
  TEST_ASSERT_NULL(channels);
  const char *error = load_config_error();
  TEST_ASSERT_NOT_NULL(error);
  TEST_ASSERT_TRUE(strstr(error, "thresholds") != NULL);
}

void test_multi_wrong_threshold_count(void) {
  char path[256];
  snprintf(path, sizeof(path), "%s/multi_wrong_threshold_count.toml", workdir);
  writeFile(path, "[[channels]]\ntype = \"multi\"\nnum_positions = 3\n"
                  "thresholds = [512]");

  channel *channels;
  int num_channels;

  channels = load_config(path, &num_channels);
  TEST_ASSERT_NULL(channels);
  const char *error = load_config_error();
  TEST_ASSERT_NOT_NULL(error);
  TEST_ASSERT_TRUE(strstr(error, "thresholds") != NULL ||
                   strstr(error, "expected") != NULL);
}

void test_multi_missing_codes(void) {
  char path[256];
  snprintf(path, sizeof(path), "%s/multi_missing_codes.toml", workdir);
  writeFile(path, "[[channels]]\ntype = \"multi\"\nnum_positions = 2\n"
                  "thresholds = [512]");

  channel *channels;
  int num_channels;

  channels = load_config(path, &num_channels);
  TEST_ASSERT_NULL(channels);
  const char *error = load_config_error();
  TEST_ASSERT_NOT_NULL(error);
  TEST_ASSERT_TRUE(strstr(error, "codes") != NULL);
}

void test_multi_wrong_codes_count(void) {
  char path[256];
  snprintf(path, sizeof(path), "%s/multi_wrong_codes_count.toml", workdir);
  writeFile(path, "[[channels]]\ntype = \"multi\"\nnum_positions = 2\n"
                  "thresholds = [512]\ncodes = [\"BTN_1\"]");

  channel *channels;
  int num_channels;

  channels = load_config(path, &num_channels);
  TEST_ASSERT_NULL(channels);
  const char *error = load_config_error();
  TEST_ASSERT_NOT_NULL(error);
  TEST_ASSERT_TRUE(strstr(error, "codes") != NULL ||
                   strstr(error, "expected") != NULL);
}

void test_multi_invalid_code(void) {
  char path[256];
  snprintf(path, sizeof(path), "%s/multi_invalid_code.toml", workdir);
  writeFile(path, "[[channels]]\ntype = \"multi\"\nnum_positions = 2\n"
                  "thresholds = [512]\ncodes = [\"BTN_1\", \"INVALID\"]");

  channel *channels;
  int num_channels;

  channels = load_config(path, &num_channels);
  TEST_ASSERT_NULL(channels);
  const char *error = load_config_error();
  TEST_ASSERT_NOT_NULL(error);
  TEST_ASSERT_TRUE(strstr(error, "unknown") != NULL ||
                   strstr(error, "button") != NULL);
}

void test_button_valid_config(void) {
  char path[256];
  snprintf(path, sizeof(path), "%s/button_valid.toml", workdir);
  writeFile(path, "[[channels]]\ntype = \"button\"\ncode = \"BTN_1\"\n"
                  "threshold = 512");

  channel *channels;
  int num_channels;

  channels = load_config(path, &num_channels);
  TEST_ASSERT_NOT_NULL(channels);
  TEST_ASSERT_EQUAL_INT(1, num_channels);
  TEST_ASSERT_EQUAL_INT(CTL_BUTTON, channels[0].type);
  TEST_ASSERT_EQUAL_INT(BTN_1, channels[0].code);
  TEST_ASSERT_EQUAL_INT(512, channels[0].threshold);

  free_config(channels);
}

void test_multi_valid_config(void) {
  char path[256];
  snprintf(path, sizeof(path), "%s/multi_valid.toml", workdir);
  writeFile(path, "[[channels]]\ntype = \"multi\"\nnum_positions = 2\n"
                  "thresholds = [512]\ncodes = [\"BTN_1\", \"BTN_2\"]");

  channel *channels;
  int num_channels;

  channels = load_config(path, &num_channels);
  TEST_ASSERT_NOT_NULL(channels);
  TEST_ASSERT_EQUAL_INT(1, num_channels);
  TEST_ASSERT_EQUAL_INT(CTL_MULTI, channels[0].type);
  TEST_ASSERT_EQUAL_INT(2, channels[0].num_positions);
  TEST_ASSERT_EQUAL_INT(512, channels[0].thresholds[0]);
  TEST_ASSERT_EQUAL_INT(BTN_1, channels[0].codes[0]);
  TEST_ASSERT_EQUAL_INT(BTN_2, channels[0].codes[1]);
  TEST_ASSERT_EQUAL_INT(0, channels[0].hysteresis);

  free_config(channels);
}

void test_multi_with_hysteresis(void) {
  char path[256];
  snprintf(path, sizeof(path), "%s/multi_hysteresis.toml", workdir);
  writeFile(path, "[[channels]]\ntype = \"multi\"\nnum_positions = 2\n"
                  "thresholds = [512]\ncodes = [\"BTN_1\", \"BTN_2\"]\n"
                  "hysteresis = 10");

  channel *channels;
  int num_channels;

  channels = load_config(path, &num_channels);
  TEST_ASSERT_NOT_NULL(channels);
  TEST_ASSERT_EQUAL_INT(1, num_channels);
  TEST_ASSERT_EQUAL_INT(CTL_MULTI, channels[0].type);
  TEST_ASSERT_EQUAL_INT(10, channels[0].hysteresis);

  free_config(channels);
}

void test_mixed_channel_types(void) {
  char path[256];
  snprintf(path, sizeof(path), "%s/mixed_types.toml", workdir);
  writeFile(
      path,
      "[[channels]]\ntype = \"axis\"\ncode = \"ABS_X\"\n\n"
      "[[channels]]\ntype = \"button\"\ncode = \"BTN_1\"\nthreshold = 512\n\n"
      "[[channels]]\ntype = \"multi\"\nnum_positions = 2\nthresholds = [512]\n"
      "codes = [\"BTN_2\", \"BTN_3\"]");

  channel *channels;
  int num_channels;

  channels = load_config(path, &num_channels);
  TEST_ASSERT_NOT_NULL(channels);
  TEST_ASSERT_EQUAL_INT(3, num_channels);
  TEST_ASSERT_EQUAL_INT(CTL_AXIS, channels[0].type);
  TEST_ASSERT_EQUAL_INT(CTL_BUTTON, channels[1].type);
  TEST_ASSERT_EQUAL_INT(CTL_MULTI, channels[2].type);

  free_config(channels);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_axis2str);
  RUN_TEST(test_button2str);
  RUN_TEST(test_load_valid_config);
  RUN_TEST(test_load_missing_config);

  // JSON structure error tests
  RUN_TEST(test_missing_channels_field);
  RUN_TEST(test_channels_not_array);
  RUN_TEST(test_empty_channels_array);
  RUN_TEST(test_channel_not_object);
  RUN_TEST(test_channel_missing_type);
  RUN_TEST(test_unknown_channel_type);

  // Axis channel error tests
  RUN_TEST(test_axis_missing_code);
  RUN_TEST(test_axis_invalid_code);

  // Button channel error tests
  RUN_TEST(test_button_missing_code);
  RUN_TEST(test_button_invalid_code);
  RUN_TEST(test_button_missing_threshold);

  // Multi channel error tests
  RUN_TEST(test_multi_missing_num_positions);
  RUN_TEST(test_multi_invalid_num_positions);
  RUN_TEST(test_multi_missing_thresholds);
  RUN_TEST(test_multi_wrong_threshold_count);
  RUN_TEST(test_multi_missing_codes);
  RUN_TEST(test_multi_wrong_codes_count);
  RUN_TEST(test_multi_invalid_code);

  // Valid configuration tests
  RUN_TEST(test_button_valid_config);
  RUN_TEST(test_multi_valid_config);
  RUN_TEST(test_multi_with_hysteresis);
  RUN_TEST(test_mixed_channel_types);

  return UNITY_END();
}
