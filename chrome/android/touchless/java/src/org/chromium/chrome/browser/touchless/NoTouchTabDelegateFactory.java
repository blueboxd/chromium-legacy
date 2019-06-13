// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import org.chromium.chrome.browser.tab.BrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.tab.TabDelegateFactory;

/**
 * TabDelegateFactory for all touchless activities.
 */
public class NoTouchTabDelegateFactory extends TabDelegateFactory {
    private final BrowserControlsVisibilityDelegate mDelegate;

    public NoTouchTabDelegateFactory(BrowserControlsVisibilityDelegate delegate) {
        mDelegate = delegate;
    }

    @Override
    public BrowserControlsVisibilityDelegate createBrowserControlsVisibilityDelegate(Tab tab) {
        return mDelegate;
    }
}
