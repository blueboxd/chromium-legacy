// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.permissiondelegation;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.content.ComponentName;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;

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
import org.chromium.chrome.browser.browserservices.TrustedWebActivityClient;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.embedder_support.util.Origin;

/**
 * Tests for {@link LocationPermissionUpdater}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures(ChromeFeatureList.TRUSTED_WEB_ACTIVITY_LOCATION_DELEGATION)
public class LocationPermissionUpdaterTest {
    private static final Origin ORIGIN = Origin.create("https://www.website.com");
    private static final String PACKAGE_NAME = "com.package.name";
    private static final String OTHER_PACKAGE_NAME = "com.other.package.name";

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock
    public TrustedWebActivityPermissionManager mPermissionManager;
    @Mock
    public TrustedWebActivityClient mTrustedWebActivityClient;

    private LocationPermissionUpdater mLocationPermissionUpdater;
    private ShadowPackageManager mShadowPackageManager;

    private boolean mLocationEnabled;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        PackageManager pm = RuntimeEnvironment.application.getPackageManager();
        mShadowPackageManager = shadowOf(pm);
        mLocationPermissionUpdater =
                new LocationPermissionUpdater(mPermissionManager, mTrustedWebActivityClient);
        installBrowsableIntentHandler(ORIGIN, PACKAGE_NAME);
    }

    @Test
    @Feature("TrustedWebActivities")
    public void disablesLocation_whenClientLocationAreDisabled() {
        installTrustedWebActivityService(ORIGIN, PACKAGE_NAME);
        setLocationEnabledForClient(false);

        mLocationPermissionUpdater.checkPermission(ORIGIN, 0 /*callback*/);

        verifyPermissionUpdated(false);
    }

    @Test
    @Feature("TrustedWebActivities")
    public void enablesLocation_whenClientLocationAreEnabled() {
        installTrustedWebActivityService(ORIGIN, PACKAGE_NAME);
        setLocationEnabledForClient(true);

        mLocationPermissionUpdater.checkPermission(ORIGIN, 0 /*callback*/);

        verifyPermissionUpdated(true);
    }

    @Test
    @Feature("TrustedWebActivities")
    public void updatesPermission_onSubsequentCalls() {
        installTrustedWebActivityService(ORIGIN, PACKAGE_NAME);
        setLocationEnabledForClient(true);
        mLocationPermissionUpdater.checkPermission(ORIGIN, 0 /*callback*/);
        verifyPermissionUpdated(true);

        setLocationEnabledForClient(false);
        mLocationPermissionUpdater.checkPermission(ORIGIN, 0 /*callback*/);
        verifyPermissionUpdated(false);
    }

    @Test
    @Feature("TrustedWebActivities")
    public void updatesPermission_onNewClient() {
        installTrustedWebActivityService(ORIGIN, PACKAGE_NAME);
        setLocationEnabledForClient(true);
        mLocationPermissionUpdater.checkPermission(ORIGIN, 0 /*callback*/);
        verifyPermissionUpdated(true);

        installBrowsableIntentHandler(ORIGIN, OTHER_PACKAGE_NAME);
        installTrustedWebActivityService(ORIGIN, OTHER_PACKAGE_NAME);
        setLocationEnabledForClient(false);
        mLocationPermissionUpdater.checkPermission(ORIGIN, 0 /*callback*/);
        verifyPermissionUpdated(OTHER_PACKAGE_NAME, false);
    }

    @Test
    @Feature("TrustedWebActivities")
    public void unregisters_onClientUninstall() {
        installTrustedWebActivityService(ORIGIN, PACKAGE_NAME);
        setLocationEnabledForClient(true);

        mLocationPermissionUpdater.checkPermission(ORIGIN, 0 /*callback*/);

        uninstallTrustedWebActivityService(ORIGIN);
        mLocationPermissionUpdater.onClientAppUninstalled(ORIGIN);

        verifyPermissionReset();
    }

    /** "Installs" the given package to handle intents for that origin. */
    private void installBrowsableIntentHandler(Origin origin, String packageName) {
        Intent intent = new Intent();
        intent.setPackage(packageName);
        intent.setData(origin.uri());
        intent.setAction(Intent.ACTION_VIEW);
        intent.addCategory(Intent.CATEGORY_BROWSABLE);

        mShadowPackageManager.addResolveInfoForIntent(intent, new ResolveInfo());
    }

    /** "Installs" a Trusted Web Activity Service for the origin. */
    @SuppressWarnings("unchecked")
    private void installTrustedWebActivityService(Origin origin, String packageName) {
        when(mTrustedWebActivityClient.checkLocationPermission(eq(origin), any()))
                .thenAnswer(invocation -> {
                    TrustedWebActivityClient.PermissionCheckCallback callback =
                            ((TrustedWebActivityClient.PermissionCheckCallback)
                                            invocation.getArgument(1));
                    callback.onPermissionCheck(
                            new ComponentName(packageName, "FakeClass"), mLocationEnabled);
                    return true;
                });
    }

    private void uninstallTrustedWebActivityService(Origin origin) {
        when(mTrustedWebActivityClient.checkLocationPermission(eq(origin), any()))
                .thenReturn(false);
    }

    private void setLocationEnabledForClient(boolean enabled) {
        mLocationEnabled = enabled;
    }

    private void verifyPermissionUpdated(boolean enabled) {
        verifyPermissionUpdated(PACKAGE_NAME, enabled);
    }

    private void verifyPermissionUpdated(String packageName, boolean enabled) {
        verify(mPermissionManager)
                .updatePermission(eq(ORIGIN), eq(packageName), eq(ContentSettingsType.GEOLOCATION),
                        eq(enabled));
    }

    private void verifyPermissionReset() {
        verify(mPermissionManager)
                .resetStoredPermission(eq(ORIGIN), eq(ContentSettingsType.GEOLOCATION));
    }

    private void verifyPermissionNotReset() {
        verify(mPermissionManager, never())
                .resetStoredPermission(eq(ORIGIN), eq(ContentSettingsType.GEOLOCATION));
    }
}
