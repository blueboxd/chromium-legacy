// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TAB_STRIP_TAB_STRIP_UI_METRICS_H_
#define CHROME_BROWSER_UI_WEBUI_TAB_STRIP_TAB_STRIP_UI_METRICS_H_

namespace base {
class TimeDelta;
}  // namespace base

enum class TabStripUIOpenAction {
  kTapOnTabCounter = 0,
  kMaxValue = kTapOnTabCounter,
};

enum class TabStripUICloseAction {
  kTapOnTabCounter = 0,
  kTapOutsideTabStrip = 1,
  kTabSelected = 2,
  kMaxValue = kTabSelected,
};

void RecordTabStripUIOpenHistogram(TabStripUIOpenAction action);
void RecordTabStripUICloseHistogram(TabStripUICloseAction action);
void RecordTabStripUIOpenDurationHistogram(base::TimeDelta duration);

#endif  // CHROME_BROWSER_UI_WEBUI_TAB_STRIP_TAB_STRIP_UI_METRICS_H_
