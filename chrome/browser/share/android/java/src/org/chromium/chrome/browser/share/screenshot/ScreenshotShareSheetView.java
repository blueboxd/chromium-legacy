// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.screenshot;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.FrameLayout;

/**
 * Manages the Android View representing the Screenshot share panel.
 */
class ScreenshotShareSheetView extends FrameLayout {
    /** Constructor for use from XML. */
    public ScreenshotShareSheetView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }
}
