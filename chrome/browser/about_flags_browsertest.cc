// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/window_open_disposition.h"

namespace {

const char kSwitchName[] = "unsafely-treat-insecure-origin-as-secure";

// Command line switch containing an invalid origin.
const char kUnsanitizedCommandLine[] =
    "http://example-cmdline.test,invalid-cmdline";

// Command line switch without the invalid origin.
const char kSanitizedCommandLine[] = "http://example-cmdline.test";

// User input containing invalid origins.
const char kUnsanitizedInput[] =
    "http://example.test/path    http://example2.test/?query\n"
    "invalid-value, filesystem:http://example.test.file, "
    "ws://example3.test http://&^.com";

// User input with invalid origins removed and formatted.
const char kSanitizedInput[] =
    "http://example.test,http://example2.test,ws://example3.test";

// Command Line + user input with invalid origins removed and formatted.
const char kSanitizedInputAndCommandLine[] =
    "http://example-cmdline.test,http://example.test,http://"
    "example2.test,ws://example3.test";

void SimulateTextType(content::WebContents* contents,
                      const char* experiment_id,
                      const char* text) {
  EXPECT_TRUE(content::ExecuteScript(
      contents, base::StringPrintf(
                    "var parent = document.getElementById('%s');"
                    "var textarea = parent.getElementsByTagName('textarea')[0];"
                    "textarea.focus();"
                    "textarea.value = `%s`;"
                    "textarea.onchange();",
                    experiment_id, text)));
}

void ToggleEnableDropdown(content::WebContents* contents,
                          const char* experiment_id,
                          bool enable) {
  EXPECT_TRUE(content::ExecuteScript(
      contents,
      base::StringPrintf(
          "var k = document.getElementById('%s');"
          "var s = k.getElementsByClassName('experiment-enable-disable')[0];"
          "s.focus();"
          "s.selectedIndex = %d;"
          "s.onchange();",
          experiment_id, enable ? 1 : 0)));
}

std::string GetOriginListText(content::WebContents* contents,
                              const char* experiment_id) {
  std::string text;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      contents,
      base::StringPrintf(
          "var k = document.getElementById('%s');"
          "var s = k.getElementsByClassName('experiment-origin-list-value')[0];"
          "window.domAutomationController.send(s.value );",
          experiment_id),
      &text));
  return text;
}

bool IsDropdownEnabled(content::WebContents* contents,
                       const char* experiment_id) {
  bool result = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      contents,
      base::StringPrintf(
          "var k = document.getElementById('%s');"
          "var s = k.getElementsByClassName('experiment-enable-disable')[0];"
          "window.domAutomationController.send(s.value == 'enabled');",
          experiment_id),
      &result));
  return result;
}

// In these tests, valid origins in the existing command line flag will be
// appended to the list entered by the user in chrome://flags.
// The tests are run twice for each bool value: Once with an existing command
// line (provided in SetUpCommandLine) and once without.
class AboutFlagsBrowserTest : public InProcessBrowserTest,
                              public testing::WithParamInterface<bool> {
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(kSwitchName, GetInitialCommandLine());
  }

 protected:
  bool has_initial_command_line() const { return GetParam(); }

  std::string GetInitialCommandLine() const {
    return has_initial_command_line() ? kUnsanitizedCommandLine : std::string();
  }

  std::string GetSanitizedCommandLine() const {
    return has_initial_command_line() ? kSanitizedCommandLine : std::string();
  }

  std::string GetSanitizedInputAndCommandLine() const {
    return has_initial_command_line() ? kSanitizedInputAndCommandLine
                                      : kSanitizedInput;
  }
};

INSTANTIATE_TEST_SUITE_P(,
                         AboutFlagsBrowserTest,
                         ::testing::Values(true, false));

// Goes to chrome://flags page, types text into an ORIGIN_LIST_VALUE field but
// does not enable the feature.
IN_PROC_BROWSER_TEST_P(AboutFlagsBrowserTest, PRE_OriginFlagDisabled) {
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://flags"));

  const base::CommandLine::SwitchMap kInitialSwitches =
      base::CommandLine::ForCurrentProcess()->GetSwitches();

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // The page should be populated with the sanitized command line value.
  EXPECT_EQ(GetSanitizedCommandLine(),
            GetOriginListText(contents, kSwitchName));

  // Type a value in the experiment's textarea. Since the flag state is
  // "Disabled" by default, command line shouldn't change.
  SimulateTextType(contents, kSwitchName, kUnsanitizedInput);
  EXPECT_EQ(kInitialSwitches,
            base::CommandLine::ForCurrentProcess()->GetSwitches());

  // Input should be restored after a page reload.
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://flags"));
  EXPECT_EQ(GetSanitizedInputAndCommandLine(),
            GetOriginListText(contents, kSwitchName));
}

IN_PROC_BROWSER_TEST_P(AboutFlagsBrowserTest, OriginFlagDisabled) {
  // Even though the feature is disabled, the switch is set directly via command
  // line.
  EXPECT_EQ(
      GetInitialCommandLine(),
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(kSwitchName));

  ui_test_utils::NavigateToURL(browser(), GURL("chrome://flags"));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(IsDropdownEnabled(contents, kSwitchName));
  EXPECT_EQ(GetSanitizedInputAndCommandLine(),
            GetOriginListText(contents, kSwitchName));
}

// Goes to chrome://flags page, types text into an ORIGIN_LIST_VALUE field and
// enables the feature.
IN_PROC_BROWSER_TEST_P(AboutFlagsBrowserTest, PRE_OriginFlagEnabled) {
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://flags"));

  const base::CommandLine::SwitchMap kInitialSwitches =
      base::CommandLine::ForCurrentProcess()->GetSwitches();

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // The page should be populated with the sanitized command line value.
  EXPECT_EQ(GetSanitizedCommandLine(),
            GetOriginListText(contents, kSwitchName));

  // Type a value in the experiment's textarea. Since the flag state is
  // "Disabled" by default, command line shouldn't change.
  SimulateTextType(contents, kSwitchName, kUnsanitizedInput);
  EXPECT_EQ(kInitialSwitches,
            base::CommandLine::ForCurrentProcess()->GetSwitches());

  // Enable the experiment. The behavior is different between ChromeOS and
  // non-ChromeOS.
  ToggleEnableDropdown(contents, kSwitchName, true);

#if !defined(OS_CHROMEOS)
  // On non-ChromeOS, the command line is not modified until restart.
  EXPECT_EQ(kInitialSwitches,
            base::CommandLine::ForCurrentProcess()->GetSwitches());
#else
  // On ChromeOS, the command line is immediately modified.
  EXPECT_NE(kInitialSwitches,
            base::CommandLine::ForCurrentProcess()->GetSwitches());
  EXPECT_EQ(
      GetSanitizedInputAndCommandLine(),
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(kSwitchName));
#endif

  // Input should be restored after a page reload.
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://flags"));
  EXPECT_EQ(GetSanitizedInputAndCommandLine(),
            GetOriginListText(contents, kSwitchName));
}

IN_PROC_BROWSER_TEST_P(AboutFlagsBrowserTest, OriginFlagEnabled) {
#if !defined(OS_CHROMEOS)
  // On non-ChromeOS, the command line is modified after restart.
  EXPECT_EQ(
      GetSanitizedInputAndCommandLine(),
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(kSwitchName));
#else
  // On ChromeOS, the command line isn't modified after restart.
  EXPECT_EQ(
      GetInitialCommandLine(),
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(kSwitchName));
#endif

  ui_test_utils::NavigateToURL(browser(), GURL("chrome://flags"));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(IsDropdownEnabled(contents, kSwitchName));
  EXPECT_EQ(GetSanitizedInputAndCommandLine(),
            GetOriginListText(contents, kSwitchName));

#if defined(OS_CHROMEOS)
  // ChromeOS doesn't read chrome://flags values on startup so we explicitly
  // need to disable and re-enable the flag here.
  ToggleEnableDropdown(contents, kSwitchName, true);
#endif

  EXPECT_EQ(
      GetSanitizedInputAndCommandLine(),
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(kSwitchName));
}

}  // namespace
