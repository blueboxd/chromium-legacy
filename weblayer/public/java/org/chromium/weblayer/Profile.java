// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import org.chromium.weblayer_private.ProfileImpl;

import java.io.File;

/**
 * Profile holds state (typically on disk) needed for browsing. Create a
 * Profile via WebLayer.
 */
public final class Profile {
    ProfileImpl mProfile;

    Profile(File path) {
        mProfile = new ProfileImpl(path.getPath());
    }

    ProfileImpl getProfileImpl() {
        return mProfile;
    }

    @Override
    protected void finalize() {
        // TODO(sky): figure out right assertion here if mProfile is non-null.
    }

    public void destroy() {
        mProfile.destroy();
        mProfile = null;
    }

    public void clearBrowsingData() {
        mProfile.clearBrowsingData();
    }
}
