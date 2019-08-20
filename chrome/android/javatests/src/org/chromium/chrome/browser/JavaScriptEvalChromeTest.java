// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Intent;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.TimeoutException;

/**
 * Tests for evaluation of JavaScript.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class JavaScriptEvalChromeTest {
    @Rule
    public ChromeActivityTestRule<? extends ChromeActivity> mActivityTestRule =
            ChromeActivityTestRule.forMainActivity();

    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    private static final String JSTEST_URL = UrlUtils.encodeHtmlDataUri(
            "<html><head><script>"
            + "  var counter = 0;"
            + "  function add2() { counter = counter + 2; return counter; }"
            + "  function foobar() { return 'foobar'; }"
            + "</script></head>"
            + "<body><button id=\"test\">Test button</button></body></html>");

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityWithURL(JSTEST_URL);
    }

    /**
     * Tests that evaluation of JavaScript for test purposes (using JavaScriptUtils, DOMUtils etc)
     * works even in presence of "background" (non-test-initiated) JavaScript evaluation activity.
     */
    @Test
    @LargeTest
    @Feature({"Browser"})
    @RetryOnFailure
    public void testJavaScriptEvalIsCorrectlyOrderedWithinOneTab()
            throws InterruptedException, TimeoutException {
        Tab tab1 = mActivityTestRule.getActivity().getActivityTab();
        Tab tab2;
        if (mActivityTestRule.getActivity() instanceof ChromeTabbedActivity) {
            ChromeTabUtils.newTabFromMenu(
                    InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
            tab2 = mActivityTestRule.getActivity().getActivityTab();
            mActivityTestRule.loadUrl(JSTEST_URL);
            ChromeTabUtils.switchTabInCurrentTabModel(
                    mActivityTestRule.getActivity(), tab1.getId());
        } else {
            // For now, only NoTouchMode should hit this path.
            // In NoTouchMode, multiple tabs are only supported though CCT, so use a CCT instead of
            // a second tab.
            Assert.assertTrue(FeatureUtilities.isNoTouchModeEnabled());
            Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(
                    InstrumentationRegistry.getTargetContext(), "about:blank");
            // NoTouchMode only allows CCT for 1p use-cases.
            IntentHandler.addTrustedIntentExtras(intent);
            mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
            tab2 = mCustomTabActivityTestRule.getActivity().getActivityTab();
            mCustomTabActivityTestRule.loadUrl(JSTEST_URL);
        }

        Assert.assertFalse("Tab didn't open", tab1 == tab2);

        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                tab1.getWebContents(), "counter = 0;");
        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                tab2.getWebContents(), "counter = 1;");

        for (int i = 1; i <= 30; ++i) {
            for (int j = 0; j < 5; ++j) {
                // Start evaluation of a JavaScript script -- we don't need a result.
                TestThreadUtils.runOnUiThreadBlocking(() -> {
                    tab1.getWebContents().evaluateJavaScriptForTests("foobar();", null);
                    tab2.getWebContents().evaluateJavaScriptForTests("foobar();", null);
                });
            }
            Assert.assertEquals("Incorrect JavaScript evaluation result on tab1", i * 2,
                    Integer.parseInt(JavaScriptUtils.executeJavaScriptAndWaitForResult(
                            tab1.getWebContents(), "add2()")));
            for (int j = 0; j < 5; ++j) {
                // Start evaluation of a JavaScript script -- we don't need a result.
                TestThreadUtils.runOnUiThreadBlocking(() -> {
                    tab1.getWebContents().evaluateJavaScriptForTests("foobar();", null);
                    tab2.getWebContents().evaluateJavaScriptForTests("foobar();", null);
                });
            }
            Assert.assertEquals("Incorrect JavaScript evaluation result on tab2", i * 2 + 1,
                    Integer.parseInt(JavaScriptUtils.executeJavaScriptAndWaitForResult(
                            tab2.getWebContents(), "add2()")));
        }
    }
}
