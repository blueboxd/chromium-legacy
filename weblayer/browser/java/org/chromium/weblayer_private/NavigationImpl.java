// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.os.RemoteException;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.weblayer_private.aidl.APICallException;
import org.chromium.weblayer_private.aidl.IClientNavigation;
import org.chromium.weblayer_private.aidl.INavigation;
import org.chromium.weblayer_private.aidl.INavigationControllerClient;

import java.util.List;

@JNINamespace("weblayer")
public final class NavigationImpl extends INavigation.Stub {
    private final IClientNavigation mClientNavigation;
    // WARNING: NavigationImpl may outlive the native side, in which case this member is set to 0.
    private long mNativeNavigationImpl;

    public NavigationImpl(INavigationControllerClient client, long nativeNavigationImpl) {
        mNativeNavigationImpl = nativeNavigationImpl;
        try {
            mClientNavigation = client.createClientNavigation(this);
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
        nativeSetJavaNavigation(mNativeNavigationImpl);
    }

    public IClientNavigation getClientNavigation() {
        return mClientNavigation;
    }

    @Override
    public int getState() {
        throwIfNativeDestroyed();
        return nativeGetState(mNativeNavigationImpl);
    }

    @Override
    public String getUri() {
        throwIfNativeDestroyed();
        return nativeGetUri(mNativeNavigationImpl);
    }

    @Override
    public List<String> getRedirectChain() {
        throwIfNativeDestroyed();
        return nativeGetRedirectChain(mNativeNavigationImpl);
    }

    private void throwIfNativeDestroyed() {
        if (mNativeNavigationImpl == 0) {
            throw new IllegalStateException("Using Navigation after native destroyed");
        }
    }

    @CalledByNative
    private void onNativeDestroyed() {
        mNativeNavigationImpl = 0;
        // TODO: this should likely notify delegate in some way.
    }

    private native void nativeSetJavaNavigation(long nativeNavigationImpl);
    private native int nativeGetState(long nativeNavigationImpl);
    private native String nativeGetUri(long nativeNavigationImpl);
    private native List<String> nativeGetRedirectChain(long nativeNavigationImpl);
}
