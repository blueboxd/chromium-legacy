// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_SEARCH_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_SEARCH_BUTTON_H_

#include "chrome/browser/ui/views/tabs/new_tab_button.h"

namespace gfx {
class Canvas;
}

namespace views {
class ButtonListener;
}

class TabStrip;

// TabSearchButton should leverage the look and feel of the existing
// NewTabButton for sizing and appropriate theming. This class updates the
// NewTabButton with the appropriate icon and will be used to anchor the
// Tab Search bubble.
//
// TODO(tluk): Break away common code from the NewTabButton and the
// TabSearchButton into a TabStripControlButton or similar.
class TabSearchButton : public NewTabButton {
 public:
  TabSearchButton(TabStrip* tab_strip, views::ButtonListener* listener);
  TabSearchButton(const TabSearchButton&) = delete;
  TabSearchButton& operator=(const TabSearchButton&) = delete;
  ~TabSearchButton() override = default;

 protected:
  // NewTabButton:
  void PaintIcon(gfx::Canvas* canvas) override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_SEARCH_BUTTON_H_
