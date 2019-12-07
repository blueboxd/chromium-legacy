// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet.host;

import android.graphics.drawable.Drawable;

import org.chromium.base.Consumer;
import org.chromium.components.feed.core.proto.ui.piet.ImagesProto.Image;

/** {@link ImageLoader} that always returns {@link null}. For image-less clients. */
public class NullImageLoader implements ImageLoader {
    @Override
    public void getImage(
            Image image, int widthPx, int heightPx, Consumer</*@Nullable*/ Drawable> consumer) {
        consumer.accept(null);
    }
}
