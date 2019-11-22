// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.piet.host;

import android.graphics.drawable.Drawable;

import com.google.android.libraries.feed.common.functional.Consumer;
import com.google.search.now.ui.piet.ImagesProto.Image;

/** {@link ImageLoader} that always returns {@link null}. For image-less clients. */
public class NullImageLoader implements ImageLoader {
    @Override
    public void getImage(
            Image image, int widthPx, int heightPx, Consumer</*@Nullable*/ Drawable> consumer) {
        consumer.accept(null);
    }
}
