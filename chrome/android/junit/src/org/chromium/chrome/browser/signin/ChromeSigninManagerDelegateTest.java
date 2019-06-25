// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.components.sync.AndroidSyncSettings;
import org.chromium.components.sync.test.util.MockSyncContentResolverDelegate;

/** Tests for {@link ChromeSigninManagerDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ChromeSigninManagerDelegateTest {
    @Rule
    public JniMocker mocker = new JniMocker();

    @Mock
    ChromeSigninManagerDelegate.Natives mNativeMock;

    @Mock
    SigninManager.Natives mSigninManagerNativeMock;

    private ChromeSigninManagerDelegate mDelegate;

    @Before
    public void setUp() {
        initMocks(this);

        mocker.mock(ChromeSigninManagerDelegateJni.TEST_HOOKS, mNativeMock);
        mocker.mock(SigninManagerJni.TEST_HOOKS, mSigninManagerNativeMock);

        // SigninManager interacts with AndroidSyncSettings, but its not the focus
        // of this test. Using MockSyncContentResolver reduces burden of test setup.
        AndroidSyncSettings.overrideForTests(new MockSyncContentResolverDelegate(), null);

        mDelegate = new ChromeSigninManagerDelegate(AndroidSyncSettings.get());
    }
    @Test
    public void signOutWithManagedDomain() {
        // Stub out various native calls. Some of these are verified as never called
        // and those stubs simply allow that verification to catch any issues.
        doNothing().when(mSigninManagerNativeMock).wipeProfileData(any(), anyLong(), any());
        doNothing()
                .when(mSigninManagerNativeMock)
                .wipeGoogleServiceWorkerCaches(any(), anyLong(), any());

        // Simulate call from SigninManager
        mDelegate.disableSyncAndWipeData(null, 0L, true, null);

        // Sign-out should only clear the profile when the user is managed.
        verify(mSigninManagerNativeMock, times(1)).wipeProfileData(any(), anyLong(), any());
        verify(mSigninManagerNativeMock, never())
                .wipeGoogleServiceWorkerCaches(any(), anyLong(), any());
    }
    @Test
    public void signOutWithNullDomain() {
        // Stub out various native calls. Some of these are verified as never called
        // and those stubs simply allow that verification to catch any issues.
        doNothing().when(mSigninManagerNativeMock).wipeProfileData(any(), anyLong(), any());
        doNothing()
                .when(mSigninManagerNativeMock)
                .wipeGoogleServiceWorkerCaches(any(), anyLong(), any());

        // Simulate call from SigninManager
        mDelegate.disableSyncAndWipeData(null, 0L, false, null);

        // Sign-out should only clear the service worker cache when the user is not managed.
        verify(mSigninManagerNativeMock, never()).wipeProfileData(any(), anyLong(), any());
        verify(mSigninManagerNativeMock, times(1))
                .wipeGoogleServiceWorkerCaches(any(), anyLong(), any());
    }
}
