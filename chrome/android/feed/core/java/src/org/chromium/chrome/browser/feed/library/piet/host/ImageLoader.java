// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet.host;

import android.graphics.drawable.Drawable;

import org.chromium.base.Consumer;
import org.chromium.components.feed.core.proto.ui.piet.ImagesProto.Image;

/** Class that allows Piet to ask the host to load an image. */
public interface ImageLoader {
    /**
     * Given an {@link Image}, asynchronously load the {@link Drawable} and return via a {@link
     * Consumer}.
     *
     * <p>The width and the height of the image can be provided preemptively, however it is not
     * guaranteed that both dimensions will be known. In the case that only one dimension is known,
     * the host should be careful to preserve the aspect ratio.
     *
     * @param image The image to load.
     * @param widthPx The width of the {@link Image} in pixels. Will be {@link #DIMENSION_UNKNOWN}
     *         if
     *     unknown.
     * @param heightPx The height of the {@link Image} in pixels. Will be {@link #DIMENSION_UNKNOWN}
     *     if unknown.
     * @param consumer Callback to return the {@link Drawable} from an {@link Image} if the load
     *     succeeds. {@literal null} should be passed to this if no source succeeds in loading the
     *     image
     */
    void getImage(
            Image image, int widthPx, int heightPx, Consumer</*@Nullable*/ Drawable> consumer);
}
