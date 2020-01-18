// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.contextmenu.ChromeContextMenuPopulator;
import org.chromium.chrome.browser.contextmenu.ContextMenuPopulator;
import org.chromium.chrome.browser.externalnav.ExternalNavigationHandler;
import org.chromium.chrome.browser.fullscreen.ComposedBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.BrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabContextMenuItemDelegate;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.tab.TabWebContentsDelegateAndroid;
import org.chromium.chrome.browser.tab_activity_glue.ActivityTabWebContentsDelegateAndroid;

/**
 * {@link TabDelegateFactory} class to be used in all {@link Tab} instances owned by a
 * {@link ChromeTabbedActivity}.
 */
public class TabbedModeTabDelegateFactory implements TabDelegateFactory {
    private final ChromeActivity mActivity;
    private final BrowserControlsVisibilityDelegate mAppBrowserControlsVisibilityDelegate;
    private final Supplier<ShareDelegate> mShareDelegateSupplier;

    public TabbedModeTabDelegateFactory(ChromeActivity activity,
            BrowserControlsVisibilityDelegate appBrowserControlsVisibilityDelegate,
            Supplier<ShareDelegate> shareDelegateSupplier) {
        mActivity = activity;
        mAppBrowserControlsVisibilityDelegate = appBrowserControlsVisibilityDelegate;
        mShareDelegateSupplier = shareDelegateSupplier;
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
                mShareDelegateSupplier, ChromeContextMenuPopulator.ContextMenuMode.NORMAL);
    }

    @Override
    public BrowserControlsVisibilityDelegate createBrowserControlsVisibilityDelegate(Tab tab) {
        return new ComposedBrowserControlsVisibilityDelegate(
                new TabStateBrowserControlsVisibilityDelegate(tab),
                mAppBrowserControlsVisibilityDelegate);
    }
}
