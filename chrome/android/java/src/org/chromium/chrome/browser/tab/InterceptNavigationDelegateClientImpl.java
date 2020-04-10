// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import org.chromium.components.external_intents.InterceptNavigationDelegateClient;
import org.chromium.content_public.browser.WebContents;

/**
 * Class that provides embedder-level information to InterceptNavigationDelegateImpl based off a
 * Tab.
 */
public class InterceptNavigationDelegateClientImpl implements InterceptNavigationDelegateClient {
    private TabImpl mTab;

    InterceptNavigationDelegateClientImpl(Tab tab) {
        mTab = (TabImpl) tab;
    }

    @Override
    public WebContents getWebContents() {
        return mTab.getWebContents();
    }
}
