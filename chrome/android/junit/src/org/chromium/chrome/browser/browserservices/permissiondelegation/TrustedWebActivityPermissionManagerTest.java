// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.permissiondelegation;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowPackageManager;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.embedder_support.util.Origin;

import dagger.Lazy;

/**
 * Tests for {@link TrustedWebActivityPermissionManager}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TrustedWebActivityPermissionManagerTest {
    private static final Origin ORIGIN = Origin.create("https://www.website.com");
    private static final String PACKAGE_NAME = "com.package.name";

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock
    public TrustedWebActivityPermissionStore mStore;
    @Mock
    public Lazy<NotificationChannelPreserver> mPreserver;

    private TrustedWebActivityPermissionManager mPermissionManager;

    private ShadowPackageManager mShadowPackageManager;

    @Before
    public void setUp() throws PackageManager.NameNotFoundException {
        MockitoAnnotations.initMocks(this);

        PackageManager pm = RuntimeEnvironment.application.getPackageManager();
        mShadowPackageManager = shadowOf(pm);

        Context context = mock(Context.class);

        when(mStore.getDelegatePackageName(eq(ORIGIN))).thenReturn(PACKAGE_NAME);

        mPermissionManager = new TrustedWebActivityPermissionManager(context, mStore, mPreserver);
    }

    @Test
    @Feature("TrustedWebActivities")
    public void locationDelegationNotEnabled_whenClientNotRequested() {
        setNoPermissionRequested();

        assertEquals(ContentSettingValues.DEFAULT,
                mPermissionManager.getPermission(ContentSettingsType.GEOLOCATION, ORIGIN));
        verifyPermissionNotUpdated();
    }

    @Test
    @Feature("TrustedWebActivities")
    public void locationPermissionAllowed_whenClientAllowed() {
        setClientLocationPermission(true);
        setStoredLocationPermission(false);

        assertEquals(ContentSettingValues.ALLOW,
                mPermissionManager.getPermission(ContentSettingsType.GEOLOCATION, ORIGIN));
        verifyLocationPermissionUpdated(true);
    }

    @Test
    @Feature("TrustedWebActivities")
    public void locationPermissionBlocked_whenClientBlocked() {
        setClientLocationPermission(false);
        setStoredLocationPermission(true);

        assertEquals(ContentSettingValues.BLOCK,
                mPermissionManager.getPermission(ContentSettingsType.GEOLOCATION, ORIGIN));
        verifyLocationPermissionUpdated(false);
    }

    @Test
    @Feature("TrustedWebActivities")
    public void locationPermissionAsk_whenNoPreviousPermission() {
        setClientLocationPermission(false);
        setStoredLocationPermission(null);

        assertEquals(ContentSettingValues.ASK,
                mPermissionManager.getPermission(ContentSettingsType.GEOLOCATION, ORIGIN));
        verifyPermissionNotUpdated();
    }

    private void setNoPermissionRequested() {
        PackageInfo packageInfo = new PackageInfo();
        packageInfo.packageName = PACKAGE_NAME;
        packageInfo.requestedPermissions = new String[0];
        packageInfo.requestedPermissionsFlags = new int[0];
        mShadowPackageManager.installPackage(packageInfo);
    }

    private void setClientLocationPermission(boolean enabled) {
        PackageInfo packageInfo = new PackageInfo();
        packageInfo.packageName = PACKAGE_NAME;
        packageInfo.requestedPermissions =
                new String[] {android.Manifest.permission.ACCESS_COARSE_LOCATION};
        packageInfo.requestedPermissionsFlags =
                new int[] {(enabled ? PackageInfo.REQUESTED_PERMISSION_GRANTED : 0)};
        mShadowPackageManager.installPackage(packageInfo);
    }

    private void setStoredLocationPermission(Boolean enabled) {
        when(mStore.arePermissionEnabled(eq(ContentSettingsType.GEOLOCATION), eq(ORIGIN)))
                .thenReturn(enabled);
    }

    private void verifyPermissionNotUpdated() {
        verify(mStore, never())
                .setStateForOrigin(any(), anyString(), anyString(), anyInt(), anyBoolean());
    }

    private void verifyLocationPermissionUpdated(boolean enabled) {
        verify(mStore).setStateForOrigin(eq(ORIGIN), eq(PACKAGE_NAME), anyString(),
                eq(ContentSettingsType.GEOLOCATION), eq(enabled));
    }
}
