#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>

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
  snprintf(cfgFile, sizeof(cfgFile) - 1, "%s/config.json", workdir);
  writeFile(cfgFile,
            "{\"channels\":[{\"type\":\"axis\",\"code\":\"ABS_X\"},{\"type\":"
            "\"axis\",\"code\":\"ABS_Y\"},{\"type\":\"axis\",\"code\":\"ABS_"
            "RX\"},{\"type\":\"axis\",\"code\":\"ABS_RY\"}]}");

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

  channels = load_config("tests/data/does-not-exist", &num_channels);
  TEST_ASSERT_NULL(channels);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_axis2str);
  RUN_TEST(test_button2str);
  RUN_TEST(test_load_valid_config);
  RUN_TEST(test_load_missing_config);
  return UNITY_END();
}
