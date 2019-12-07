// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.sharedstream.publicapi.menumeasurer;

import androidx.annotation.Nullable;

/**
 * Class to represent the width and height of a view in pixels. This is a minimal version of {@link
 * android.util.Size} which is only available in API 21.
 */
public class Size {
    private final int mWidth;
    private final int mHeight;

    public Size(int width, int height) {
        this.mWidth = width;
        this.mHeight = height;
    }

    /** Gets the width of the size in pixels. */
    public int getWidth() {
        return mWidth;
    }

    /** Gets the height of the size in pixels. */
    public int getHeight() {
        return mHeight;
    }

    @Override
    public boolean equals(@Nullable Object o) {
        if (this == o) {
            return true;
        }
        if (!(o instanceof Size)) {
            return false;
        }

        Size size = (Size) o;

        return mWidth == size.mWidth && mHeight == size.mHeight;
    }

    @Override
    public int hashCode() {
        return 31 * mWidth + mHeight;
    }
}
