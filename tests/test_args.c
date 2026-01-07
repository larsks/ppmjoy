#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "args.h"
#include "unity.h"

// Test helper: reset getopt state between tests
static void reset_getopt(void) {
  optind = 1; // Reset to start of argv
  opterr = 1; // Reset error printing
  optopt = 0; // Reset error character
}

// Test helper: mock show_usage that doesn't actually print
static int usage_called = 0;
static FILE *usage_file = NULL;
static void mock_show_usage(FILE *out) {
  usage_called = 1;
  usage_file = out;
}

void setUp(void) {
  reset_getopt();
  usage_called = 0;
  usage_file = NULL;

  // Clear environment variables to avoid interference
  unsetenv("PPMJOY_ALSA_DEVICE");
  unsetenv("PPMJOY_CONFIG");
}

void tearDown(void) {
  // Nothing needed
}

// Test: Default values when no arguments provided
void test_defaults(void) {
  char *argv[] = {"ppmjoy"};
  int argc = 1;
  app_config_t config;

  args_result_t result = parse_arguments(argc, argv, &config, mock_show_usage);

  TEST_ASSERT_EQUAL_INT(ARGS_OK, result);
  TEST_ASSERT_EQUAL_STRING(DEFAULT_ALSA_DEVICE, config.alsa_device);
  TEST_ASSERT_EQUAL_STRING(DEFAULT_CONFIG_PATH, config.config_path);
  TEST_ASSERT_EQUAL_INT(0, config.verbose);
  TEST_ASSERT_EQUAL_INT(0, config.monitor);
  TEST_ASSERT_EQUAL_INT(0, usage_called);
}

// Test: Short option -d for device
void test_device_short_option(void) {
  char *argv[] = {"ppmjoy", "-d", "hw:1,0"};
  int argc = 3;
  app_config_t config;

  args_result_t result = parse_arguments(argc, argv, &config, mock_show_usage);

  TEST_ASSERT_EQUAL_INT(ARGS_OK, result);
  TEST_ASSERT_EQUAL_STRING("hw:1,0", config.alsa_device);
}

// Test: Long option --device for device
void test_device_long_option(void) {
  char *argv[] = {"ppmjoy", "--device", "plughw:2,0"};
  int argc = 3;
  app_config_t config;

  args_result_t result = parse_arguments(argc, argv, &config, mock_show_usage);

  TEST_ASSERT_EQUAL_INT(ARGS_OK, result);
  TEST_ASSERT_EQUAL_STRING("plughw:2,0", config.alsa_device);
}

// Test: Short option -f for config file
void test_config_short_option(void) {
  char *argv[] = {"ppmjoy", "-f", "/etc/ppmjoy.json"};
  int argc = 3;
  app_config_t config;

  args_result_t result = parse_arguments(argc, argv, &config, mock_show_usage);

  TEST_ASSERT_EQUAL_INT(ARGS_OK, result);
  TEST_ASSERT_EQUAL_STRING("/etc/ppmjoy.json", config.config_path);
}

// Test: Long option --config for config file
void test_config_long_option(void) {
  char *argv[] = {"ppmjoy", "--config", "myconfig.json"};
  int argc = 3;
  app_config_t config;

  args_result_t result = parse_arguments(argc, argv, &config, mock_show_usage);

  TEST_ASSERT_EQUAL_INT(ARGS_OK, result);
  TEST_ASSERT_EQUAL_STRING("myconfig.json", config.config_path);
}

// Test: Verbose flag increments counter
void test_verbose_flag(void) {
  char *argv[] = {"ppmjoy", "-v"};
  int argc = 2;
  app_config_t config;

  args_result_t result = parse_arguments(argc, argv, &config, mock_show_usage);

  TEST_ASSERT_EQUAL_INT(ARGS_OK, result);
  TEST_ASSERT_EQUAL_INT(1, config.verbose);
}

// Test: Multiple verbose flags accumulate
void test_multiple_verbose_flags(void) {
  char *argv[] = {"ppmjoy", "-v", "-v", "-v"};
  int argc = 4;
  app_config_t config;

  args_result_t result = parse_arguments(argc, argv, &config, mock_show_usage);

  TEST_ASSERT_EQUAL_INT(ARGS_OK, result);
  TEST_ASSERT_EQUAL_INT(3, config.verbose);
}

// Test: Monitor flag
void test_monitor_flag(void) {
  char *argv[] = {"ppmjoy", "-m"};
  int argc = 2;
  app_config_t config;

  args_result_t result = parse_arguments(argc, argv, &config, mock_show_usage);

  TEST_ASSERT_EQUAL_INT(ARGS_OK, result);
  TEST_ASSERT_EQUAL_INT(1, config.monitor);
}

// Test: Combined short options
void test_combined_options(void) {
  char *argv[] = {"ppmjoy", "-vm", "-d", "hw:0,0"};
  int argc = 4;
  app_config_t config;

  args_result_t result = parse_arguments(argc, argv, &config, mock_show_usage);

  TEST_ASSERT_EQUAL_INT(ARGS_OK, result);
  TEST_ASSERT_EQUAL_INT(1, config.verbose);
  TEST_ASSERT_EQUAL_INT(1, config.monitor);
  TEST_ASSERT_EQUAL_STRING("hw:0,0", config.alsa_device);
}

// Test: Help short option
void test_help_short_option(void) {
  char *argv[] = {"ppmjoy", "-h"};
  int argc = 2;
  app_config_t config;

  args_result_t result = parse_arguments(argc, argv, &config, mock_show_usage);

  TEST_ASSERT_EQUAL_INT(ARGS_HELP_REQUESTED, result);
  TEST_ASSERT_EQUAL_INT(1, usage_called);
  TEST_ASSERT_EQUAL_PTR(stdout, usage_file);
}

// Test: Help long option
void test_help_long_option(void) {
  char *argv[] = {"ppmjoy", "--help"};
  int argc = 2;
  app_config_t config;

  args_result_t result = parse_arguments(argc, argv, &config, mock_show_usage);

  TEST_ASSERT_EQUAL_INT(ARGS_HELP_REQUESTED, result);
  TEST_ASSERT_EQUAL_INT(1, usage_called);
  TEST_ASSERT_EQUAL_PTR(stdout, usage_file);
}

// Test: Invalid option returns error
void test_invalid_option(void) {
  char *argv[] = {"ppmjoy", "-x"};
  int argc = 2;
  app_config_t config;

  args_result_t result = parse_arguments(argc, argv, &config, mock_show_usage);

  TEST_ASSERT_EQUAL_INT(ARGS_ERROR, result);
  TEST_ASSERT_EQUAL_INT(1, usage_called);
  TEST_ASSERT_EQUAL_PTR(stderr, usage_file);
}

// Test: Environment variable PPMJOY_ALSA_DEVICE
void test_env_alsa_device(void) {
  setenv("PPMJOY_ALSA_DEVICE", "env_device", 1);

  char *argv[] = {"ppmjoy"};
  int argc = 1;
  app_config_t config;

  args_result_t result = parse_arguments(argc, argv, &config, mock_show_usage);

  TEST_ASSERT_EQUAL_INT(ARGS_OK, result);
  TEST_ASSERT_EQUAL_STRING("env_device", config.alsa_device);

  unsetenv("PPMJOY_ALSA_DEVICE");
}

// Test: Command line overrides environment variable
void test_cmdline_overrides_env(void) {
  setenv("PPMJOY_ALSA_DEVICE", "env_device", 1);

  char *argv[] = {"ppmjoy", "-d", "cmdline_device"};
  int argc = 3;
  app_config_t config;

  args_result_t result = parse_arguments(argc, argv, &config, mock_show_usage);

  TEST_ASSERT_EQUAL_INT(ARGS_OK, result);
  TEST_ASSERT_EQUAL_STRING("cmdline_device", config.alsa_device);

  unsetenv("PPMJOY_ALSA_DEVICE");
}

// Test: Empty environment variable uses default
void test_empty_env_uses_default(void) {
  setenv("PPMJOY_ALSA_DEVICE", "", 1);

  char *argv[] = {"ppmjoy"};
  int argc = 1;
  app_config_t config;

  args_result_t result = parse_arguments(argc, argv, &config, mock_show_usage);

  TEST_ASSERT_EQUAL_INT(ARGS_OK, result);
  TEST_ASSERT_EQUAL_STRING(DEFAULT_ALSA_DEVICE, config.alsa_device);

  unsetenv("PPMJOY_ALSA_DEVICE");
}

// Test: Environment variable PPMJOY_CONFIG
void test_env_config_path(void) {
  setenv("PPMJOY_CONFIG", "/custom/config.json", 1);

  char *argv[] = {"ppmjoy"};
  int argc = 1;
  app_config_t config;

  args_result_t result = parse_arguments(argc, argv, &config, mock_show_usage);

  TEST_ASSERT_EQUAL_INT(ARGS_OK, result);
  TEST_ASSERT_EQUAL_STRING("/custom/config.json", config.config_path);

  unsetenv("PPMJOY_CONFIG");
}

int main(void) {
  UNITY_BEGIN();

  RUN_TEST(test_defaults);
  RUN_TEST(test_device_short_option);
  RUN_TEST(test_device_long_option);
  RUN_TEST(test_config_short_option);
  RUN_TEST(test_config_long_option);
  RUN_TEST(test_verbose_flag);
  RUN_TEST(test_multiple_verbose_flags);
  RUN_TEST(test_monitor_flag);
  RUN_TEST(test_combined_options);
  RUN_TEST(test_help_short_option);
  RUN_TEST(test_help_long_option);
  RUN_TEST(test_invalid_option);
  RUN_TEST(test_env_alsa_device);
  RUN_TEST(test_cmdline_overrides_env);
  RUN_TEST(test_empty_env_uses_default);
  RUN_TEST(test_env_config_path);

  return UNITY_END();
}
