// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller;

import static org.junit.Assert.assertEquals;

import android.content.Intent;
import android.net.Uri;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.LargeTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.CommandLine;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.browserservices.TrustedWebActivityTestUtil;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller.CurrentPageVerifier.VerificationStatus;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.test.MockCertVerifierRuleAndroid;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.NativeLibraryTestRule;
import org.chromium.content_public.common.ContentSwitches;

import java.util.concurrent.TimeoutException;

/**
 * Tests the {@link CurrentPageVerifier} integration with Trusted Web Activity Mode.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public final class TrustedWebActivityCurrentPageVerifierTest {
    public final CustomTabActivityTestRule mActivityTestRule = new CustomTabActivityTestRule();

    public final NativeLibraryTestRule mNativeLibraryTestRule = new NativeLibraryTestRule();

    public MockCertVerifierRuleAndroid mCertVerifierRule =
            new MockCertVerifierRuleAndroid(mNativeLibraryTestRule, 0 /* net::OK */);

    @Rule
    public RuleChain mRuleChain = RuleChain.emptyRuleChain()
                                          .around(mActivityTestRule)
                                          .around(mNativeLibraryTestRule)
                                          .around(mCertVerifierRule);

    @Before
    public void setUp() {
        mActivityTestRule.getEmbeddedTestServerRule().setServerUsesHttps(true);
        Uri mapToUri =
                Uri.parse(mActivityTestRule.getEmbeddedTestServerRule().getServer().getURL("/"));
        CommandLine.getInstance().appendSwitchWithValue(
                ContentSwitches.HOST_RESOLVER_RULES, "MAP * " + mapToUri.getAuthority());
    }

    private void launchTwa(String url) throws TimeoutException {
        String packageName = InstrumentationRegistry.getTargetContext().getPackageName();
        Intent intent = TrustedWebActivityTestUtil.createTrustedWebActivityIntent(url);
        TrustedWebActivityTestUtil.spoofVerification(packageName, url);
        TrustedWebActivityTestUtil.createSession(intent, packageName);
        mActivityTestRule.startCustomTabActivityWithIntent(intent);
    }

    private @VerificationStatus int getCurrentPageVerifierStatus() {
        CustomTabActivity customTabActivity = mActivityTestRule.getActivity();
        return customTabActivity.getComponent().resolveCurrentPageVerifier().getState().status;
    }

    @Test
    @LargeTest
    public void testInScope() throws TimeoutException {
        String page = "https://foo.com/chrome/test/data/android/customtabs/cct_header.html";
        String otherPageInScope =
                "https://foo.com/chrome/test/data/android/customtabs/cct_header_frame.html";
        launchTwa(page);

        mActivityTestRule.loadUrl(otherPageInScope);

        TrustedWebActivityTestUtil.waitForCurrentPageVerifierToFinish(
                mActivityTestRule.getActivity());
        assertEquals(VerificationStatus.SUCCESS, getCurrentPageVerifierStatus());
    }

    @Test
    @LargeTest
    public void testOutsideScope() throws TimeoutException {
        String page = "https://foo.com/chrome/test/data/android/simple.html";
        String pageDifferentOrigin = "https://bar.com/chrome/test/data/android/simple.html";
        launchTwa(page);

        mActivityTestRule.loadUrl(pageDifferentOrigin, 10 /* secondsToWait */);

        TrustedWebActivityTestUtil.waitForCurrentPageVerifierToFinish(
                mActivityTestRule.getActivity());
        assertEquals(VerificationStatus.FAILURE, getCurrentPageVerifierStatus());
    }
}
