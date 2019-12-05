// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import org.chromium.chrome.browser.contextmenu.ChromeContextMenuPopulator;
import org.chromium.chrome.browser.contextmenu.ContextMenuPopulator;
import org.chromium.chrome.browser.externalnav.ExternalNavigationHandler;
import org.chromium.chrome.browser.fullscreen.ComposedBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.tab.BrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabContextMenuItemDelegate;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.tab.TabWebContentsDelegateAndroid;
import org.chromium.chrome.browser.tab_activity_glue.ActivityTabWebContentsDelegateAndroid;
import org.chromium.chrome.browser.vr.VrModuleProvider;

/**
 * {@link TabDelegateFactory} class to be used in all {@link Tab} instances owned by a
 * {@link ChromeTabbedActivity}.
 */
public class TabbedModeTabDelegateFactory implements TabDelegateFactory {
    private static class TabbedModeBrowserControlsVisibilityDelegate
            extends TabStateBrowserControlsVisibilityDelegate {
        public TabbedModeBrowserControlsVisibilityDelegate(Tab tab) {
            super(tab);
        }

        @Override
        public boolean canShowBrowserControls() {
            if (VrModuleProvider.getDelegate().isInVr()) return false;
            return super.canShowBrowserControls();
        }

        @Override
        public boolean canAutoHideBrowserControls() {
            if (VrModuleProvider.getDelegate().isInVr()) return true;
            return super.canAutoHideBrowserControls();
        }
    }

    private final ChromeActivity mActivity;

    public TabbedModeTabDelegateFactory(ChromeActivity activity) {
        mActivity = activity;
    }

    @Override
    public TabWebContentsDelegateAndroid createWebContentsDelegate(Tab tab) {
        return new ActivityTabWebContentsDelegateAndroid(tab, mActivity);
    }

    @Override
    public ExternalNavigationHandler createExternalNavigationHandler(Tab tab) {
        return new ExternalNavigationHandler(tab);
    }

    @Override
    public ContextMenuPopulator createContextMenuPopulator(Tab tab) {
        return new ChromeContextMenuPopulator(new TabContextMenuItemDelegate(tab),
                mActivity.getShareDelegate(), ChromeContextMenuPopulator.ContextMenuMode.NORMAL);
    }

    @Override
    public BrowserControlsVisibilityDelegate createBrowserControlsVisibilityDelegate(Tab tab) {
        return new ComposedBrowserControlsVisibilityDelegate(
                new TabbedModeBrowserControlsVisibilityDelegate(tab),
                mActivity.getFullscreenManager().getBrowserVisibilityDelegate());
    }
}
