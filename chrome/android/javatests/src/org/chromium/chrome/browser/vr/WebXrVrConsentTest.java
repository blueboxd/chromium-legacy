// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import static org.chromium.chrome.browser.vr.XrTestFramework.PAGE_LOAD_TIMEOUT_S;
import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_TIMEOUT_LONG_MS;
import static org.chromium.chrome.browser.vr.XrTestFramework.POLL_TIMEOUT_SHORT_MS;

import android.os.Build;
import android.support.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.vr.rules.XrActivityRestriction;
import org.chromium.chrome.browser.vr.util.VrTestRuleUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.ContentFeatureList;

import java.util.List;
import java.util.concurrent.Callable;

/**
 * End-to-end tests for various scenarios around when the consent dialog is expected.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.
Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "enable-features=WebXR,LogJsConsoleMessages"})
@MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP) // WebXR is only supported on L+
public class WebXrVrConsentTest {
    @ClassParameter
    private static List<ParameterSet> sClassParams =
            VrTestRuleUtils.generateDefaultTestRuleParameters();
    @Rule
    public RuleChain mRuleChain;

    private ChromeActivityTestRule mTestRule;
    private WebXrVrConsentTestFramework mWebXrVrConsentTestFramework;

    public WebXrVrConsentTest(Callable<ChromeActivityTestRule> callable) throws Exception {
        mTestRule = callable.call();
        mRuleChain = VrTestRuleUtils.wrapRuleInActivityRestrictionRule(mTestRule);
    }

    @Before
    public void setUp() {
        mWebXrVrConsentTestFramework = new WebXrVrConsentTestFramework(mTestRule);
    }

    @Test
    @MediumTest
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    @DisableFeatures(ContentFeatureList.WEBXR_PERMISSIONS_API)
    public void testConsentCancelFailsSessionCreation() {
        testConsentCancelFailsSessionCreationImpl();
    }

    @Test
    @MediumTest
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    @EnableFeatures(ContentFeatureList.WEBXR_PERMISSIONS_API)
    public void testPermissionDenyFailsSessionCreation() {
        testConsentCancelFailsSessionCreationImpl();
    }

    /**
     * Tests that denying consent blocks the session from being created.
     */
    private void testConsentCancelFailsSessionCreationImpl() {
        mWebXrVrConsentTestFramework.setConsentDialogAction(
                WebXrVrTestFramework.CONSENT_DIALOG_ACTION_DENY);
        mWebXrVrConsentTestFramework.setConsentDialogExpected(true);

        mWebXrVrConsentTestFramework.loadFileAndAwaitInitialization(
                "test_webxr_consent", PAGE_LOAD_TIMEOUT_S);

        mWebXrVrConsentTestFramework.enterSessionWithUserGesture();
        mWebXrVrConsentTestFramework.pollJavaScriptBooleanOrFail(
                "sessionInfos[sessionTypes.IMMERSIVE].error != null", POLL_TIMEOUT_LONG_MS);
        mWebXrVrConsentTestFramework.runJavaScriptOrFail(
                "verifySessionConsentError(sessionTypes.IMMERSIVE)", POLL_TIMEOUT_SHORT_MS);
        mWebXrVrConsentTestFramework.assertNoJavaScriptErrors();
    }

    @Test
    @MediumTest
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    @DisableFeatures(ContentFeatureList.WEBXR_PERMISSIONS_API)
    public void testConsentPersistsSameLevel() {
        testConsentPersistsImpl();
    }

    @Test
    @MediumTest
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    @EnableFeatures(ContentFeatureList.WEBXR_PERMISSIONS_API)
    public void testVrPermissionPersistance() {
        testConsentPersistsImpl();
    }

    /**
     * Tests that attempting to enter a session that requires the same permission level/type does
     * not reprompt.
     */
    private void testConsentPersistsImpl() {
        mWebXrVrConsentTestFramework.loadFileAndAwaitInitialization(
                "generic_webxr_page", PAGE_LOAD_TIMEOUT_S);
        mWebXrVrConsentTestFramework.enterSessionWithUserGestureOrFail();
        mWebXrVrConsentTestFramework.endSession();

        mWebXrVrConsentTestFramework.setConsentDialogExpected(false);

        mWebXrVrConsentTestFramework.enterSessionWithUserGestureOrFail();
    }

    @Test
    @MediumTest
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    @DisableFeatures(ContentFeatureList.WEBXR_PERMISSIONS_API)
    public void testConsentNotNeededForInline() {
        testConsentNotNeededForInlineImpl();
    }

    @Test
    @MediumTest
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    @EnableFeatures(ContentFeatureList.WEBXR_PERMISSIONS_API)
    public void testPermissionNotNeededForInline() {
        testConsentNotNeededForInlineImpl();
    }

    /**
     * Tests that attempting to enter an inline session with no special features does not require
     * consent.
     */
    private void testConsentNotNeededForInlineImpl() {
        mWebXrVrConsentTestFramework.setConsentDialogExpected(false);

        mWebXrVrConsentTestFramework.loadFileAndAwaitInitialization(
                "test_webxr_consent", PAGE_LOAD_TIMEOUT_S);

        mWebXrVrConsentTestFramework.runJavaScriptOrFail(
                "requestMagicWindowSession()", POLL_TIMEOUT_SHORT_MS);
        mWebXrVrConsentTestFramework.pollJavaScriptBooleanOrFail(
                "sessionInfos[sessionTypes.MAGIC_WINDOW].currentSession != null",
                POLL_TIMEOUT_LONG_MS);
    }

    /**
     * Tests that if consent is granted for a higher level, the lower level does not need consent.
     * Consent-prompt flow specific.
     */
    @Test
    @MediumTest
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    @DisableFeatures(ContentFeatureList.WEBXR_PERMISSIONS_API)
    public void testConsentPersistsLowerLevel() {
        mWebXrVrConsentTestFramework.loadFileAndAwaitInitialization(
                "test_webxr_consent", PAGE_LOAD_TIMEOUT_S);

        // Set up to request the highest level of consent support on Android (height).
        mWebXrVrConsentTestFramework.runJavaScriptOrFail(
                "setupImmersiveSessionToRequestHeight()", POLL_TIMEOUT_SHORT_MS);
        mWebXrVrConsentTestFramework.enterSessionWithUserGestureOrFail();
        mWebXrVrConsentTestFramework.endSession();

        // Now set up to request the lower level of consent. The session should be entered without
        // the consent prompt.
        mWebXrVrConsentTestFramework.setConsentDialogExpected(false);
        mWebXrVrConsentTestFramework.runJavaScriptOrFail(
                "setupImmersiveSessionToRequestDefault()", POLL_TIMEOUT_SHORT_MS);
        mWebXrVrConsentTestFramework.enterSessionWithUserGestureOrFail();
    }

    /**
     * Tests that if consent is granted for a lower level, the higher level still needs consent.
     * Consent-prompt flow specific.
     */
    @Test
    @MediumTest
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    @DisableFeatures(ContentFeatureList.WEBXR_PERMISSIONS_API)
    // Need to force consent prompt visible
    public void testConsentRepromptsHigherLevel() {
        mWebXrVrConsentTestFramework.loadFileAndAwaitInitialization(
                "test_webxr_consent", PAGE_LOAD_TIMEOUT_S);

        // Request a session at the lowest level of consent, and ensure that it is entered.
        mWebXrVrConsentTestFramework.runJavaScriptOrFail(
                "setupImmersiveSessionToRequestDefault()", POLL_TIMEOUT_SHORT_MS);
        mWebXrVrConsentTestFramework.enterSessionWithUserGestureOrFail();
        mWebXrVrConsentTestFramework.endSession();

        // Now request a session that requires a higher level of consent. It should still be
        // prompted for consent and the session should enter.
        mWebXrVrConsentTestFramework.runJavaScriptOrFail(
                "setupImmersiveSessionToRequestHeight()", POLL_TIMEOUT_SHORT_MS);
        mWebXrVrConsentTestFramework.enterSessionWithUserGestureOrFail();
    }

    @Test
    @MediumTest
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    @DisableFeatures(ContentFeatureList.WEBXR_PERMISSIONS_API)
    public void testConsentRepromptsAfterReload() {
        // Verfies that consent is prompted for again after reload.
        testConsentAfterReload(true);
    }

    @Test
    @MediumTest
    @XrActivityRestriction({XrActivityRestriction.SupportedActivity.ALL})
    @EnableFeatures(ContentFeatureList.WEBXR_PERMISSIONS_API)
    public void testPermissionPersistsAfterReload() {
        // Verifies that permission is persisted after reload.
        testConsentAfterReload(false);
    }

    /**
     * Tests that granted consent does not persist after a page reload.
     */
    private void testConsentAfterReload(boolean expectedAfterReload) {
        mWebXrVrConsentTestFramework.loadFileAndAwaitInitialization(
                "generic_webxr_page", PAGE_LOAD_TIMEOUT_S);

        mWebXrVrConsentTestFramework.enterSessionWithUserGestureOrFail();
        mWebXrVrConsentTestFramework.endSession();

        mWebXrVrConsentTestFramework.loadFileAndAwaitInitialization(
                "generic_webxr_page", PAGE_LOAD_TIMEOUT_S);

        mWebXrVrConsentTestFramework.setConsentDialogExpected(expectedAfterReload);

        mWebXrVrConsentTestFramework.enterSessionWithUserGestureOrFail();
    }
}
