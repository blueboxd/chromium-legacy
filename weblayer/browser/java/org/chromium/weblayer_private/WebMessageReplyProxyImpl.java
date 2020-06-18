// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.os.RemoteException;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.weblayer_private.interfaces.APICallException;
import org.chromium.weblayer_private.interfaces.IWebMessageCallbackClient;
import org.chromium.weblayer_private.interfaces.IWebMessageReplyProxy;

/**
 * WebMessageReplyProxyImpl is responsible for both sending and receiving WebMessages.
 */
@JNINamespace("weblayer")
public final class WebMessageReplyProxyImpl extends IWebMessageReplyProxy.Stub {
    private long mNativeWebMessageReplyProxyImpl;
    private final IWebMessageCallbackClient mClient;
    // Unique id (scoped to the call to Tab.registerWebMessageCallback()) for this proxy. This is
    // sent over AIDL.
    private final int mId;

    private WebMessageReplyProxyImpl(long nativeWebMessageReplyProxyImpl, int id,
            IWebMessageCallbackClient client, boolean isMainFrame, String sourceOrigin) {
        mNativeWebMessageReplyProxyImpl = nativeWebMessageReplyProxyImpl;
        mClient = client;
        mId = id;
        try {
            client.onNewReplyProxy(this, mId, isMainFrame, sourceOrigin);
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @CalledByNative
    private static WebMessageReplyProxyImpl create(long nativeWebMessageReplyProxyImpl, int id,
            IWebMessageCallbackClient client, boolean isMainFrame, String sourceOrigin) {
        return new WebMessageReplyProxyImpl(
                nativeWebMessageReplyProxyImpl, id, client, isMainFrame, sourceOrigin);
    }

    @CalledByNative
    private void onNativeDestroyed() {
        mNativeWebMessageReplyProxyImpl = 0;
        try {
            mClient.onReplyProxyDestroyed(mId);
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @CalledByNative
    private void onPostMessage(String message) {
        try {
            mClient.onPostMessage(mId, message);
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    @Override
    public void postMessage(String message) {
        if (mNativeWebMessageReplyProxyImpl != 0) {
            WebMessageReplyProxyImplJni.get().postMessage(mNativeWebMessageReplyProxyImpl, message);
        }
    }

    @NativeMethods
    interface Natives {
        void postMessage(long nativeWebMessageReplyProxyImpl, String message);
    }
}
