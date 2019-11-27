// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet.ui;

import android.annotation.TargetApi;
import android.os.Build;
import android.view.ViewGroup;

/**
 * Rounding delegate for the outline provider strategy.
 *
 * <p>Adds logic to clip rounded corner views to their outline. {@link RoundedCornerWrapperView}
 * decides which rounding strategy to use and sets the appropriate delegate.
 */
@TargetApi(Build.VERSION_CODES.LOLLIPOP)
class OutlineProviderRoundedCornerDelegate extends RoundedCornerDelegate {
    /**
     * Sets clipToOutline to true.
     *
     * <p>Setting it in an initializer rather than a constructor means a new instance of this
     * delegate doesn't need to be created every time an outline provider is needed.
     */
    @Override
    public void initializeForView(ViewGroup view) {
        view.setClipToOutline(true);
        view.setClipChildren(true);
    }

    /** Reset clipToOutline to false (the default) when this strategy stops being used. */
    @Override
    public void destroy(ViewGroup view) {
        view.setClipToOutline(false);
        view.setClipChildren(false);
    }
}
