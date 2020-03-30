// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.doReturn;

import android.content.Context;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.net.ConnectionType;

/** Unit tests for {@link DeviceConditions} class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config
public class DeviceConditionsTest {
    @Mock
    private Context mContext;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    @Test
    public void testDefaultConstructor() {
        DeviceConditions deviceConditions = new DeviceConditions();

        // Default values are equivalent to most restrictive conditions.
        assertEquals(ConnectionType.CONNECTION_UNKNOWN, deviceConditions.getNetConnectionType());
        assertTrue(deviceConditions.isActiveNetworkMetered());
        assertTrue(deviceConditions.isInPowerSaveMode());
        assertTrue(deviceConditions.isScreenOnAndUnlocked());
        assertFalse(deviceConditions.isPowerConnected());
        assertEquals(0, deviceConditions.getBatteryPercentage());
    }

    @Test
    public void testNoNpeOnNullBatteryStatus() {
        doReturn(null).when(mContext).registerReceiver(isNull(), any());

        DeviceConditions deviceConditions = DeviceConditions.getCurrent(mContext);

        assertNotNull("Device conditions should not be null.", deviceConditions);
        assertEquals(ConnectionType.CONNECTION_UNKNOWN, deviceConditions.getNetConnectionType());
        assertTrue(deviceConditions.isActiveNetworkMetered());
        assertTrue(deviceConditions.isInPowerSaveMode());
        assertTrue(deviceConditions.isScreenOnAndUnlocked());
        assertFalse(deviceConditions.isPowerConnected());
        assertEquals(0, deviceConditions.getBatteryPercentage());
    }
}