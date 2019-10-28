// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.os.RemoteException;

import org.chromium.weblayer_private.aidl.APICallException;
import org.chromium.weblayer_private.aidl.IProfile;
import org.chromium.weblayer_private.aidl.ObjectWrapper;

/**
 * Profile holds state (typically on disk) needed for browsing. Create a
 * Profile via WebLayer.
 */
public final class Profile {
    private IProfile mImpl;
    private Runnable mOnDestroyRunnable;


    /* package */ Profile(IProfile impl, Runnable onDestroyRunnable) {
        mImpl = impl;
        mOnDestroyRunnable = onDestroyRunnable;
    }

    @Override
    protected void finalize() {
        // TODO(sky): figure out right assertion here if mImpl is non-null.
    }

    public ListenableResult<Void> clearBrowsingData() {
        ThreadCheck.ensureOnUiThread();
        try {
            ListenableResult<Void> result = new ListenableResult<>();
            mImpl.clearBrowsingData(ObjectWrapper.wrap((Runnable) () -> result.supplyResult(null)));
            return result;
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    public void destroy() {
        ThreadCheck.ensureOnUiThread();
        try {
            mImpl.destroy();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
        mImpl = null;
        mOnDestroyRunnable.run();
        mOnDestroyRunnable = null;
    }
}
