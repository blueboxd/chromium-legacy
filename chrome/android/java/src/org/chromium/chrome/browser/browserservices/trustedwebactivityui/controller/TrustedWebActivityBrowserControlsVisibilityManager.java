// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarCoordinator;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabObserverRegistrar;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.content_public.common.BrowserControlsState;

import javax.inject.Inject;

/**
 * Updates the browser controls state based on whether the browser is in TWA mode and the page's
 * security level.
 */
public class TrustedWebActivityBrowserControlsVisibilityManager {
    private final TabObserverRegistrar mTabObserverRegistrar;
    private final CustomTabActivityTabProvider mTabProvider;
    private final CustomTabToolbarCoordinator mToolbarCoordinator;

    private boolean mInTwaMode;
    private @BrowserControlsState int mBrowserControlsState = BrowserControlsState.BOTH;

    private final TabObserver mTabObserver = new EmptyTabObserver() {
        @Override
        public void onSSLStateUpdated(Tab tab) {
            updateBrowserControlsState(tab);
        }
    };

    private final CustomTabActivityTabProvider.Observer mActivityTabObserver =
            new CustomTabActivityTabProvider.Observer() {
                @Override
                public void onTabSwapped(@NonNull Tab tab) {
                    updateBrowserControlsState(tab);
                }
            };

    @Inject
    public TrustedWebActivityBrowserControlsVisibilityManager(
            TabObserverRegistrar tabObserverRegistrar, CustomTabActivityTabProvider tabProvider,
            CustomTabToolbarCoordinator toolbarCoordinator) {
        mTabObserverRegistrar = tabObserverRegistrar;
        mTabProvider = tabProvider;
        mToolbarCoordinator = toolbarCoordinator;
    }

    /**
     * Should be called when the browser enters and exits TWA mode.
     */
    public void updateIsInTwaMode(boolean inTwaMode) {
        if (mInTwaMode == inTwaMode) return;

        mInTwaMode = inTwaMode;
        updateBrowserControlsState(mTabProvider.getTab());

        if (mInTwaMode) {
            mTabObserverRegistrar.registerTabObserver(mTabObserver);
            mTabProvider.addObserver(mActivityTabObserver);
        } else {
            mTabObserverRegistrar.unregisterTabObserver(mTabObserver);
            mTabProvider.removeObserver(mActivityTabObserver);
        }
    }

    private void updateBrowserControlsState(Tab tab) {
        @BrowserControlsState
        int newBrowserControlsState = computeBrowserControlsState(tab);
        if (mBrowserControlsState == newBrowserControlsState) return;

        mBrowserControlsState = newBrowserControlsState;
        mToolbarCoordinator.setBrowserControlsState(mBrowserControlsState);

        if (mBrowserControlsState == BrowserControlsState.BOTH) {
            // Force showing the controls for a bit when leaving Trusted Web Activity
            // mode.
            mToolbarCoordinator.showToolbarTemporarily();
        }
    }

    private @BrowserControlsState int computeBrowserControlsState(Tab tab) {
        // TODO(pkotwicz): Add check for PWA minimal UI display mode.
        if (tab != null && tab.getSecurityLevel() == ConnectionSecurityLevel.DANGEROUS) {
            return BrowserControlsState.SHOWN;
        }

        return mInTwaMode ? BrowserControlsState.HIDDEN : BrowserControlsState.BOTH;
    }
}
