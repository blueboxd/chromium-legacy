// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.tab.BrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_activity_glue.TabDelegateFactoryImpl;

/**
 * TabDelegateFactory for all touchless activities.
 */
public class NoTouchTabDelegateFactory extends TabDelegateFactoryImpl {
    private final BrowserControlsVisibilityDelegate mDelegate;

    public NoTouchTabDelegateFactory(
            ChromeActivity activity, BrowserControlsVisibilityDelegate delegate) {
        super(activity);
        mDelegate = delegate;
    }

    @Override
    public BrowserControlsVisibilityDelegate createBrowserControlsVisibilityDelegate(Tab tab) {
        return mDelegate;
    }
}
