// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import org.chromium.base.annotations.JNINamespace;

@JNINamespace("weblayer")
public class Profile {
    private long mNativeProfile;

    public static Profile create(String path) {
        return new Profile(nativeInit(path));
    }

    private Profile(long nativeProfile) {
        mNativeProfile = nativeProfile;
    }

    public void destroy() {}

    /* package */ long getNativeProfile() {
        return mNativeProfile;
    }

    private static native long nativeInit(String path);
}
