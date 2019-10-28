// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.weblayer_private.aidl.IObjectWrapper;
import org.chromium.weblayer_private.aidl.IProfile;
import org.chromium.weblayer_private.aidl.ObjectWrapper;

import java.util.ArrayList;
import java.util.List;

@JNINamespace("weblayer")
public final class ProfileImpl extends IProfile.Stub {
    private final List<Runnable> mCurrentClearDataCallbacks = new ArrayList<>();
    private final List<Runnable> mPendingClearDataCallbacks = new ArrayList<>();
    private long mNativeProfile;
    private Runnable mOnDestroyCallback;

    ProfileImpl(String path, Runnable onDestroyCallback) {
        mNativeProfile = ProfileImplJni.get().createProfile(this, path);
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
    public void clearBrowsingData(IObjectWrapper completionCallback) {
        Runnable callback = ObjectWrapper.unwrap(completionCallback, Runnable.class);
        if (!mCurrentClearDataCallbacks.isEmpty()) {
            // Already running a clear data job. Will have to re-run the job once it's completed,
            // because new data may have been stored.
            mPendingClearDataCallbacks.add(callback);
            return;
        }
        mCurrentClearDataCallbacks.add(callback);
        ProfileImplJni.get().clearBrowsingData(mNativeProfile);
    }

    @CalledByNative
    private void onBrowsingDataCleared() {
        for (Runnable callback : mCurrentClearDataCallbacks) {
            callback.run();
        }
        mCurrentClearDataCallbacks.clear();
        if (!mPendingClearDataCallbacks.isEmpty()) {
            mCurrentClearDataCallbacks.addAll(mPendingClearDataCallbacks);
            mPendingClearDataCallbacks.clear();
            ProfileImplJni.get().clearBrowsingData(mNativeProfile);
        }
    }

    long getNativeProfile() {
        return mNativeProfile;
    }

    @NativeMethods
    interface Natives {
        long createProfile(ProfileImpl caller, String path);
        void deleteProfile(long profile);
        void clearBrowsingData(long nativeProfileImpl);
    }
}
