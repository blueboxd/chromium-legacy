// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.content.ComponentName;
import android.content.Intent;
import android.net.Uri;
import android.support.test.InstrumentationRegistry;
import android.support.test.rule.ActivityTestRule;

import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.NavigationController;
import org.chromium.weblayer.shell.WebLayerShellActivity;

/**
 * ActivityTestRule for WebLayerShellActivity.
 *
 * Test can use this ActivityTestRule to launch or get WebLayerShellActivity.
 */
public class WebLayerShellActivityTestRule extends ActivityTestRule<WebLayerShellActivity> {
    private static final long WAIT_FOR_NAVIGATION_TIMEOUT = 10000L;

    public WebLayerShellActivityTestRule() {
        super(WebLayerShellActivity.class, false, false);
    }

    /**
     * Starts the WebLayer activity and loads the given URL.
     */
    public WebLayerShellActivity launchShellWithUrl(String url) {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        if (url != null) intent.setData(Uri.parse(url));
        intent.setComponent(
                new ComponentName(InstrumentationRegistry.getInstrumentation().getTargetContext(),
                        WebLayerShellActivity.class));
        return launchActivity(intent);
    }

    /**
     * Waits for the shell to navigate to the given URI.
     */
    public void waitForNavigation(String uri) {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                NavigationController navigationController =
                        getActivity().getBrowserController().getNavigationController();
                Uri currentUri = navigationController.getNavigationEntryDisplayUri(
                        navigationController.getNavigationListCurrentIndex());
                return currentUri.toString().equals(uri);
            }
        }, WAIT_FOR_NAVIGATION_TIMEOUT, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    /**
     * Loads the given URL in the shell.
     */
    public void loadUrl(String url) {
        TestThreadUtils.runOnUiThreadBlocking(() -> { getActivity().loadUrl(url); });
    }
}
