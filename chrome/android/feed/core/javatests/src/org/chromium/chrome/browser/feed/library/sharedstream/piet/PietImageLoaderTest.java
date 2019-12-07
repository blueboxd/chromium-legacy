// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.sharedstream.piet;

import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.graphics.drawable.Drawable;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.annotation.Config;

import org.chromium.base.Consumer;
import org.chromium.chrome.browser.feed.library.api.host.imageloader.ImageLoaderApi;
import org.chromium.chrome.browser.feed.library.api.host.stream.CardConfiguration;
import org.chromium.chrome.browser.feed.library.common.time.testing.FakeClock;
import org.chromium.components.feed.core.proto.ui.piet.ImagesProto.Image;
import org.chromium.components.feed.core.proto.ui.piet.ImagesProto.ImageSource;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeUnit;

/** Tests for {@link PietImageLoader}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PietImageLoaderTest {
    private static final int DEFAULT_CORNER_RADIUS = 10;
    private static final int WIDTH_PX = 50;
    private static final int HEIGHT_PX = 150;

    @Mock
    private CardConfiguration mCardConfiguration;
    @Mock
    private ImageLoaderApi mImageLoaderApi;

    private FakeClock mClock;
    private PietImageLoader mPietImageLoader;

    @Before
    public void setUp() {
        initMocks(this);

        when(mCardConfiguration.getDefaultCornerRadius()).thenReturn(DEFAULT_CORNER_RADIUS);

        mClock = new FakeClock();
        mClock.set(TimeUnit.MINUTES.toMillis(1));
        mPietImageLoader = new PietImageLoader(mImageLoaderApi);
    }

    @Test
    public void testGetImage() {
        List<String> urls = Arrays.asList("url0", "url1", "url2");
        Image.Builder imageBuilder = Image.newBuilder();
        for (String url : urls) {
            imageBuilder.addSources(ImageSource.newBuilder().setUrl(url).build());
        }

        Consumer<Drawable> consumer = value
                -> {
                        // Do nothing.
                };

        mPietImageLoader.getImage(imageBuilder.build(), WIDTH_PX, HEIGHT_PX, consumer);
        verify(mImageLoaderApi).loadDrawable(urls, WIDTH_PX, HEIGHT_PX, consumer);
    }
}
