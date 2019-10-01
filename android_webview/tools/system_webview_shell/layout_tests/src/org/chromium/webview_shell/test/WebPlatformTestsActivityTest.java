// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_shell.test;

import static org.chromium.base.test.util.ScalableTimeout.scaleTimeout;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.support.test.rule.ActivityTestRule;
import android.webkit.WebView;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.webview_shell.WebPlatformTestsActivity;

import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;

/**
 * Tests to ensure that system webview shell can handle WPT tests correctly.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class WebPlatformTestsActivityTest {
    private static final String OPEN_TEST_WINDOW_SCRIPT =
            "<html><head><script>function ensure_test_window() {"
            + "  if (!this.test_window || this.test_window.location === null) {"
            + "    this.test_window = window.open('about:blank', 800, 600);"
            // https://crbug.com/1002727
            + "    setTimeout(function() { this.test_window.close(); }, 50);"
            + "  }"
            + "};"
            + "ensure_test_window();"
            + "</script></head><body>TestRunner Window</body></html>";

    private static final int CHILD_LAYOUT_ADDED = 1;
    private static final int CHILD_LAYOUT_REMOVED = 2;

    private static final long TEST_TIMEOUT_IN_SECONDS = scaleTimeout(3);

    private WebPlatformTestsActivity mTestActivity;

    @Rule
    public ActivityTestRule<WebPlatformTestsActivity> mActivityTestRule =
            new ActivityTestRule<>(WebPlatformTestsActivity.class, false, true);

    @Before
    public void setUp() {
        mTestActivity = mActivityTestRule.getActivity();
    }

    @Test
    @MediumTest
    public void testOpenDestroyWindowFromTestRunner() throws Exception {
        final BlockingQueue<Integer> queue = new LinkedBlockingQueue<>();

        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> {
            mTestActivity.setTestCallback(new WebPlatformTestsActivity.TestCallback() {
                @Override
                public void onChildLayoutAdded() {
                    queue.add(CHILD_LAYOUT_ADDED);
                }

                @Override
                public void onChildLayoutRemoved() {
                    queue.add(CHILD_LAYOUT_REMOVED);
                }
            });
            WebView webView = mTestActivity.getTestRunnerWebView();
            webView.loadDataWithBaseURL(
                    "https://some.domain.test/", OPEN_TEST_WINDOW_SCRIPT, "text/html", null, null);
        });
        Assert.assertEquals("Child window should be added.", Integer.valueOf(CHILD_LAYOUT_ADDED),
                queue.poll(TEST_TIMEOUT_IN_SECONDS, TimeUnit.SECONDS));
        Assert.assertEquals("Child window should be removed.",
                Integer.valueOf(CHILD_LAYOUT_REMOVED),
                queue.poll(TEST_TIMEOUT_IN_SECONDS, TimeUnit.SECONDS));
    }
}