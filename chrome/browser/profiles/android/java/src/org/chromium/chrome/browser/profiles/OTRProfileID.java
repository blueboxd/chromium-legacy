// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profiles;

import org.chromium.base.annotations.CalledByNative;

/**
 * Wrapper that allows passing a OTRProfileID reference around in the Java layer.
 */
public class OTRProfileID {
    private final String mProfileID;

    @CalledByNative
    public OTRProfileID(String profileID) {
        assert profileID != null;
        mProfileID = profileID;
    }

    @CalledByNative
    private String getProfileID() {
        return mProfileID;
    }

    @Override
    public String toString() {
        return String.format("OTRProfileID{%s}", mProfileID);
    }

    @Override
    public int hashCode() {
        return mProfileID.hashCode();
    }

    @Override
    public boolean equals(Object obj) {
        if (!(obj instanceof OTRProfileID)) return false;
        OTRProfileID other = (OTRProfileID) obj;
        return mProfileID.equals(other.mProfileID);
    }
}
