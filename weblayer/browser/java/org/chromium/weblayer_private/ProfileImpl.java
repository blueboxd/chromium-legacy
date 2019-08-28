// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import org.chromium.base.annotations.JNINamespace;

@JNINamespace("weblayer")
public final class ProfileImpl {
    private long mNativeProfile;

    public ProfileImpl(String path) {
        mNativeProfile = nativeCreateProfile(path);
    }

    public void destroy() {
        nativeDeleteProfile(mNativeProfile);
        mNativeProfile = 0;
    }

    public void clearBrowsingData() {
        nativeClearBrowsingData(mNativeProfile);
    }

    long getNativeProfile() {
        return mNativeProfile;
    }

    private static native long nativeCreateProfile(String path);

    private static native void nativeDeleteProfile(long profile);

    private static native void nativeClearBrowsingData(long nativeProfileImpl);
}
