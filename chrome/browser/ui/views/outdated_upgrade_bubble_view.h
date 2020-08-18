// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OUTDATED_UPGRADE_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_OUTDATED_UPGRADE_BUBBLE_VIEW_H_

#include "base/macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace content {
class PageNavigator;
}

// OutdatedUpgradeBubbleView warns the user that an upgrade is long overdue.
// It is intended to be used as the content of a bubble anchored off of the
// Chrome toolbar. Don't create an OutdatedUpgradeBubbleView directly,
// instead use the static ShowBubble method.
class OutdatedUpgradeBubbleView {
 public:
  static void ShowBubble(views::View* anchor_view,
                         content::PageNavigator* navigator,
                         bool auto_update_enabled);
};

#endif  // CHROME_BROWSER_UI_VIEWS_OUTDATED_UPGRADE_BUBBLE_VIEW_H_
