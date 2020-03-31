// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.content.Context;
import android.content.pm.PackageManager;
import android.os.IBinder;
import android.os.RemoteException;
import android.util.AndroidRuntimeException;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.weblayer_private.test_interfaces.ITestWebLayer;

/**
 * TestWebLayer is responsible for passing messages over a test only AIDL to the
 * WebLayer implementation.
 */
public final class TestWebLayer {
    @Nullable
    private ITestWebLayer mITestWebLayer;

    @Nullable
    private static TestWebLayer sInstance;

    public static TestWebLayer getTestWebLayer(@NonNull Context appContext) {
        if (sInstance == null) sInstance = new TestWebLayer(appContext);
        return sInstance;
    }

    private TestWebLayer(@NonNull Context appContext) {
        ClassLoader remoteClassLoader;
        try {
            remoteClassLoader = WebLayer.getOrCreateRemoteContext(appContext).getClassLoader();
            Class TestWebLayerClass = remoteClassLoader.loadClass(
                    "org.chromium.weblayer_private.test.TestWebLayerImpl");
            mITestWebLayer = ITestWebLayer.Stub.asInterface(
                    (IBinder) TestWebLayerClass.getMethod("create").invoke(null));
        } catch (PackageManager.NameNotFoundException | ReflectiveOperationException e) {
            throw new AndroidRuntimeException(e);
        }
    }

    public boolean isNetworkChangeAutoDetectOn() throws RemoteException {
        return mITestWebLayer.isNetworkChangeAutoDetectOn();
    }

    public static Context getRemoteContext(@NonNull Context appContext) {
        try {
            return WebLayer.getOrCreateRemoteContext(appContext);
        } catch (PackageManager.NameNotFoundException | ReflectiveOperationException e) {
            throw new AndroidRuntimeException(e);
        }
    }

    public void setMockLocationProvider(boolean enabled) throws RemoteException {
        mITestWebLayer.setMockLocationProvider(enabled);
    }

    public boolean isMockLocationProviderRunning() throws RemoteException {
        return mITestWebLayer.isMockLocationProviderRunning();
    }

    public boolean isPermissionDialogShown() throws RemoteException {
        return mITestWebLayer.isPermissionDialogShown();
    }

    public void clickPermissionDialogButton(boolean allow) throws RemoteException {
        mITestWebLayer.clickPermissionDialogButton(allow);
    }
}
