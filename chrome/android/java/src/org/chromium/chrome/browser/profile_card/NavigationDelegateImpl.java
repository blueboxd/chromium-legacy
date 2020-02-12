// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profile_card;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.PageTransition;

/**
 * Base implementation of {@link NavigationDelegate} that handles navigating to a new page on the
 * current tab.
 */
public class NavigationDelegateImpl implements NavigationDelegate {
    private Tab mCurrentTab;

    NavigationDelegateImpl(Tab currentTab) {
        mCurrentTab = currentTab;
    }

    @Override
    public void navigateTo(String url) {
        if (url.isEmpty()) return;
        mCurrentTab.loadUrl(new LoadUrlParams(url, PageTransition.LINK));
    }
}