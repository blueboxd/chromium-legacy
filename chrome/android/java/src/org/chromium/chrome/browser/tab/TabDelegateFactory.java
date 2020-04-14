// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.contextmenu.ContextMenuPopulator;
import org.chromium.chrome.browser.native_page.NativePageFactory;
import org.chromium.components.browser_ui.util.BrowserControlsVisibilityDelegate;
import org.chromium.components.embedder_support.delegate.WebContentsDelegateAndroid;
import org.chromium.components.external_intents.ExternalNavigationHandler;
import org.chromium.components.navigation_interception.InterceptNavigationDelegate;

/**
 * An interface for factory to create {@link Tab} related delegates.
 */
public interface TabDelegateFactory {
    /**
     * Creates the {@link WebContentsDelegateAndroid} the tab will be initialized with.
     * @param tab The associated {@link Tab}.
     * @return The {@link WebContentsDelegateAndroid} to be used for this tab.
     */
    TabWebContentsDelegateAndroid createWebContentsDelegate(Tab tab);

    /**
     * Creates the {@link ExternalNavigationHandler} the tab will use for its
     * {@link InterceptNavigationDelegate}.
     * @param tab The associated {@link Tab}.
     * @return The {@link ExternalNavigationHandler} to be used for this tab.
     */
    ExternalNavigationHandler createExternalNavigationHandler(Tab tab);

    /**
     * Creates the {@link ContextMenuPopulator} the tab will be initialized with.
     * @param tab The associated {@link Tab}.
     * @return The {@link ContextMenuPopulator} to be used for this tab.
     */
    ContextMenuPopulator createContextMenuPopulator(Tab tab);

    /**
     * Creates the {@link BrowserControlsVisibilityDelegate} the tab will be initialized with.
     * @param tab The associated {@link Tab}.
     */
    BrowserControlsVisibilityDelegate createBrowserControlsVisibilityDelegate(Tab tab);

    /**
     * @return {@link NativePageFactory} providing a method that creates a new native page.
     */
    @Nullable
    NativePageFactory getNativePageFactory();
}
