// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.toolbar;

import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.fullscreen.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.components.browser_ui.util.BrowserControlsVisibilityDelegate;
import org.chromium.content_public.common.BrowserControlsState;

import javax.inject.Inject;

import dagger.Lazy;

/**
 * Implementation of {@link BrowserControlsVisibilityDelegate} for custom tabs.
 */
@ActivityScope
public class CustomTabBrowserControlsVisibilityDelegate extends BrowserControlsVisibilityDelegate {
    private final Lazy<ChromeFullscreenManager> mFullscreenManagerDelegate;
    private final ActivityTabProvider mTabProvider;
    private @BrowserControlsState int mBrowserControlsState = BrowserControlsState.BOTH;

    @Inject
    public CustomTabBrowserControlsVisibilityDelegate(
            Lazy<ChromeFullscreenManager> fullscreenManager, ActivityTabProvider tabProvider) {
        super(BrowserControlsState.BOTH);
        mFullscreenManagerDelegate = fullscreenManager;
        mTabProvider = tabProvider;
        getDefaultVisibilityDelegate().addObserver((constraints) -> updateVisibilityConstraints());
        updateVisibilityConstraints();
    }

    /**
     * Sets the browser controls state. Note: this is not enough to completely hide the toolbar, use
     * {@link CustomTabToolbarCoordinator#setBrowserControlsState()} for that.
     */
    public void setControlsState(@BrowserControlsState int browserControlsState) {
        if (browserControlsState == mBrowserControlsState) return;
        mBrowserControlsState = browserControlsState;
        updateVisibilityConstraints();
    }

    @BrowserControlsState
    private int calculateVisibilityConstraints() {
        @BrowserControlsState
        int defaultConstraints = getDefaultVisibilityDelegate().get();
        if (defaultConstraints == BrowserControlsState.HIDDEN
                || mBrowserControlsState == BrowserControlsState.HIDDEN) {
            return BrowserControlsState.HIDDEN;
        } else if (mBrowserControlsState != BrowserControlsState.BOTH) {
            return mBrowserControlsState;
        }
        return defaultConstraints;
    }

    private void updateVisibilityConstraints() {
        set(calculateVisibilityConstraints());
    }

    private BrowserStateBrowserControlsVisibilityDelegate getDefaultVisibilityDelegate() {
        return mFullscreenManagerDelegate.get().getBrowserVisibilityDelegate();
    }
}
