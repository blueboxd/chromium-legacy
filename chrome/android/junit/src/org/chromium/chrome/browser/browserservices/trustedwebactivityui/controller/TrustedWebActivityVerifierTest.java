// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
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
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.browserservices.Origin;
import org.chromium.chrome.browser.browserservices.OriginVerifier;
import org.chromium.chrome.browser.browserservices.OriginVerifier.OriginVerificationListener;
import org.chromium.chrome.browser.browserservices.permissiondelegation.NotificationPermissionUpdater;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller.TrustedWebActivityVerifier.VerificationStatus;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabObserverRegistrar;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.NavigationHandle;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

/**
 * Tests for {@link TrustedWebActivityVerifier}.
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

    @Mock ClientAppDataRecorder mClientAppDataRecorder;
    @Mock CustomTabsConnection mCustomTabsConnection;
    @Mock CustomTabIntentDataProvider mIntentDataProvider;
    @Mock TabObserverRegistrar mTabObserverRegistrar;
    @Mock ActivityLifecycleDispatcher mLifecycleDispatcher;
    @Mock OriginVerifier.Factory mOriginVerifierFactory;
    @Mock CustomTabActivityTabProvider mTabProvider;
    @Mock Tab mTab;
    @Mock NotificationPermissionUpdater mNotificationPermissionUpdater;
    @Mock ChromeActivity mChromeActivity;
    @Captor ArgumentCaptor<TabObserver> mTabObserverCaptor;

    private final FakeOriginVerifier mOriginVerifier = new FakeOriginVerifier();

    private TrustedWebActivityVerifier mVerifier;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        when(mCustomTabsConnection.getClientPackageNameForSession(any())).thenReturn(PACKAGE_NAME);
        when(mOriginVerifierFactory.create(any(), anyInt(), any()))
                .thenReturn(mOriginVerifier.mock);
        when(mTabProvider.getTab()).thenReturn(mTab);
        when(mIntentDataProvider.getTrustedWebActivityAdditionalOrigins()).thenReturn(
                Arrays.asList("https://www.origin2.com/"));
        doNothing().when(mTabObserverRegistrar).registerTabObserver(mTabObserverCaptor.capture());
        mVerifier = new TrustedWebActivityVerifier(() -> mClientAppDataRecorder,
                mIntentDataProvider, mCustomTabsConnection, mLifecycleDispatcher,
                mTabObserverRegistrar, mOriginVerifierFactory,
                mTabProvider, mChromeActivity, mNotificationPermissionUpdater);
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
        mOriginVerifier.finishCurrentVerification();
        assertStatus(VerificationStatus.SUCCESS);
    }

    @Test
    public void statusIsFail_WhenVerificationFails() {
        setInitialUrl(UNTRUSTED_PAGE);
        mVerifier.onFinishNativeInitialization();
        mOriginVerifier.finishCurrentVerification();
        assertStatus(VerificationStatus.FAILURE);
    }

    @Test
    public void usesCache_whenNavigatingWithinPreviouslyVerifiedOrigin() {
        setInitialUrl(TRUSTED_ORIGIN_PAGE1);
        mVerifier.onFinishNativeInitialization();
        mOriginVerifier.finishCurrentVerification();

        clearInvocations(mOriginVerifier.mock);
        navigateToUrl(TRUSTED_ORIGIN_PAGE2);
        verifyUsesCache();
    }

    @Test
    public void verifies_WhenNavigatingToOtherTrustedOrigin() {
        setInitialUrl(TRUSTED_ORIGIN_PAGE1);
        mVerifier.onFinishNativeInitialization();
        mOriginVerifier.finishCurrentVerification();

        clearInvocations(mOriginVerifier.mock);
        navigateToUrl(OTHER_TRUSTED_ORIGIN_PAGE1);
        verifyStartsVerification(OTHER_TRUSTED_ORIGIN_PAGE1);
    }

    @Test
    public void usesCache_WhenReturningBackToFirstVerifiedOrigin() {
        setInitialUrl(TRUSTED_ORIGIN_PAGE1);
        mVerifier.onFinishNativeInitialization();
        mOriginVerifier.finishCurrentVerification();
        navigateToUrl(OTHER_TRUSTED_ORIGIN_PAGE1);
        mOriginVerifier.finishCurrentVerification();

        clearInvocations(mOriginVerifier.mock);
        navigateToUrl(TRUSTED_ORIGIN_PAGE2);
        verifyUsesCache();
    }

    @Test
    public void usesCache_WhenReturningBackToSecondVerifiedOrigin() {
        setInitialUrl(TRUSTED_ORIGIN_PAGE1);
        mVerifier.onFinishNativeInitialization();
        mOriginVerifier.finishCurrentVerification();
        navigateToUrl(OTHER_TRUSTED_ORIGIN_PAGE1);
        mOriginVerifier.finishCurrentVerification();
        navigateToUrl(TRUSTED_ORIGIN_PAGE1);

        clearInvocations(mOriginVerifier.mock);
        navigateToUrl(OTHER_TRUSTED_ORIGIN_PAGE2);
        verifyUsesCache();
    }

    @Test
    public void usesCache_WhenNavigatesToUntrustedOrigin() {
        setInitialUrl(TRUSTED_ORIGIN_PAGE1);
        mVerifier.onFinishNativeInitialization();
        mOriginVerifier.finishCurrentVerification();

        clearInvocations(mOriginVerifier.mock);
        navigateToUrl(UNTRUSTED_PAGE);
        verifyUsesCache();
    }

    @Test
    public void doesntUpdateState_IfVerificationFinishedAfterLeavingOrigin() {
        setInitialUrl(TRUSTED_ORIGIN_PAGE1);
        mVerifier.onFinishNativeInitialization();
        navigateToUrl(UNTRUSTED_PAGE);
        mOriginVerifier.finishCurrentVerification();
        assertStatus(VerificationStatus.FAILURE);
    }

    @Test
    public void reverifiesOrigin_WhenReturningToIt_IfFirstVerificationDidntFinishInTime() {
        setInitialUrl(TRUSTED_ORIGIN_PAGE1);
        mVerifier.onFinishNativeInitialization();
        navigateToUrl(OTHER_TRUSTED_ORIGIN_PAGE1);
        mOriginVerifier.finishCurrentVerification();
        navigateToUrl(TRUSTED_ORIGIN_PAGE1);
        mOriginVerifier.finishCurrentVerification();
        assertStatus(VerificationStatus.SUCCESS);
    }

    private void assertStatus(@TrustedWebActivityVerifier.VerificationStatus int status) {
        assertEquals(status, mVerifier.getState().status);
    }

    private void verifyUsesCache() {
        verify(mOriginVerifier.mock).wasPreviouslyVerified(any());
        verify(mOriginVerifier.mock, never()).start(any(), any());
    }

    private void verifyStartsVerification(String urlToVerify) {
        verify(mOriginVerifier.mock).start(any(), argThat(new Origin(urlToVerify)::equals));
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

    private static class FakeOriginVerifier {

        public final OriginVerifier mock = mock(OriginVerifier.class);
        private final Set<Origin> mPreviouslyVerifiedOrigins = new HashSet<>();

        private OriginVerificationListener mCurrentListener;
        private Origin mCurrentOrigin;

        public FakeOriginVerifier() {
            doAnswer(invocation -> {
                start(invocation.getArgument(0), invocation.getArgument(1));
                return null;
            }).when(mock).start(any(), any());

            doAnswer(invocation -> wasPreviouslyVerified(invocation.getArgument(0)))
                    .when(mock).wasPreviouslyVerified(any());
        }

        private void start(OriginVerificationListener listener, Origin origin) {
            mCurrentListener = listener;
            mCurrentOrigin = origin;
        }

        private boolean wasPreviouslyVerified(Origin origin) {
            return mPreviouslyVerifiedOrigins.contains(origin);
        }

        public void finishCurrentVerification() {
            if (mCurrentListener == null) {
                return;
            }
            boolean verified = mCurrentOrigin.equals(TRUSTED_ORIGIN)
                    || mCurrentOrigin.equals(OTHER_TRUSTED_ORIGIN);
            if (verified) {
                mPreviouslyVerifiedOrigins.add(mCurrentOrigin);
            }
            mCurrentListener.onOriginVerified(PACKAGE_NAME, mCurrentOrigin, verified, true);
            mCurrentListener = null;
            mCurrentOrigin = null;
        }
    }
}
