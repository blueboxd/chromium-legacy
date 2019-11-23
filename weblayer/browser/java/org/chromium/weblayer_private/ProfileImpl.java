// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import androidx.annotation.NonNull;

import org.chromium.base.CollectionUtil;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.weblayer_private.interfaces.BrowsingDataType;
import org.chromium.weblayer_private.interfaces.IObjectWrapper;
import org.chromium.weblayer_private.interfaces.IProfile;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;
import org.chromium.weblayer_private.interfaces.StrictModeWorkaround;

import java.util.ArrayList;
import java.util.List;

/**
 * Implementation of IProfile.
 */
@JNINamespace("weblayer")
public final class ProfileImpl extends IProfile.Stub {
    private final String mName;
    private long mNativeProfile;
    private Runnable mOnDestroyCallback;

    ProfileImpl(String name, Runnable onDestroyCallback) {
        if (!name.matches("^\\w*$")) {
            throw new IllegalArgumentException("Name can only contain words: " + name);
        }
        mName = name;
        mNativeProfile = ProfileImplJni.get().createProfile(name);
        mOnDestroyCallback = onDestroyCallback;
    }

    @Override
    public void destroy() {
        StrictModeWorkaround.apply();
        ProfileImplJni.get().deleteProfile(mNativeProfile);
        mNativeProfile = 0;
        mOnDestroyCallback.run();
        mOnDestroyCallback = null;
    }

    @Override
    public String getName() {
        StrictModeWorkaround.apply();
        return mName;
    }

    @Override
    public void clearBrowsingData(@NonNull @BrowsingDataType int[] dataTypes, long fromMillis,
            long toMillis, @NonNull IObjectWrapper completionCallback) {
        StrictModeWorkaround.apply();
        Runnable callback = ObjectWrapper.unwrap(completionCallback, Runnable.class);
        ProfileImplJni.get().clearBrowsingData(
                mNativeProfile, mapBrowsingDataTypes(dataTypes), fromMillis, toMillis, callback);
    }

    private static @ImplBrowsingDataType int[] mapBrowsingDataTypes(
            @NonNull @BrowsingDataType int[] dataTypes) {
        // Convert data types coming from aidl to the ones accepted by C++ (ImplBrowsingDataType is
        // generated from a C++ enum).
        List<Integer> convertedTypes = new ArrayList<>();
        for (int aidlType : dataTypes) {
            switch (aidlType) {
                case BrowsingDataType.COOKIES_AND_SITE_DATA:
                    convertedTypes.add(ImplBrowsingDataType.COOKIES_AND_SITE_DATA);
                    break;
                case BrowsingDataType.CACHE:
                    convertedTypes.add(ImplBrowsingDataType.CACHE);
                    break;
                default:
                    break; // Skip unrecognized values for forward compatibility.
            }
        }
        return CollectionUtil.integerListToIntArray(convertedTypes);
    }

    long getNativeProfile() {
        return mNativeProfile;
    }

    @NativeMethods
    interface Natives {
        long createProfile(String name);
        void deleteProfile(long profile);
        void clearBrowsingData(long nativeProfileImpl, @ImplBrowsingDataType int[] dataTypes,
                long fromMillis, long toMillis, Runnable callback);
    }
}
