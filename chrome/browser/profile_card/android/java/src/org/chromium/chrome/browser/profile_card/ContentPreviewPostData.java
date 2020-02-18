// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profile_card;

import android.graphics.Bitmap;

import androidx.annotation.Nullable;

import java.util.Date;

/** Defines the content oreview post data. */
public class ContentPreviewPostData {
    private String mTitle;
    private Date mPostedTime;
    private String mUrl;
    @Nullable
    private Bitmap mImageBitmap;

    public ContentPreviewPostData(
            String title, Date postedTime, String url, @Nullable Bitmap imageBitmap) {
        mTitle = title;
        mPostedTime = postedTime;
        mUrl = url;
        mImageBitmap = imageBitmap;
    }

    public String getTitle() {
        return mTitle;
    }

    public Date getPostedTime() {
        return mPostedTime;
    }

    public String getUrl() {
        return mUrl;
    }

    public Bitmap getImageBitmap() {
        return mImageBitmap;
    }
}
