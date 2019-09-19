// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.content.Context;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.weblayer_private.aidl.IBrowserController;
import org.chromium.weblayer_private.aidl.IObjectWrapper;
import org.chromium.weblayer_private.aidl.IProfile;
import org.chromium.weblayer_private.aidl.ObjectWrapper;

@JNINamespace("weblayer")
public final class ProfileImpl extends IProfile.Stub {
    private long mNativeProfile;

    public ProfileImpl(String path) {
        mNativeProfile = ProfileImplJni.get().createProfile(path);
    }

    @Override
    public void destroy() {
        ProfileImplJni.get().deleteProfile(mNativeProfile);
        mNativeProfile = 0;
    }

    @Override
    public void clearBrowsingData() {
        ProfileImplJni.get().clearBrowsingData(mNativeProfile);
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

    @NativeMethods
    interface Natives {
        long createProfile(String path);
        void deleteProfile(long profile);
        void clearBrowsingData(long nativeProfileImpl);
    }
}
