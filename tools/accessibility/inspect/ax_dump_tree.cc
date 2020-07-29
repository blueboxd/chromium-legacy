// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "tools/accessibility/inspect/ax_tree_server.h"

char kWindowSwitch[] = "window";
char kPatternSwitch[] = "pattern";
char kFiltersSwitch[] = "filters";
char kJsonSwitch[] = "json";

// Convert from string to int, whether in 0x hex format or decimal format.
bool StringToInt(std::string str, unsigned* result) {
  if (str.empty())
    return false;
  bool is_hex =
      str.size() > 2 && str[0] == '0' && (str[1] == 'x' || str[1] == 'X');
  return is_hex ? base::HexStringToUInt(str, result)
                : base::StringToUint(str, result);
}

bool AXDumpTreeLogMessageHandler(int severity,
                                 const char* file,
                                 int line,
                                 size_t message_start,
                                 const std::string& str) {
  printf("%s", str.substr(message_start).c_str());
  return true;
}

gfx::AcceleratedWidget CastToAcceleratedWidget(unsigned window_id) {
#if defined(USE_OZONE) || defined(USE_X11) || defined(OS_MAC)
  return static_cast<gfx::AcceleratedWidget>(window_id);
#else
  return reinterpret_cast<gfx::AcceleratedWidget>(window_id);
#endif
}

int main(int argc, char** argv) {
  logging::SetLogMessageHandler(AXDumpTreeLogMessageHandler);

  base::AtExitManager at_exit_manager;

  base::CommandLine::Init(argc, argv);

  base::FilePath filters_path =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          kFiltersSwitch);

  bool use_json =
      base::CommandLine::ForCurrentProcess()->HasSwitch(kJsonSwitch);

  std::string window_str =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          kWindowSwitch);
  if (!window_str.empty()) {
    unsigned window_id;
    if (!StringToInt(window_str, &window_id)) {
      LOG(ERROR) << "* Error: Could not convert window id string to integer.";
      return 1;
    }
    gfx::AcceleratedWidget widget(CastToAcceleratedWidget(window_id));

    std::unique_ptr<content::AXTreeServer> server(
        new content::AXTreeServer(widget, filters_path, use_json));
    return 0;
  }

  std::string pattern_str =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          kPatternSwitch);
  if (!pattern_str.empty()) {
    std::unique_ptr<content::AXTreeServer> server(
        new content::AXTreeServer(pattern_str, filters_path, use_json));
    return 0;
  }

  LOG(ERROR) << "* Error: Neither window handle (--window=[window-handle]) "
                "nor pattern (--pattern=[pattern]) provided.";
  return 1;
}
