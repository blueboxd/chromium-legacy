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

const char* enum_names[] = {
  COLOR_IDS,
  CHROME_COLOR_IDS,
};

#include "ui/color/color_id_macros.inc"

int main(int argc, const char* argv[]) {
  const auto add_mixers = [](ui::ColorProvider* provider, bool dark_window) {
    // TODO(pkasting): Use standard provider setup functions once those exist.
    ui::AddCoreDefaultColorMixers(provider, dark_window);
    ui::AddNativeColorMixers(provider);
    ui::AddUiColorMixers(provider);
    AddChromeColorMixers(provider);
    AddOmniboxColorMixers(provider, false);
  };
  ui::ColorProvider light_provider, dark_provider;
  add_mixers(&light_provider, false);
  add_mixers(&dark_provider, true);

  for (ui::ColorId id = ui::kUiColorsStart; id < kChromeColorsEnd; ++id) {
    std::cout << "ID: " << std::setfill(' ') << std::setw(49) << enum_names[id]
              << " Light: "<< std::hex << std::setfill('0') << std::setw(8)
              << light_provider.GetColor(id)
              << " Dark: " << std::setw(8) << dark_provider.GetColor(id)
              << '\n';
  }
  std::cout.flush();

  return 0;
}
