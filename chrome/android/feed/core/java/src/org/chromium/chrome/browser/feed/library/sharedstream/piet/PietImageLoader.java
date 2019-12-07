// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.sharedstream.piet;

import android.graphics.drawable.Drawable;

import org.chromium.base.Consumer;
import org.chromium.chrome.browser.feed.library.api.host.imageloader.ImageLoaderApi;
import org.chromium.chrome.browser.feed.library.piet.host.ImageLoader;
import org.chromium.components.feed.core.proto.ui.piet.ImagesProto.Image;
import org.chromium.components.feed.core.proto.ui.piet.ImagesProto.ImageSource;

import java.util.ArrayList;
import java.util.List;

import javax.inject.Inject;

/** A Piet {@link ImageLoader} which uses {@link ImageLoaderApi} to load images. */
public class PietImageLoader implements ImageLoader {
    private final ImageLoaderApi mImageLoaderApi;

    @Inject
    public PietImageLoader(ImageLoaderApi imageLoaderApi) {
        this.mImageLoaderApi = imageLoaderApi;
    }

    @Override
    public void getImage(
            Image image, int widthPx, int heightPx, Consumer</*@Nullable*/ Drawable> consumer) {
        List<String> urls = new ArrayList<>(image.getSourcesList().size());
        for (ImageSource source : image.getSourcesList()) {
            urls.add(source.getUrl());
        }

        mImageLoaderApi.loadDrawable(urls, widthPx, heightPx, consumer);
    }
}
