// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.os.RemoteException;

import androidx.annotation.NonNull;

import org.chromium.weblayer_private.interfaces.APICallException;
import org.chromium.weblayer_private.interfaces.IProfile;
import org.chromium.weblayer_private.interfaces.ObjectWrapper;

import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

/**
 * Profile holds state (typically on disk) needed for browsing. Create a
 * Profile via WebLayer.
 */
public final class Profile {
    private static final Map<String, Profile> sProfiles = new HashMap<>();

    /* package */ static Profile of(IProfile impl) {
        ThreadCheck.ensureOnUiThread();
        String name;
        try {
            name = impl.getName();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
        Profile profile = sProfiles.get(name);
        if (profile != null) {
            return profile;
        }

        return new Profile(name, impl);
    }

    /**
     * Return all profiles that have been created and not yet called destroyed.
     * Note returned structure is immutable.
     */
    @NonNull
    public static Collection<Profile> getAllProfiles() {
        ThreadCheck.ensureOnUiThread();
        return Collections.unmodifiableCollection(sProfiles.values());
    }

    private final String mName;
    private IProfile mImpl;

    private Profile(String name, IProfile impl) {
        mName = name;
        mImpl = impl;

        sProfiles.put(name, this);
    }

    /**
     * Clears the data associated with the Profile.
     * The clearing is asynchronous, and new data may be generated during clearing. It is safe to
     * call this method repeatedly without waiting for callback.
     *
     * @param dataTypes See {@link BrowsingDataType}.
     * @param fromMillis Defines the start (in milliseconds since epoch) of the time range to clear.
     * @param toMillis Defines the end (in milliseconds since epoch) of the time range to clear.
     * For clearing all data prefer using {@link Long#MAX_VALUE} to
     * {@link System.currentTimeMillis()} to take into account possible system clock changes.
     * @param callback {@link Runnable} which is run when clearing is finished.
     */
    public void clearBrowsingData(@NonNull @BrowsingDataType int[] dataTypes, long fromMillis,
            long toMillis, @NonNull Runnable callback) {
        ThreadCheck.ensureOnUiThread();
        try {
            mImpl.clearBrowsingData(dataTypes, fromMillis, toMillis, ObjectWrapper.wrap(callback));
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
    }

    /**
     * Clears the data associated with the Profile.
     * Same as {@link #clearBrowsingData(int[], long, long, Runnable)} with unbounded time range.
     */
    public void clearBrowsingData(
            @NonNull @BrowsingDataType int[] dataTypes, @NonNull Runnable callback) {
        clearBrowsingData(dataTypes, 0, Long.MAX_VALUE, callback);
    }

    public void destroy() {
        ThreadCheck.ensureOnUiThread();
        try {
            mImpl.destroy();
        } catch (RemoteException e) {
            throw new APICallException(e);
        }
        mImpl = null;
        sProfiles.remove(mName);
    }
}
