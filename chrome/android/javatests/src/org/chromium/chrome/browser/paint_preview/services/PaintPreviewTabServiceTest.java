// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.paint_preview.services;

import android.support.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.ScalableTimeout;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.TimeUnit;

/** Tests for the Paint Preview Tab Manager. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaintPreviewTabServiceTest {
    private static final long TIMEOUT_MS = ScalableTimeout.scaleTimeout(5000);
    private static final long POLLING_INTERVAL_MS = ScalableTimeout.scaleTimeout(500);

    @Rule
    public final ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private TabModelSelector mTabModelSelector;
    private TabModel mTabModel;
    private Tab mTab;
    private PaintPreviewTabService mPaintPreviewTabService;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mTab = mActivityTestRule.getActivity().getActivityTab();
        mTabModelSelector = mActivityTestRule.getActivity().getTabModelSelector();
        mTabModel = mTabModelSelector.getModel(/*incognito*/ false);
    }

    /**
     * Verifies that a Tab's contents are captured when the page is loaded and subsequently deleted
     * when the tab is closed.
     */
    @Test
    @MediumTest
    @Feature({"PaintPreview"})
    public void testCapturedAndDeleted() throws Exception {
        EmbeddedTestServer testServer = mActivityTestRule.getTestServer();
        final String url = testServer.getURL("/chrome/test/data/android/about.html");

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mPaintPreviewTabService = PaintPreviewTabServiceFactory.getServiceInstance();
            mPaintPreviewTabService.onRestoreCompleted(mTabModelSelector, true);
            mTab.loadUrl(new LoadUrlParams(url));
        });
        // Give the tab time to complete layout before hiding.
        TimeUnit.SECONDS.sleep(1);

        // This will hide mTab so that a capture occurs.
        mActivityTestRule.loadUrlInNewTab(url);

        int tabId = mTab.getId();
        CriteriaHelper.pollUiThread(() -> {
            return mPaintPreviewTabService.hasCaptureForTab(tabId);
        }, "Paint Preview didn't get captured.", TIMEOUT_MS, POLLING_INTERVAL_MS);

        CallbackHelper callbackHelper = new CallbackHelper();
        int callCount = callbackHelper.getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(() -> { mTabModel.closeTab(mTab); });

        CriteriaHelper.pollUiThread(() -> {
            return !mPaintPreviewTabService.hasCaptureForTab(tabId);
        }, "Paint Preview didn't get deleted.", TIMEOUT_MS, POLLING_INTERVAL_MS);
    }
}
