// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSION_BUBBLE_FRONT_ELIDING_TITLE_LABEL_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSION_BUBBLE_FRONT_ELIDING_TITLE_LABEL_H_

#include <memory>

#include "ui/views/controls/label.h"

// Constructs a custom title label for permission and chooser bubbles that takes
// care of eliding the origin from the left, and configures itself to be
// ignored by screen readers (since the bubbles handle the context).
std::unique_ptr<views::Label> ConstructFrontElidingTitleLabel(
    const base::string16& text);

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSION_BUBBLE_FRONT_ELIDING_TITLE_LABEL_H_
