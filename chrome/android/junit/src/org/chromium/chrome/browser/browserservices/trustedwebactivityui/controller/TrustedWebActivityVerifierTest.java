// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.browserservices.Origin;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller.TrustedWebActivityVerifier.VerificationStatus;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabObserverRegistrar;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.NavigationHandle;

import java.util.Collections;

/**
 * Tests for {@link Verifier}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures(ChromeFeatureList.TRUSTED_WEB_ACTIVITY)
@DisableFeatures(ChromeFeatureList.TRUSTED_WEB_ACTIVITY_POST_MESSAGE)
public class TrustedWebActivityVerifierTest {

    private static final Origin TRUSTED_ORIGIN = new Origin("https://www.origin1.com/");
    private static final Origin OTHER_TRUSTED_ORIGIN = new Origin("https://www.origin2.com/");
    private static final String TRUSTED_ORIGIN_PAGE1 = TRUSTED_ORIGIN + "/page1";
    private static final String TRUSTED_ORIGIN_PAGE2 = TRUSTED_ORIGIN + "/page2";
    private static final String OTHER_TRUSTED_ORIGIN_PAGE1 = OTHER_TRUSTED_ORIGIN + "/page1";
    private static final String OTHER_TRUSTED_ORIGIN_PAGE2 = OTHER_TRUSTED_ORIGIN + "/page2";
    private static final String UNTRUSTED_PAGE = "https://www.origin3.com/page1";

    public static final String PACKAGE_NAME = "package.name";

    @Rule
    public TestRule mFeaturesProcessor = new Features.JUnitProcessor();

    @Mock TabObserverRegistrar mTabObserverRegistrar;
    @Mock ActivityLifecycleDispatcher mLifecycleDispatcher;
    @Mock CustomTabActivityTabProvider mTabProvider;
    @Mock CustomTabIntentDataProvider mIntentDataProvider;
    @Mock Tab mTab;
    @Mock TwaRegistrar mTwaRegistrar;
    @Captor ArgumentCaptor<TabObserver> mTabObserverCaptor;

    TestVerifierDelegate mVerifierDelegate = new TestVerifierDelegate(PACKAGE_NAME);

    private TrustedWebActivityVerifier mVerifier;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        when(mTabProvider.getTab()).thenReturn(mTab);
        doNothing().when(mTabObserverRegistrar).registerTabObserver(mTabObserverCaptor.capture());
        when(mIntentDataProvider.getTrustedWebActivityAdditionalOrigins())
                .thenReturn(Collections.singletonList("https://www.origin2.com/"));
        mVerifier = new TrustedWebActivityVerifier(mLifecycleDispatcher, mTabObserverRegistrar,
                mTabProvider, mIntentDataProvider, mVerifierDelegate, mTwaRegistrar);
        // TODO(peconn): Add check on permission updated being updated.
    }

    @Test
    public void verifiesOriginOfInitialPage() {
        setInitialUrl(TRUSTED_ORIGIN_PAGE1);
        mVerifier.onFinishNativeInitialization();
        verifyStartsVerification(TRUSTED_ORIGIN_PAGE1);
    }

    @Test
    public void statusIsPending_UntilVerificationFinished() {
        setInitialUrl(TRUSTED_ORIGIN_PAGE1);
        mVerifier.onFinishNativeInitialization();
        assertStatus(VerificationStatus.PENDING);
    }

    @Test
    public void statusIsSuccess_WhenVerificationSucceeds() {
        setInitialUrl(TRUSTED_ORIGIN_PAGE1);
        mVerifier.onFinishNativeInitialization();
        mVerifierDelegate.passVerification(new Origin(TRUSTED_ORIGIN_PAGE1));
        assertStatus(VerificationStatus.SUCCESS);
    }

    @Test
    public void statusIsFail_WhenVerificationFails() {
        setInitialUrl(UNTRUSTED_PAGE);
        mVerifier.onFinishNativeInitialization();
        mVerifierDelegate.failVerification(new Origin(UNTRUSTED_PAGE));
        assertStatus(VerificationStatus.FAILURE);
    }

    @Test
    public void usesCache_whenNavigatingWithinPreviouslyVerifiedOrigin() {
        setInitialUrl(TRUSTED_ORIGIN_PAGE1);
        mVerifier.onFinishNativeInitialization();
        mVerifierDelegate.passVerification(new Origin(TRUSTED_ORIGIN_PAGE1));

        navigateToUrl(TRUSTED_ORIGIN_PAGE2);
        verifyUsesCache();
    }

    @Test
    public void verifies_WhenNavigatingToOtherTrustedOrigin() {
        setInitialUrl(TRUSTED_ORIGIN_PAGE1);
        mVerifier.onFinishNativeInitialization();
        mVerifierDelegate.passVerification(new Origin(TRUSTED_ORIGIN_PAGE1));

        navigateToUrl(OTHER_TRUSTED_ORIGIN_PAGE1);
        verifyStartsVerification(OTHER_TRUSTED_ORIGIN_PAGE1);
    }

    @Test
    public void usesCache_WhenReturningBackToFirstVerifiedOrigin() {
        setInitialUrl(TRUSTED_ORIGIN_PAGE1);
        mVerifier.onFinishNativeInitialization();
        mVerifierDelegate.passVerification(new Origin(TRUSTED_ORIGIN_PAGE1));
        navigateToUrl(OTHER_TRUSTED_ORIGIN_PAGE1);
        mVerifierDelegate.passVerification(new Origin(OTHER_TRUSTED_ORIGIN_PAGE1));

        navigateToUrl(TRUSTED_ORIGIN_PAGE2);
        verifyUsesCache();
    }

    @Test
    public void usesCache_WhenReturningBackToSecondVerifiedOrigin() {
        setInitialUrl(TRUSTED_ORIGIN_PAGE1);
        mVerifier.onFinishNativeInitialization();
        mVerifierDelegate.passVerification(new Origin(TRUSTED_ORIGIN_PAGE1));
        navigateToUrl(OTHER_TRUSTED_ORIGIN_PAGE1);
        mVerifierDelegate.passVerification(new Origin(OTHER_TRUSTED_ORIGIN_PAGE1));
        navigateToUrl(TRUSTED_ORIGIN_PAGE1);

        navigateToUrl(OTHER_TRUSTED_ORIGIN_PAGE2);
        verifyUsesCache();
    }

    @Test
    public void usesCache_WhenNavigatesToUntrustedOrigin() {
        setInitialUrl(TRUSTED_ORIGIN_PAGE1);
        mVerifier.onFinishNativeInitialization();
        mVerifierDelegate.passVerification(new Origin(TRUSTED_ORIGIN_PAGE1));

        navigateToUrl(UNTRUSTED_PAGE);
        verifyUsesCache();
    }

    @Test
    public void doesntUpdateState_IfVerificationFinishedAfterLeavingOrigin() {
        setInitialUrl(TRUSTED_ORIGIN_PAGE1);
        mVerifier.onFinishNativeInitialization();
        navigateToUrl(UNTRUSTED_PAGE);
        mVerifierDelegate.failVerification(new Origin(UNTRUSTED_PAGE));

        assertStatus(VerificationStatus.FAILURE);
    }

    @Test
    public void reverifiesOrigin_WhenReturningToIt_IfFirstVerificationDidntFinishInTime() {
        setInitialUrl(TRUSTED_ORIGIN_PAGE1);
        mVerifier.onFinishNativeInitialization();
        navigateToUrl(OTHER_TRUSTED_ORIGIN_PAGE1);
        mVerifierDelegate.passVerification(new Origin(OTHER_TRUSTED_ORIGIN_PAGE1));
        navigateToUrl(TRUSTED_ORIGIN_PAGE1);
        mVerifierDelegate.passVerification(new Origin(TRUSTED_ORIGIN_PAGE1));
        assertStatus(VerificationStatus.SUCCESS);
    }

    private void assertStatus(@TrustedWebActivityVerifier.VerificationStatus int status) {
        assertEquals(status, mVerifier.getState().status);
    }

    private void verifyUsesCache() {
        assertFalse(mVerifierDelegate.hasAnyPendingVerifications());
    }

    private void verifyStartsVerification(String url) {
        assertTrue(mVerifierDelegate.hasPendingVerification(new Origin(url)));
    }

    private void setInitialUrl(String url) {
        when(mIntentDataProvider.getUrlToLoad()).thenReturn(url);
        when(mTab.getUrl()).thenReturn(url);
    }

    private void navigateToUrl(String url) {
        when(mTab.getUrl()).thenReturn(url);
        NavigationHandle navigation =
                new NavigationHandle(0 /* navigationHandleProxy */, url, true /* isMainFrame */,
                        false /* isSameDocument */, false /* isRendererInitiated */);
        for (TabObserver tabObserver : mTabObserverCaptor.getAllValues()) {
            tabObserver.onDidStartNavigation(mTab, navigation);
        }

        navigation.didFinish(url, false /* isErrorPage */, true /* hasCommitted */,
                false /* isFragmentNavigation */, false /* isDownload */,
                false /* isValidSearchFormUrl */, 0 /* pageTransition */, 0 /* errorCode*/,
                200 /* httpStatusCode*/);
        for (TabObserver tabObserver : mTabObserverCaptor.getAllValues()) {
            tabObserver.onDidFinishNavigation(mTab, navigation);
        }
    }
}
