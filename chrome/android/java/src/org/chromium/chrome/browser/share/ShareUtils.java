// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.embedder_support.util.UrlConstants;

/**
 *  A collection of helper functions for sharing in a non static context.
 */
public class ShareUtils {
    /*
     * Determines whether a tab is eligible to be shared.
     *
     * @param tab The tab being tested.
     */
    public boolean shouldEnableShare(@Nullable Tab tab) {
        if (tab == null) {
            return false;
        }

        String url = tab.getUrlString();
        boolean isChromeScheme = url.startsWith(UrlConstants.CHROME_URL_PREFIX)
                || url.startsWith(UrlConstants.CHROME_NATIVE_URL_PREFIX);
        boolean isDataScheme = url.startsWith(UrlConstants.DATA_SCHEME);
        boolean isShowingInterstitialPage =
                tab.getWebContents() != null && tab.getWebContents().isShowingInterstitialPage();

        return !isChromeScheme && !isDataScheme && !isShowingInterstitialPage;
    }
}
