// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.test;

import android.os.IBinder;

import org.chromium.base.annotations.UsedByReflection;
import org.chromium.device.geolocation.LocationProviderOverrider;
import org.chromium.device.geolocation.MockLocationProvider;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.weblayer_private.test_interfaces.ITestWebLayer;

/**
 * Root implementation class for TestWebLayer.
 */
@UsedByReflection("WebLayer")
public final class TestWebLayerImpl extends ITestWebLayer.Stub {
    private MockLocationProvider mMockLocationProvider;

    @UsedByReflection("WebLayer")
    public static IBinder create() {
        return new TestWebLayerImpl();
    }

    private TestWebLayerImpl() {}

    @Override
    public boolean isNetworkChangeAutoDetectOn() {
        return NetworkChangeNotifier.getAutoDetectorForTest() != null;
    }

    @Override
    public void setMockLocationProvider(boolean enable) {
        if (enable) {
            mMockLocationProvider = new MockLocationProvider();
            LocationProviderOverrider.setLocationProviderImpl(mMockLocationProvider);
        } else if (mMockLocationProvider != null) {
            mMockLocationProvider.stop();
            mMockLocationProvider.stopUpdates();
        }
    }

    @Override
    public boolean isMockLocationProviderRunning() {
        return mMockLocationProvider.isRunning();
    }
}