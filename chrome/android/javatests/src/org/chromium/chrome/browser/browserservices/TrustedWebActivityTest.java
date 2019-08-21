// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.browserservices.TrustedWebActivityTestUtil.createSession;
import static org.chromium.chrome.browser.browserservices.TrustedWebActivityTestUtil.createTrustedWebActivityIntent;
import static org.chromium.chrome.browser.browserservices.TrustedWebActivityTestUtil.isTrustedWebActivity;
import static org.chromium.chrome.browser.browserservices.TrustedWebActivityTestUtil.spoofVerification;

import android.content.Intent;
import android.support.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.net.test.EmbeddedTestServerRule;

import java.util.concurrent.TimeoutException;

import androidx.browser.customtabs.TrustedWebUtils;

/**
 * Instrumentation tests for launching
 * {@link org.chromium.chrome.browser.customtabs.CustomTabActivity} in Trusted Web Activity Mode.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TrustedWebActivityTest {
    // TODO(peconn): Add test for navigating away from the trusted origin.
    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();
    @Rule
    public EmbeddedTestServerRule mEmbeddedTestServerRule = new EmbeddedTestServerRule();

    private static final String TEST_PAGE = "/chrome/test/data/android/google.html";
    private static final String PACKAGE_NAME =
            ContextUtils.getApplicationContext().getPackageName();

    private String mTestPage;

    @Before
    public void setUp() throws InterruptedException, ProcessInitException {
        // Native needs to be initialized to start the test server.
        LibraryLoader.getInstance().ensureInitialized(LibraryProcessType.PROCESS_BROWSER);

        mEmbeddedTestServerRule.setServerUsesHttps(true); // TWAs only work with HTTPS.
        mTestPage = mEmbeddedTestServerRule.getServer().getURL(TEST_PAGE);
    }

    @Test
    @MediumTest
    public void launchesTwa() throws TimeoutException, InterruptedException {
        Intent intent = createTrustedWebActivityIntent(mTestPage);
        spoofVerification(PACKAGE_NAME, mTestPage);
        createSession(intent, PACKAGE_NAME);

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        assertTrue(isTrustedWebActivity(mCustomTabActivityTestRule.getActivity()));
    }

    @Test
    @MediumTest
    public void doesntLaunchTwa_WithoutFlag() throws TimeoutException, InterruptedException {
        Intent intent = createTrustedWebActivityIntent(mTestPage);
        spoofVerification(PACKAGE_NAME, mTestPage);
        createSession(intent, PACKAGE_NAME);

        intent.putExtra(TrustedWebUtils.EXTRA_LAUNCH_AS_TRUSTED_WEB_ACTIVITY, false);

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        assertFalse(isTrustedWebActivity(mCustomTabActivityTestRule.getActivity()));
    }

    @Test
    @MediumTest
    public void leavesTwa_VerificationFailure() throws TimeoutException, InterruptedException {
        Intent intent = createTrustedWebActivityIntent(mTestPage);
        createSession(intent, PACKAGE_NAME);

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        assertFalse(isTrustedWebActivity(mCustomTabActivityTestRule.getActivity()));
    }
}
