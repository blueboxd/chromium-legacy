// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.os.RemoteException;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
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
        NavigationImplJni.get().setJavaNavigation(mNativeNavigationImpl, NavigationImpl.this);
    }

    public IClientNavigation getClientNavigation() {
        return mClientNavigation;
    }

    @Override
    public int getState() {
        throwIfNativeDestroyed();
        return NavigationImplJni.get().getState(mNativeNavigationImpl, NavigationImpl.this);
    }

    @Override
    public String getUri() {
        throwIfNativeDestroyed();
        return NavigationImplJni.get().getUri(mNativeNavigationImpl, NavigationImpl.this);
    }

    @Override
    public List<String> getRedirectChain() {
        throwIfNativeDestroyed();
        return NavigationImplJni.get().getRedirectChain(mNativeNavigationImpl, NavigationImpl.this);
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

    @NativeMethods
    interface Natives {
        void setJavaNavigation(long nativeNavigationImpl, NavigationImpl caller);
        int getState(long nativeNavigationImpl, NavigationImpl caller);
        String getUri(long nativeNavigationImpl, NavigationImpl caller);
        List<String> getRedirectChain(long nativeNavigationImpl, NavigationImpl caller);
    }
}
