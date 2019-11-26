// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui_metrics.h"

#include "base/metrics/histogram_macros.h"

void RecordTabStripUIOpenHistogram(TabStripUIOpenAction action) {
  UMA_HISTOGRAM_ENUMERATION("WebUITabStrip.OpenAction", action);
}

void RecordTabStripUICloseHistogram(TabStripUICloseAction action) {
  UMA_HISTOGRAM_ENUMERATION("WebUITabStrip.CloseAction", action);
}
