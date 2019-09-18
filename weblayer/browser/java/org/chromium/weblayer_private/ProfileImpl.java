// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.content.Context;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.weblayer_private.aidl.IBrowserController;
import org.chromium.weblayer_private.aidl.IObjectWrapper;
import org.chromium.weblayer_private.aidl.IProfile;
import org.chromium.weblayer_private.aidl.ObjectWrapper;

@JNINamespace("weblayer")
public final class ProfileImpl extends IProfile.Stub {
    private long mNativeProfile;

    public ProfileImpl(String path) {
        mNativeProfile = nativeCreateProfile(path);
    }

    @Override
    public void destroy() {
        nativeDeleteProfile(mNativeProfile);
        mNativeProfile = 0;
    }

    @Override
    public void clearBrowsingData() {
        nativeClearBrowsingData(mNativeProfile);
    }

    @Override
    public IBrowserController createBrowserController(
            IObjectWrapper clientContext, IObjectWrapper implContext) {
        return new BrowserControllerImpl(ObjectWrapper.unwrap(clientContext, Context.class),
                ObjectWrapper.unwrap(implContext, Context.class), this);
    }

    long getNativeProfile() {
        return mNativeProfile;
    }

    private static native long nativeCreateProfile(String path);

    private static native void nativeDeleteProfile(long profile);

    private static native void nativeClearBrowsingData(long nativeProfileImpl);
}
