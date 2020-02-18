// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profile_card;

import android.graphics.Bitmap;

import java.util.ArrayList;

/** Defines the profile card data. */
public class ProfileCardData {
    private Bitmap mAvatarBitmap;
    private String mTitle;
    private String mDescription;
    private String mPostFrequency;
    private ArrayList<ContentPreviewPostData> mPostDataList;

    public ProfileCardData(Bitmap avatarBitmap, String title, String description,
            String postFrequency, ArrayList<ContentPreviewPostData> postDataList) {
        mAvatarBitmap = avatarBitmap;
        mTitle = title;
        mDescription = description;
        mPostFrequency = postFrequency;
        mPostDataList = postDataList;
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

    public ArrayList<ContentPreviewPostData> getPostDataList() {
        return mPostDataList;
    }
}
