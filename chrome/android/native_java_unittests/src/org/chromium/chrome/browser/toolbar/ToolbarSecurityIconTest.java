// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.toolbar;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.when;

import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.CalledByNativeJavaTest;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.omnibox.LocationBarLayout;
import org.chromium.chrome.browser.tab.TabImpl;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.components.security_state.SecurityStateModel;
import org.chromium.components.security_state.SecurityStateModelJni;

/**
 * Unit tests for {@link LocationBarLayout} class.
 */
public final class ToolbarSecurityIconTest {
    private static final boolean IS_SMALL_DEVICE = true;
    private static final boolean IS_OFFLINE_PAGE = true;
    private static final boolean IS_PREVIEW = true;
    private static final int[] SECURITY_LEVELS = new int[] {ConnectionSecurityLevel.NONE,
            ConnectionSecurityLevel.WARNING, ConnectionSecurityLevel.DANGEROUS,
            ConnectionSecurityLevel.SECURE, ConnectionSecurityLevel.EV_SECURE};

    @Mock
    private TabImpl mTab;
    @Mock
    SecurityStateModel.Natives mSecurityStateMocks;

    @Mock
    private LocationBarModel mLocationBarModel;

    @CalledByNative
    private ToolbarSecurityIconTest() {}

    @CalledByNative
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        SecurityStateModelJni.TEST_HOOKS.setInstanceForTesting(mSecurityStateMocks);
        mLocationBarModel = spy(new LocationBarModel(ContextUtils.getApplicationContext()));
        mLocationBarModel.initializeWithNative();
    }

    @CalledByNative
    public void tearDown() {
        SecurityStateModelJni.TEST_HOOKS.setInstanceForTesting(null);
    }

    @CalledByNativeJavaTest
    public void testGetSecurityLevel() {
        assertEquals(ConnectionSecurityLevel.NONE,
                mLocationBarModel.getSecurityLevel(null, !IS_OFFLINE_PAGE, null));
        assertEquals(ConnectionSecurityLevel.NONE,
                mLocationBarModel.getSecurityLevel(null, IS_OFFLINE_PAGE, null));
        assertEquals(ConnectionSecurityLevel.NONE,
                mLocationBarModel.getSecurityLevel(mTab, IS_OFFLINE_PAGE, null));

        for (int securityLevel : SECURITY_LEVELS) {
            doReturn(securityLevel).when(mLocationBarModel).getSecurityLevelFromStateModel(any());
            assertEquals("Wrong security level returned for " + securityLevel, securityLevel,
                    mLocationBarModel.getSecurityLevel(mTab, !IS_OFFLINE_PAGE, null));
        }

        doReturn(ConnectionSecurityLevel.SECURE)
                .when(mLocationBarModel)
                .getSecurityLevelFromStateModel(any());
        assertEquals("Wrong security level returned for HTTPS publisher URL",
                ConnectionSecurityLevel.SECURE,
                mLocationBarModel.getSecurityLevel(mTab, !IS_OFFLINE_PAGE, "https://example.com"));
        assertEquals("Wrong security level returned for HTTP publisher URL",
                ConnectionSecurityLevel.WARNING,
                mLocationBarModel.getSecurityLevel(mTab, !IS_OFFLINE_PAGE, "http://example.com"));

        doReturn(ConnectionSecurityLevel.DANGEROUS)
                .when(mLocationBarModel)
                .getSecurityLevelFromStateModel(any());
        assertEquals("Wrong security level returned for publisher URL on insecure page",
                ConnectionSecurityLevel.DANGEROUS,
                mLocationBarModel.getSecurityLevel(mTab, !IS_OFFLINE_PAGE, null));
    }

    @CalledByNativeJavaTest
    public void testGetSecurityIconResource() {
        when(mSecurityStateMocks.shouldShowDangerTriangleForWarningLevel()).thenReturn(false);
        for (int securityLevel : SECURITY_LEVELS) {
            assertEquals("Wrong phone resource for security level " + securityLevel,
                    R.drawable.ic_offline_pin_24dp,
                    mLocationBarModel.getSecurityIconResource(
                            securityLevel, IS_SMALL_DEVICE, IS_OFFLINE_PAGE, !IS_PREVIEW));
            assertEquals("Wrong tablet resource for security level " + securityLevel,
                    R.drawable.ic_offline_pin_24dp,
                    mLocationBarModel.getSecurityIconResource(
                            securityLevel, !IS_SMALL_DEVICE, IS_OFFLINE_PAGE, !IS_PREVIEW));
            assertEquals("Wrong phone resource for security level " + securityLevel,
                    R.drawable.preview_pin_round,
                    mLocationBarModel.getSecurityIconResource(
                            securityLevel, IS_SMALL_DEVICE, !IS_OFFLINE_PAGE, IS_PREVIEW));
            assertEquals("Wrong tablet resource for security level " + securityLevel,
                    R.drawable.preview_pin_round,
                    mLocationBarModel.getSecurityIconResource(
                            securityLevel, !IS_SMALL_DEVICE, !IS_OFFLINE_PAGE, IS_PREVIEW));
        }

        assertEquals(0,
                mLocationBarModel.getSecurityIconResource(ConnectionSecurityLevel.NONE,
                        IS_SMALL_DEVICE, !IS_OFFLINE_PAGE, !IS_PREVIEW));
        assertEquals(R.drawable.omnibox_info,
                mLocationBarModel.getSecurityIconResource(ConnectionSecurityLevel.NONE,
                        !IS_SMALL_DEVICE, !IS_OFFLINE_PAGE, !IS_PREVIEW));

        assertEquals(R.drawable.omnibox_info,
                mLocationBarModel.getSecurityIconResource(ConnectionSecurityLevel.WARNING,
                        IS_SMALL_DEVICE, !IS_OFFLINE_PAGE, !IS_PREVIEW));
        assertEquals(R.drawable.omnibox_info,
                mLocationBarModel.getSecurityIconResource(ConnectionSecurityLevel.WARNING,
                        !IS_SMALL_DEVICE, !IS_OFFLINE_PAGE, !IS_PREVIEW));

        assertEquals(R.drawable.omnibox_not_secure_warning,
                mLocationBarModel.getSecurityIconResource(ConnectionSecurityLevel.DANGEROUS,
                        IS_SMALL_DEVICE, !IS_OFFLINE_PAGE, !IS_PREVIEW));
        assertEquals(R.drawable.omnibox_not_secure_warning,
                mLocationBarModel.getSecurityIconResource(ConnectionSecurityLevel.DANGEROUS,
                        !IS_SMALL_DEVICE, !IS_OFFLINE_PAGE, !IS_PREVIEW));

        assertEquals(R.drawable.omnibox_https_valid,
                mLocationBarModel.getSecurityIconResource(
                        ConnectionSecurityLevel.SECURE_WITH_POLICY_INSTALLED_CERT, IS_SMALL_DEVICE,
                        !IS_OFFLINE_PAGE, !IS_PREVIEW));
        assertEquals(R.drawable.omnibox_https_valid,
                mLocationBarModel.getSecurityIconResource(
                        ConnectionSecurityLevel.SECURE_WITH_POLICY_INSTALLED_CERT, !IS_SMALL_DEVICE,
                        !IS_OFFLINE_PAGE, !IS_PREVIEW));

        assertEquals(R.drawable.omnibox_https_valid,
                mLocationBarModel.getSecurityIconResource(ConnectionSecurityLevel.SECURE,
                        IS_SMALL_DEVICE, !IS_OFFLINE_PAGE, !IS_PREVIEW));
        assertEquals(R.drawable.omnibox_https_valid,
                mLocationBarModel.getSecurityIconResource(ConnectionSecurityLevel.SECURE,
                        !IS_SMALL_DEVICE, !IS_OFFLINE_PAGE, !IS_PREVIEW));

        assertEquals(R.drawable.omnibox_https_valid,
                mLocationBarModel.getSecurityIconResource(ConnectionSecurityLevel.EV_SECURE,
                        IS_SMALL_DEVICE, !IS_OFFLINE_PAGE, !IS_PREVIEW));
        assertEquals(R.drawable.omnibox_https_valid,
                mLocationBarModel.getSecurityIconResource(ConnectionSecurityLevel.EV_SECURE,
                        !IS_SMALL_DEVICE, !IS_OFFLINE_PAGE, !IS_PREVIEW));
    }

    @CalledByNativeJavaTest
    public void testGetSecurityIconResourceForMarkHttpAsDangerWarning() {
        when(mSecurityStateMocks.shouldShowDangerTriangleForWarningLevel()).thenReturn(false);
        assertEquals(0,
                mLocationBarModel.getSecurityIconResource(ConnectionSecurityLevel.NONE,
                        IS_SMALL_DEVICE, !IS_OFFLINE_PAGE, !IS_PREVIEW));
        assertEquals(R.drawable.omnibox_info,
                mLocationBarModel.getSecurityIconResource(ConnectionSecurityLevel.NONE,
                        !IS_SMALL_DEVICE, !IS_OFFLINE_PAGE, !IS_PREVIEW));

        when(mSecurityStateMocks.shouldShowDangerTriangleForWarningLevel()).thenReturn(true);
        assertEquals(R.drawable.omnibox_not_secure_warning,
                mLocationBarModel.getSecurityIconResource(ConnectionSecurityLevel.WARNING,
                        IS_SMALL_DEVICE, !IS_OFFLINE_PAGE, !IS_PREVIEW));
        assertEquals(R.drawable.omnibox_not_secure_warning,
                mLocationBarModel.getSecurityIconResource(ConnectionSecurityLevel.WARNING,
                        !IS_SMALL_DEVICE, !IS_OFFLINE_PAGE, !IS_PREVIEW));
    }
}
