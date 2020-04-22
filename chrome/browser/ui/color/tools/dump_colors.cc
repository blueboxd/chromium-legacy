// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This command-line program dumps the computed values of all color IDs to
// stdout.

#include <iomanip>
#include <ios>
#include <iostream>

#include "build/build_config.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/color/chrome_color_mixers.h"
#include "chrome/browser/ui/color/omnibox_color_mixers.h"
#include "ui/color/color_mixers.h"
#include "ui/color/color_provider.h"

#define STRINGIZE_COLOR_IDS
#include "ui/color/color_id_macros.inc"

// clang-format off
const char* enum_names[] = {
  COLOR_IDS
  CHROME_COLOR_IDS
};
// clang-format on

#include "ui/color/color_id_macros.inc"

int main(int argc, const char* argv[]) {
  const auto add_mixers = [](ui::ColorProvider* provider, bool dark_window) {
    // TODO(pkasting): Use standard provider setup functions once those exist.
    ui::AddCoreDefaultColorMixer(provider, dark_window);
    ui::AddNativeCoreColorMixer(provider, dark_window);
    ui::AddUiColorMixer(provider);
    ui::AddNativeUiColorMixer(provider, dark_window);
    AddChromeColorMixers(provider);
    AddOmniboxColorMixers(provider, false);
  };
  ui::ColorProvider light_provider, dark_provider;
  add_mixers(&light_provider, false);
  add_mixers(&dark_provider, true);

  size_t longest_name = 0;
  for (const char* name : enum_names)
    longest_name = std::max(longest_name, strlen(name));
  ++longest_name;  // For trailing space.

  std::cout << std::setfill(' ') << std::left;
  std::cout << std::setw(longest_name) << "ID";
  constexpr size_t kColorColumnWidth = 1 + 8 + 1;  // '#xxxxxxxx '/'#xxxxxxxx\n'
  std::cout << std::setw(kColorColumnWidth) << "Light";
  std::cout << "Dark\n";
  std::cout << std::setfill('-') << std::right;
  std::cout << std::setw(longest_name) << ' ';
  std::cout << std::setw(kColorColumnWidth) << ' ';
  std::cout << std::setw(kColorColumnWidth) << '\n';

  for (ui::ColorId id = ui::kUiColorsStart; id < kChromeColorsEnd; ++id) {
    std::cout << std::setfill(' ') << std::left;
    std::cout << std::setw(longest_name) << enum_names[id];
    std::cout << std::hex << std::setfill('0') << std::right;
    std::cout << '#' << std::setw(8) << light_provider.GetColor(id) << ' ';
    std::cout << '#' << std::setw(8) << dark_provider.GetColor(id) << '\n';
  }

  std::cout.flush();

  return 0;
}
