// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profile_card;

import android.graphics.Bitmap;

/** Defines the creator's metadata. */
public class CreatorMetadata {
    private Bitmap mAvatarBitmap;
    private String mTitle;
    private String mDescription;
    private String mPostFrequency;

    public CreatorMetadata(
            Bitmap avatarBitmap, String title, String description, String postFrequency) {
        mAvatarBitmap = avatarBitmap;
        mTitle = title;
        mDescription = description;
        mPostFrequency = postFrequency;
    }

    public Bitmap getAvatarBitmap() {
        return mAvatarBitmap;
    }

    public String getTitle() {
        return mTitle;
    }

    public String getDescription() {
        return mDescription;
    }

    public String getPostFrequency() {
        return mPostFrequency;
    }
}
