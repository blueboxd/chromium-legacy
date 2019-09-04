// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.os.IBinder;
import android.os.RemoteException;
import android.util.AndroidRuntimeException;
import android.util.Log;

import org.chromium.weblayer_private.aidl.IWebLayer;

import java.io.File;

/**
 * WebLayer is responsible for initializing state necessary to use* any of the classes in web layer.
 */
public final class WebLayer {
    private static final String TAG = "WebLayer";

    private static WebLayer sInstance;
    private IWebLayer mImpl;

    public static WebLayer getInstance() {
        if (sInstance == null) {
            sInstance = new WebLayer();
        }
        return sInstance;
    }

    WebLayer() {
        try {
            mImpl = IWebLayer.Stub.asInterface(
                    (IBinder) Class.forName("org.chromium.weblayer_private.WebLayerImpl")
                            .getMethod("create")
                            .invoke(null));
        } catch (Exception e) {
            Log.e(TAG, "Failed to get WebLayerImpl.", e);
            throw new AndroidRuntimeException(e);
        }
    }

    @Override
    protected void finalize() {
        // TODO(sky): figure out right assertion here if mProfile is non-null.
    }

    public void destroy() {
        // TODO: implement me.
        sInstance = null;
    }

    /**
     * Creates a new Profile with the given path. Pass in an empty path for an in-memory profile.
     */
    public Profile createProfile(File path) {
        try {
            return new Profile(mImpl.createProfile(path.getPath()));
        } catch (RemoteException e) {
            Log.e(TAG, "Failed to call createProfile.", e);
            throw new AndroidRuntimeException(e);
        }
    }
}
