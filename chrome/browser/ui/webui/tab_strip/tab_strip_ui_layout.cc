// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui_layout.h"

#include "base/values.h"
#include "ui/gfx/geometry/size.h"

// static
TabStripUILayout TabStripUILayout::CalculateForWebViewportSize(
    const gfx::Size& viewport_size) {
  // The smaller of the thumbnail's height or width is fixed to this
  // value. The other dimension will be at least this long.
  constexpr int kThumbnailMinDimensionLength = 176;

  TabStripUILayout layout;
  layout.padding_around_tab_list = 16;
  layout.tab_title_height = 40;

  if (viewport_size.IsEmpty()) {
    layout.tab_thumbnail_size =
        gfx::Size(kThumbnailMinDimensionLength, kThumbnailMinDimensionLength);
    return layout;
  }

  // Size the thumbnail to match the web viewport's aspect ratio.
  if (viewport_size.width() > viewport_size.height()) {
    layout.tab_thumbnail_size.set_height(kThumbnailMinDimensionLength);
    layout.tab_thumbnail_size.set_width(kThumbnailMinDimensionLength *
                                        viewport_size.width() /
                                        viewport_size.height());
  } else {
    layout.tab_thumbnail_size.set_width(kThumbnailMinDimensionLength);
    layout.tab_thumbnail_size.set_height(kThumbnailMinDimensionLength *
                                         viewport_size.height() /
                                         viewport_size.width());
  }

  return layout;
}

base::Value TabStripUILayout::AsDictionary() const {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetIntKey("--tabstrip-tab-list-padding", padding_around_tab_list);
  dict.SetIntKey("--tabstrip-tab-title-height", tab_title_height);
  dict.SetIntKey("--tabstrip-tab-thumbnail-width", tab_thumbnail_size.width());
  dict.SetIntKey("--tabstrip-tab-thumbnail-height",
                 tab_thumbnail_size.height());
  return dict;
}

int TabStripUILayout::CalculateContainerHeight() const {
  return 2 * padding_around_tab_list + tab_title_height +
         tab_thumbnail_size.height();
}
