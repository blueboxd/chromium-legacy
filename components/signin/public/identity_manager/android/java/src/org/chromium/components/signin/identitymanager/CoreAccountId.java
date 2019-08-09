// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.signin.identitymanager;

import android.support.annotation.NonNull;

/**
 * A wrapper around Gaia ID that represents a stable account identifier.
 *
 * This wrapper helps to make sure that code using accounts doesn't accidentally use account name in
 * place of Gaia ID and vice versa.
 *
 * This class has a native counterpart called CoreAccountId.
 */
public class CoreAccountId {
    private final String mGaiaId;

    /**
     * Constructs a new CoreAccountId from a String representation of Gaia ID.
     */
    public CoreAccountId(@NonNull String gaiaId) {
        assert gaiaId != null;
        // Check that a user email is not used by mistake.
        assert !gaiaId.contains("@");

        mGaiaId = gaiaId;
    }

    public String getGaiaIdAsString() {
        return mGaiaId;
    }

    @Override
    public String toString() {
        return mGaiaId;
    }

    @Override
    public int hashCode() {
        return mGaiaId.hashCode();
    }

    @Override
    public boolean equals(Object obj) {
        if (!(obj instanceof CoreAccountId)) return false;
        CoreAccountId other = (CoreAccountId) obj;
        return mGaiaId.equals(other.mGaiaId);
    }
}
