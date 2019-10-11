// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.weblayer_private.aidl.IProfile;

@JNINamespace("weblayer")
public final class ProfileImpl extends IProfile.Stub {
    private long mNativeProfile;
    private Runnable mOnDestroyCallback;

    ProfileImpl(String path, Runnable onDestroyCallback) {
        mNativeProfile = ProfileImplJni.get().createProfile(path);
        mOnDestroyCallback = onDestroyCallback;
    }

    @Override
    public void destroy() {
        ProfileImplJni.get().deleteProfile(mNativeProfile);
        mNativeProfile = 0;
        mOnDestroyCallback.run();
        mOnDestroyCallback = null;
    }

    @Override
    public void clearBrowsingData() {
        ProfileImplJni.get().clearBrowsingData(mNativeProfile);
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
