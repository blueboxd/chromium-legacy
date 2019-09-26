// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.content.Context;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.weblayer_private.aidl.IBrowserFragmentController;
import org.chromium.weblayer_private.aidl.IObjectWrapper;
import org.chromium.weblayer_private.aidl.IProfile;
import org.chromium.weblayer_private.aidl.IRemoteFragmentClient;
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
    public IBrowserFragmentController createBrowserFragmentController(IRemoteFragmentClient
            fragmentClient,
            IObjectWrapper context) {
        BrowserControllerImpl browserController = new BrowserControllerImpl(
                    ObjectWrapper.unwrap(context, Context.class), this);
        return new BrowserFragmentControllerImpl(browserController, fragmentClient);
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
