// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import org.junit.Assert;

import org.chromium.chrome.browser.ntp.RecentTabsPage;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;

/**
 * Utilities for testing the RecentTabsPage.
 */
public class RecentTabsPageTestUtils {
    public static void waitForRecentTabsPageLoaded(final Tab tab) {
        CriteriaHelper.pollUiThread(new Criteria("RecentTabsPage never fully loaded") {
            @Override
            public boolean isSatisfied() {
                return tab.getNativePage() instanceof RecentTabsPage;
            }
        });
        Assert.assertTrue(tab.getNativePage() instanceof RecentTabsPage);
    }
}
