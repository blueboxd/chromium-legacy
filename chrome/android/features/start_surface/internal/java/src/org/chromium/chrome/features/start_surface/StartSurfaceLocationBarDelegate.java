// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.omnibox.LocationBar;
import org.chromium.chrome.browser.omnibox.UrlFocusChangeListener;
import org.chromium.chrome.browser.tasks.TasksSurface;

/** The delegate to interact with the {@link LocationBar}. */
class StartSurfaceLocationBarDelegate
        implements TasksSurface.FakeSearchBoxDelegate, StartSurfaceMediator.Delegate {
    private final LocationBar mLocationBar;

    StartSurfaceLocationBarDelegate(LocationBar locationBar) {
        mLocationBar = locationBar;
    }

    // Implements TasksSurface.FakeSearchBoxDelegate.
    @Override
    public void requestUrlFocus(@Nullable String pastedText) {
        mLocationBar.setUrlBarFocus(true);
    }

    // Implements StartSurfaceMediator.Delegate.
    @Override
    public void addUrlFocusChangeListener(UrlFocusChangeListener listener) {
        mLocationBar.addUrlFocusChangeListener(listener);
    }

    @Override
    public void removeUrlFocusChangeListener(UrlFocusChangeListener listener) {
        mLocationBar.removeUrlFocusChangeListener(listener);
    }
}
