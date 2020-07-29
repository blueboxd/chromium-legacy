// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.os.Bundle;

import org.junit.Assert;
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
import org.robolectric.shadows.ShadowPackageManager;
import org.robolectric.shadows.ShadowToast;

import org.chromium.base.ContextUtils;
import org.chromium.base.Promise;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.browserservices.ui.controller.Verifier;
import org.chromium.chrome.browser.browserservices.ui.controller.trustedwebactivity.ClientPackageNameProvider;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar.CustomTabTabObserver;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.NavigationHandle;

/**
 * Tests for {@link QualityEnforcer}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@DisableFeatures(ChromeFeatureList.TRUSTED_WEB_ACTIVITY_QUALITY_ENFORCEMENT)
public class QualityEnforcerUnitTest {
    private static final String TRUSTED_ORIGIN_PAGE = "https://www.origin1.com/page1";
    private static final String UNTRUSTED_PAGE = "https://www.origin2.com/page1";
    private static final int HTTP_STATUS_SUCCESS = 200;
    private static final int HTTP_ERROR_NOT_FOUND = 404;

    @Rule
    public TestRule mFeaturesProcessor = new Features.JUnitProcessor();
    @Mock
    private ChromeActivity mActivity;
    @Mock
    private CustomTabIntentDataProvider mIntentDataProvider;
    @Mock
    private CustomTabsConnection mCustomTabsConnection;
    @Mock
    private Verifier mVerifier;
    @Mock
    private ClientPackageNameProvider mClientPackageNameProvider;
    @Mock
    private TabObserverRegistrar mTabObserverRegistrar;
    @Captor
    private ArgumentCaptor<CustomTabTabObserver> mTabObserverCaptor;
    @Mock
    private Tab mTab;

    private ShadowPackageManager mShadowPackageManager;

    private QualityEnforcer mQualityEnforcer;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doNothing()
                .when(mTabObserverRegistrar)
                .registerActivityTabObserver(mTabObserverCaptor.capture());

        when(mVerifier.verify(TRUSTED_ORIGIN_PAGE)).thenReturn(Promise.fulfilled(true));
        when(mVerifier.verify(UNTRUSTED_PAGE)).thenReturn(Promise.fulfilled(false));

        mQualityEnforcer = new QualityEnforcer(mActivity, mTabObserverRegistrar,
                mIntentDataProvider, mCustomTabsConnection, mVerifier, mClientPackageNameProvider);
    }

    @Test
    public void trigger_navigateTo404() {
        navigateToUrl(TRUSTED_ORIGIN_PAGE, HTTP_ERROR_NOT_FOUND);
        verifyTriggered404();
    }

    @Test
    public void notTrigger_navigationSuccess() {
        navigateToUrl(TRUSTED_ORIGIN_PAGE, HTTP_STATUS_SUCCESS);
        verifyNotTriggered();
    }

    @Test
    public void notTrigger_navigateTo404NotVerifiedSite() {
        navigateToUrl(UNTRUSTED_PAGE, HTTP_ERROR_NOT_FOUND);
        verifyNotTriggered();
    }

    @Test
    public void notTrigger_navigateFromNotVerifiedToVerified404() {
        navigateToUrl(UNTRUSTED_PAGE, HTTP_STATUS_SUCCESS);
        navigateToUrl(TRUSTED_ORIGIN_PAGE, HTTP_ERROR_NOT_FOUND);
        verifyNotTriggered();
    }

    @Test
    public void trigger_notVerifiedToVerifiedThen404() {
        navigateToUrl(UNTRUSTED_PAGE, HTTP_STATUS_SUCCESS);
        navigateToUrl(TRUSTED_ORIGIN_PAGE, HTTP_STATUS_SUCCESS);
        navigateToUrl(TRUSTED_ORIGIN_PAGE, HTTP_ERROR_NOT_FOUND);
        verifyTriggered404();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TRUSTED_WEB_ACTIVITY_QUALITY_ENFORCEMENT)
    public void triggerCrash_whenClientSupports() {
        Bundle result = new Bundle();
        result.putBoolean("success", true);
        when(mCustomTabsConnection.sendExtraCallbackWithResult(
                     any(), eq(QualityEnforcer.CRASH), any()))
                .thenReturn(result);

        navigateToUrl(TRUSTED_ORIGIN_PAGE, HTTP_ERROR_NOT_FOUND);
        verify(mActivity).finish();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TRUSTED_WEB_ACTIVITY_QUALITY_ENFORCEMENT)
    public void notTriggerCrash_whenClientDoesntSupport() {
        Bundle result = new Bundle();
        result.putBoolean("success", false);
        when(mCustomTabsConnection.sendExtraCallbackWithResult(
                     any(), eq(QualityEnforcer.CRASH), any()))
                .thenReturn(result);

        navigateToUrl(TRUSTED_ORIGIN_PAGE, HTTP_ERROR_NOT_FOUND);
        verify(mActivity, never()).finish();
    }

    private void verifyTriggered404() {
        Assert.assertEquals(ContextUtils.getApplicationContext().getString(
                                    R.string.twa_quality_enforcement_violation_error,
                                    HTTP_ERROR_NOT_FOUND, TRUSTED_ORIGIN_PAGE),
                ShadowToast.getTextOfLatestToast());
        verify(mCustomTabsConnection)
                .sendExtraCallbackWithResult(any(), eq(QualityEnforcer.NOTIFY), any());
    }

    private void verifyNotTriggered() {
        verify(mCustomTabsConnection, never())
                .sendExtraCallbackWithResult(any(), eq(QualityEnforcer.NOTIFY), any());
    }

    private void navigateToUrl(String url, int httpStatusCode) {
        when(mTab.getOriginalUrl()).thenReturn(url);

        NavigationHandle navigation =
                new NavigationHandle(0 /* navigationHandleProxy */, url, true /* isMainFrame */,
                        false /* isSameDocument */, false /* isRendererInitiated */);
        navigation.didFinish(url, false /* isErrorPage */, true /* hasCommitted */,
                false /* isFragmentNavigation */, false /* isDownload */,
                false /* isValidSearchFormUrl */, 0 /* pageTransition */, 0 /* errorCode*/,
                httpStatusCode);
        for (CustomTabTabObserver tabObserver : mTabObserverCaptor.getAllValues()) {
            tabObserver.onDidFinishNavigation(mTab, navigation);
        }
    }
}
