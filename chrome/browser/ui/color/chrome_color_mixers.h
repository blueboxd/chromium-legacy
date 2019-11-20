// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COLOR_CHROME_COLOR_MIXERS_H_
#define CHROME_BROWSER_UI_COLOR_CHROME_COLOR_MIXERS_H_

namespace ui {
class ColorProvider;
}

// Adds color mixers to |provider| that supply default values for various
// chrome/ colors before taking into account any custom themes.
void AddChromeColorMixers(ui::ColorProvider* provider);

#endif  // CHROME_BROWSER_UI_COLOR_CHROME_COLOR_MIXERS_H_
