// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.os.RemoteException;
import android.webkit.ValueCallback;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.weblayer_private.aidl.APICallException;
import org.chromium.weblayer_private.aidl.IFullscreenDelegateClient;
import org.chromium.weblayer_private.aidl.ObjectWrapper;

/**
 * Owns the c++ FullscreenDelegateProxy class, which is responsible for forwarding all
 * FullscreenDelegate delegate calls to this class, which in turn forwards to the
 * FullscreenDelegateClient.
 */
@JNINamespace("weblayer")
public final class FullscreenDelegateProxy {
    private long mNativeFullscreenDelegateProxy;
    private IFullscreenDelegateClient mClient;

    FullscreenDelegateProxy(long browserController, IFullscreenDelegateClient client) {
        assert client != null;
        mClient = client;
        mNativeFullscreenDelegateProxy =
                FullscreenDelegateProxyJni.get().createFullscreenDelegateProxy(
                        this, browserController);
    }

    public void setClient(IFullscreenDelegateClient client) {
        assert client != null;
        mClient = client;
    }

    public void destroy() {
        FullscreenDelegateProxyJni.get().deleteFullscreenDelegateProxy(
                mNativeFullscreenDelegateProxy);
        mNativeFullscreenDelegateProxy = 0;
    }

    @CalledByNative
    private void enterFullscreen() {
        try {
            ValueCallback<Void> exitFullscreenCallback = new ValueCallback<Void>() {
                @Override
                public void onReceiveValue(Void result) {
                    if (mNativeFullscreenDelegateProxy == 0) {
                        throw new IllegalStateException("Called after destroy()");
                    }
                    FullscreenDelegateProxyJni.get().doExitFullscreen(
                            mNativeFullscreenDelegateProxy, FullscreenDelegateProxy.this);
                }
            };
            mClient.enterFullscreen(ObjectWrapper.wrap(exitFullscreenCallback));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @CalledByNative
    private void exitFullscreen() {
        try {
            mClient.exitFullscreen();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @NativeMethods
    interface Natives {
        long createFullscreenDelegateProxy(FullscreenDelegateProxy proxy, long browserController);
        void deleteFullscreenDelegateProxy(long proxy);
        void doExitFullscreen(long nativeFullscreenDelegateProxy, FullscreenDelegateProxy proxy);
    }
}
