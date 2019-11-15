// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.ServerCertificate;
import org.chromium.weblayer.ErrorPageCallback;
import org.chromium.weblayer.shell.InstrumentationActivity;

/**
 * Tests that ErrorPageCallback works as expected for handling error page interactions.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class ErrorPageCallbackTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();
    private InstrumentationActivity mActivity;
    private Callback mCallback;
    private EmbeddedTestServer mServer;

    private static class Callback extends ErrorPageCallback {
        private final CallbackHelper mCallbackHelper = new CallbackHelper();

        @Override
        public boolean onBackToSafety() {
            mCallbackHelper.notifyCalled();
            return true;
        }

        public void waitForBackToSafety() {
            try {
                mCallbackHelper.waitForFirst();
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }
    }

    @Before
    public void setUp() {
        mActivity = mActivityTestRule.launchShellWithUrl(null);
        Assert.assertNotNull(mActivity);
        mServer = EmbeddedTestServer.createAndStartHTTPSServer(
                mActivity, ServerCertificate.CERT_MISMATCHED_NAME);

        mCallback = new Callback();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mActivity.getTab().setErrorPageCallback(mCallback); });
    }

    /**
     * Verifies that ErrorPageCallback.onBackToSafety() is called after a user encounters an SSL
     * error and clicks "back to safety".
     */
    @Test
    @SmallTest
    public void testBackToSafetyOverride() throws Throwable {
        mActivityTestRule.navigateAndWaitForFailure(
                mServer.getURL("/weblayer/test/data/simple_page.html"));
        mActivityTestRule.executeScriptSync(
                "window.certificateErrorPageController.dontProceed();", false);
        mCallback.waitForBackToSafety();
        // TODO(estade): verify default behavior happens or is skipped as appropriate.
    }
}
