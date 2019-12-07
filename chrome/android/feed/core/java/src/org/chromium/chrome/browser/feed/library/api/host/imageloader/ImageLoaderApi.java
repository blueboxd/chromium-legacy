// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.host.imageloader;

import android.graphics.drawable.Drawable;

import org.chromium.base.Consumer;

import java.util.List;

/** Feed Host API to load images. */
public interface ImageLoaderApi {
    /** Constant used to notify host that an image's height or width is not known. */
    int DIMENSION_UNKNOWN = -1;

    /**
     * Asks host to load an image from the web, a bundled asset, or some other type of drawable like
     * a monogram if supported.
     *
     * <p>The width and the height of the image can be provided preemptively, however it is not
     * guaranteed that both dimensions will be known. In the case that only one dimension is known,
     * the host should be careful to preserve the aspect ratio.
     *
     * <p>Feed will not cache any images, so if caching is desired, it should be done on the host
     * side.
     *
     * @param urls A list of urls, tried in order until a load succeeds. Urls may be bundled. The
     *         list
     *     of bundled assets will be defined via the {@link BundledAssets} StringDef.
     * @param widthPx The width of the image in pixels. Will be {@link #DIMENSION_UNKNOWN} if
     *         unknown.
     * @param heightPx The height of the image in pixels. Will be {@link #DIMENSION_UNKNOWN} if
     *     unknown.
     * @param consumer Callback to return the Drawable to if one of the urls provided is successful.
     *     {@literal null} if no Drawable is found (or could not be loaded) after trying all
     * possible urls.
     */
    void loadDrawable(List<String> urls, int widthPx, int heightPx, Consumer<Drawable> consumer);
}
