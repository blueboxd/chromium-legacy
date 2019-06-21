// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profiles;

import org.chromium.base.annotations.CalledByNative;

/**
 * Wrapper that allows passing a ProfileKey reference around in the Java layer.
 */
public class ProfileKey {
    /** Whether this wrapper corresponds to an off the record ProfileKey. */
    private final boolean mIsOffTheRecord;

    /** Pointer to the Native-side ProfileKey. */
    private long mNativeProfileKeyAndroid;

    private ProfileKey(long nativeProfileKeyAndroid) {
        mNativeProfileKeyAndroid = nativeProfileKeyAndroid;
        mIsOffTheRecord = nativeIsOffTheRecord(mNativeProfileKeyAndroid);
    }

    public static ProfileKey getLastUsedProfileKey() {
        // TODO(mheikal): Assert at least reduced mode is started when https://crbug.com/973241 is
        // fixed.
        return (ProfileKey) nativeGetLastUsedProfileKey();
    }

    public ProfileKey getOriginalKey() {
        return (ProfileKey) nativeGetOriginalKey(mNativeProfileKeyAndroid);
    }

    public boolean isOffTheRecord() {
        return mIsOffTheRecord;
    }

    @CalledByNative
    private static ProfileKey create(long nativeProfileKeyAndroid) {
        return new ProfileKey(nativeProfileKeyAndroid);
    }

    @CalledByNative
    private void onNativeDestroyed() {
        mNativeProfileKeyAndroid = 0;
    }

    @CalledByNative
    private long getNativePointer() {
        return mNativeProfileKeyAndroid;
    }

    private static native Object nativeGetLastUsedProfileKey();
    private native Object nativeGetOriginalKey(long nativeProfileKeyAndroid);
    private native boolean nativeIsOffTheRecord(long nativeProfileKeyAndroid);
}
