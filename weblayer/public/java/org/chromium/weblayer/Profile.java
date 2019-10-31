// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.os.RemoteException;

import androidx.annotation.NonNull;

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

    /**
     * Clears the data associated with the Profile.
     * The clearing is asynchronous, and new data may be generated during clearing. It is safe to
     * call this method repeatedly without waiting for callback.
     *
     * @param dataTypes See {@link BrowsingDataType}.
     * @return {@link ListenableResult} into which a "null" will be supplied when clearing is
     * finished.
     */
    @NonNull
    public ListenableResult<Void> clearBrowsingData(@NonNull @BrowsingDataType int[] dataTypes) {
        ThreadCheck.ensureOnUiThread();
        try {
            ListenableResult<Void> result = new ListenableResult<>();
            mImpl.clearBrowsingData(dataTypes,
                    ObjectWrapper.wrap((Runnable) () -> result.supplyResult(null)));
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
