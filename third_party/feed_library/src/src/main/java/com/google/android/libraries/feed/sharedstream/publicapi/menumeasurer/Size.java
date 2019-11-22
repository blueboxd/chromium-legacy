// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.sharedstream.publicapi.menumeasurer;

/**
 * Class to represent the width and height of a view in pixels. This is a minimal version of {@link
 * android.util.Size} which is only available in API 21.
 */
public class Size {
    private final int width;
    private final int height;

    public Size(int width, int height) {
        this.width = width;
        this.height = height;
    }

    /** Gets the width of the size in pixels. */
    public int getWidth() {
        return width;
    }

    /** Gets the height of the size in pixels. */
    public int getHeight() {
        return height;
    }

    @Override
    public boolean equals(/*@Nullable*/ Object o) {
        if (this == o) {
            return true;
        }
        if (!(o instanceof Size)) {
            return false;
        }

        Size size = (Size) o;

        return width == size.width && height == size.height;
    }

    @Override
    public int hashCode() {
        return 31 * width + height;
    }
}
