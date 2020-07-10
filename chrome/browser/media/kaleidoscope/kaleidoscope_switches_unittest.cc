// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/kaleidoscope/kaleidoscope_switches.h"

#include "base/command_line.h"
#include "base/test/scoped_command_line.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

const char kDefaultUrl[] =
    "https://chromemediarecommendations-pa.googleapis.com";

}  // namespace

using KaleidoscopeSwitchesTest = testing::Test;

TEST_F(KaleidoscopeSwitchesTest, Switches_Default) {
  GURL expected(kDefaultUrl);

  EXPECT_EQ(expected,
            GetGoogleAPIBaseURL(*base::CommandLine::ForCurrentProcess()));
}

TEST_F(KaleidoscopeSwitchesTest, Switches_Prod) {
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchASCII(switches::kKaleidoscopeBackendUrl,
                                  switches::kKaleidoscopeBackendUrlProdAlias);

  GURL expected("https://chromemediarecommendations-pa.googleapis.com");
  EXPECT_EQ(expected, GetGoogleAPIBaseURL(*command_line));
}

TEST_F(KaleidoscopeSwitchesTest, Switches_Staging) {
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchASCII(
      switches::kKaleidoscopeBackendUrl,
      switches::kKaleidoscopeBackendUrlStagingAlias);

  GURL expected(
      "https://staging-chromemediarecommendations-pa.sandbox.googleapis.com");
  EXPECT_EQ(expected, GetGoogleAPIBaseURL(*command_line));
}

TEST_F(KaleidoscopeSwitchesTest, Switches_Autopush) {
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchASCII(
      switches::kKaleidoscopeBackendUrl,
      switches::kKaleidoscopeBackendUrlAutopushAlias);

  GURL expected(
      "https://autopush-chromemediarecommendations-pa.sandbox.googleapis.com");
  EXPECT_EQ(expected, GetGoogleAPIBaseURL(*command_line));
}

TEST_F(KaleidoscopeSwitchesTest, Switches_Bad) {
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchASCII(switches::kKaleidoscopeBackendUrl, "xxx");

  GURL expected(kDefaultUrl);
  EXPECT_EQ(expected, GetGoogleAPIBaseURL(*command_line));
}

TEST_F(KaleidoscopeSwitchesTest, Switches_CustomURL) {
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchASCII(switches::kKaleidoscopeBackendUrl,
                                  "https://test.sandbox.googleapis.com");

  GURL expected("https://test.sandbox.googleapis.com");
  EXPECT_EQ(expected, GetGoogleAPIBaseURL(*command_line));
}

TEST_F(KaleidoscopeSwitchesTest, Switches_CustomURL_WithPath) {
  base::test::ScopedCommandLine scoped_command_line;
  base::CommandLine* command_line = scoped_command_line.GetProcessCommandLine();
  command_line->AppendSwitchASCII(switches::kKaleidoscopeBackendUrl,
                                  "https://test.sandbox.googleapis.com/v1");

  GURL expected(kDefaultUrl);
  EXPECT_EQ(expected, GetGoogleAPIBaseURL(*command_line));
}
