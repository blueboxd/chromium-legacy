// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.webkit.ValueCallback;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.base.CollectionUtil;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.weblayer_private.interfaces.BrowsingDataType;
import org.chromium.weblayer_private.interfaces.ICookieManager;
import org.chromium.weblayer_private.interfaces.IDownloadCallbackClient;
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
    private final CookieManagerImpl mCookieManager;
    private Runnable mOnDestroyCallback;
    private boolean mBeingDeleted;
    private DownloadCallbackProxy mDownloadCallbackProxy;

    public static void enumerateAllProfileNames(ValueCallback<String[]> callback) {
        final Callback<String[]> baseCallback = (String[] names) -> callback.onReceiveValue(names);
        ProfileImplJni.get().enumerateAllProfileNames(baseCallback);
    }

    ProfileImpl(String name, Runnable onDestroyCallback) {
        if (!name.matches("^\\w*$")) {
            throw new IllegalArgumentException("Name can only contain words: " + name);
        }
        mName = name;
        mNativeProfile = ProfileImplJni.get().createProfile(name);
        mCookieManager =
                new CookieManagerImpl(ProfileImplJni.get().getCookieManager(mNativeProfile));
        mOnDestroyCallback = onDestroyCallback;
    }

    @Override
    public void destroy() {
        StrictModeWorkaround.apply();
        if (mBeingDeleted) return;

        if (mDownloadCallbackProxy != null) {
            mDownloadCallbackProxy.destroy();
            mDownloadCallbackProxy = null;
        }

        deleteNativeProfile();
        maybeRunDestroyCallback();
    }

    private void deleteNativeProfile() {
        ProfileImplJni.get().deleteProfile(mNativeProfile);
        mNativeProfile = 0;
    }

    private void maybeRunDestroyCallback() {
        if (mOnDestroyCallback == null) return;
        mOnDestroyCallback.run();
        mOnDestroyCallback = null;
    }

    @Override
    public void destroyAndDeleteDataFromDisk(IObjectWrapper completionCallback) {
        StrictModeWorkaround.apply();
        checkNotDestroyed();
        final Runnable callback = ObjectWrapper.unwrap(completionCallback, Runnable.class);
        assert mNativeProfile != 0;
        mBeingDeleted = ProfileImplJni.get().deleteDataFromDisk(mNativeProfile, () -> {
            deleteNativeProfile();
            if (callback != null) callback.run();
        });
        if (!mBeingDeleted) {
            throw new IllegalStateException("Profile still in use: " + mName);
        }
        maybeRunDestroyCallback();
    }

    @Override
    public String getName() {
        StrictModeWorkaround.apply();
        checkNotDestroyed();
        return mName;
    }

    public boolean isIncognito() {
        return mName.isEmpty();
    }

    @Override
    public void clearBrowsingData(@NonNull @BrowsingDataType int[] dataTypes, long fromMillis,
            long toMillis, @NonNull IObjectWrapper completionCallback) {
        StrictModeWorkaround.apply();
        checkNotDestroyed();
        Runnable callback = ObjectWrapper.unwrap(completionCallback, Runnable.class);
        ProfileImplJni.get().clearBrowsingData(
                mNativeProfile, mapBrowsingDataTypes(dataTypes), fromMillis, toMillis, callback);
    }

    @Override
    public void setDownloadDirectory(String directory) {
        StrictModeWorkaround.apply();
        checkNotDestroyed();
        ProfileImplJni.get().setDownloadDirectory(mNativeProfile, directory);
    }

    @Override
    public void setDownloadCallbackClient(IDownloadCallbackClient client) {
        StrictModeWorkaround.apply();
        if (client != null) {
            if (mDownloadCallbackProxy == null) {
                mDownloadCallbackProxy = new DownloadCallbackProxy(mNativeProfile, client);
            } else {
                mDownloadCallbackProxy.setClient(client);
            }
        } else if (mDownloadCallbackProxy != null) {
            mDownloadCallbackProxy.destroy();
            mDownloadCallbackProxy = null;
        }
    }

    @Override
    public ICookieManager getCookieManager() {
        StrictModeWorkaround.apply();
        checkNotDestroyed();
        return mCookieManager;
    }

    void checkNotDestroyed() {
        if (!mBeingDeleted) return;
        throw new IllegalArgumentException("Profile being destroyed: " + mName);
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
        void enumerateAllProfileNames(Callback<String[]> callback);
        long createProfile(String name);
        void deleteProfile(long profile);
        boolean deleteDataFromDisk(long nativeProfileImpl, Runnable completionCallback);
        void clearBrowsingData(long nativeProfileImpl, @ImplBrowsingDataType int[] dataTypes,
                long fromMillis, long toMillis, Runnable callback);
        void setDownloadDirectory(long nativeProfileImpl, String directory);
        long getCookieManager(long nativeProfileImpl);
    }
}
