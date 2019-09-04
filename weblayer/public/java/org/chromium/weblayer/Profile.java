// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.content.Context;
import android.os.RemoteException;
import android.util.AndroidRuntimeException;
import android.util.Log;

import org.chromium.weblayer_private.aidl.IProfile;
import org.chromium.weblayer_private.aidl.ObjectWrapper;

/**
 * Profile holds state (typically on disk) needed for browsing. Create a
 * Profile via WebLayer.
 */
public final class Profile {
    private static final String TAG = "WL_Profile";

    private IProfile mImpl;

    Profile(IProfile impl) {
        mImpl = impl;
    }

    @Override
    protected void finalize() {
        // TODO(sky): figure out right assertion here if mImpl is non-null.
    }

    public void destroy() {
        try {
            mImpl.destroy();
        } catch (RemoteException e) {
            Log.e(TAG, "Failed to call destroy.", e);
            throw new AndroidRuntimeException(e);
        }
    }

    public void clearBrowsingData() {
        try {
            mImpl.clearBrowsingData();
        } catch (RemoteException e) {
            Log.e(TAG, "Failed to call clearBrowsingData.", e);
            throw new AndroidRuntimeException(e);
        }
    }

    public BrowserController createBrowserController(Context context) {
        try {
            return new BrowserController(
                    mImpl.createBrowserController(ObjectWrapper.wrap(context)));
        } catch (RemoteException e) {
            Log.e(TAG, "Failed to call createBrowserController.", e);
            throw new AndroidRuntimeException(e);
        }
    }
}
