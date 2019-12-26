// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.content.Context;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.shadows.ShadowToast;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.components.signin.metrics.SigninAccessPoint;

/** Tests for the method startSigninActivityIfAllowed {@link SigninUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SigninUtilsStartActivityTest {
    @Mock
    private SigninManager mSigninManagerMock;

    @Mock
    private SigninActivityLauncher mLauncherMock;

    private final Context mContext = RuntimeEnvironment.application.getApplicationContext();

    @Before
    public void setUp() {
        initMocks(this);
        IdentityServicesProvider.setInstanceForTests(mock(IdentityServicesProvider.class));
        when(IdentityServicesProvider.get().getSigninManager()).thenReturn(mSigninManagerMock);
        SigninActivityLauncher.setLauncherForTest(mLauncherMock);
    }

    @Test
    public void testSigninActivityLaunchedWhenSigninIsAllowed() {
        when(mSigninManagerMock.isSignInAllowed()).thenReturn(true);
        Assert.assertTrue(
                SigninUtils.startSigninActivityIfAllowed(mContext, SigninAccessPoint.SETTINGS));
        verify(SigninActivityLauncher.get()).launchActivity(mContext, SigninAccessPoint.SETTINGS);
    }

    @Test
    public void testSigninActivityNotLaunchedWhenSigninIsNotAllowed() {
        when(mSigninManagerMock.isSignInAllowed()).thenReturn(false);
        when(mSigninManagerMock.isSigninDisabledByPolicy()).thenReturn(false);
        Object toastBeforeCall = ShadowToast.getLatestToast();
        Assert.assertFalse(
                SigninUtils.startSigninActivityIfAllowed(mContext, SigninAccessPoint.SETTINGS));
        Object toastAfterCall = ShadowToast.getLatestToast();
        verify(SigninActivityLauncher.get(), never()).launchActivity(any(Context.class), anyInt());
        Assert.assertEquals(
                "No new toast should be made during the call!", toastBeforeCall, toastAfterCall);
    }

    @Test
    public void testToastShownWhenSigninIsDisabledByPolicy() {
        when(mSigninManagerMock.isSignInAllowed()).thenReturn(false);
        when(mSigninManagerMock.isSigninDisabledByPolicy()).thenReturn(true);
        Assert.assertFalse(
                SigninUtils.startSigninActivityIfAllowed(mContext, SigninAccessPoint.SETTINGS));
        verify(SigninActivityLauncher.get(), never()).launchActivity(any(Context.class), anyInt());
        Assert.assertEquals(
                mContext.getResources().getString(R.string.managed_by_your_organization),
                ShadowToast.getTextOfLatestToast());
    }
}
