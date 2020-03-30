// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.webkit.ValueCallback;

import org.chromium.base.Callback;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.weblayer_private.interfaces.ICookieManager;
import org.chromium.weblayer_private.interfaces.IObjectWrapper;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;
import org.chromium.weblayer_private.interfaces.StrictModeWorkaround;

/**
 * Implementation of ICookieManager.
 */
@JNINamespace("weblayer")
public final class CookieManagerImpl extends ICookieManager.Stub {
    private long mNativeCookieManager;

    CookieManagerImpl(long nativeCookieManager) {
        mNativeCookieManager = nativeCookieManager;
    }

    @Override
    public boolean setCookie(String url, String value, IObjectWrapper callback) {
        StrictModeWorkaround.apply();
        ValueCallback<Boolean> valueCallback =
                (ValueCallback<Boolean>) ObjectWrapper.unwrap(callback, ValueCallback.class);
        Callback<Boolean> baseCallback = (Boolean result) -> valueCallback.onReceiveValue(result);
        return CookieManagerImplJni.get().setCookie(mNativeCookieManager, url, value, baseCallback);
    }

    @Override
    public void getCookie(String url, IObjectWrapper callback) {
        StrictModeWorkaround.apply();
        ValueCallback<String> valueCallback =
                (ValueCallback<String>) ObjectWrapper.unwrap(callback, ValueCallback.class);
        Callback<String> baseCallback = (String result) -> valueCallback.onReceiveValue(result);
        CookieManagerImplJni.get().getCookie(mNativeCookieManager, url, baseCallback);
    }

    @NativeMethods
    interface Natives {
        boolean setCookie(
                long nativeCookieManagerImpl, String url, String value, Callback<Boolean> callback);
        void getCookie(long nativeCookieManagerImpl, String url, Callback<String> callback);
    }
}
