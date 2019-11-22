// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.sharedstream.piet;

import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.graphics.drawable.Drawable;

import com.google.android.libraries.feed.api.host.imageloader.ImageLoaderApi;
import com.google.android.libraries.feed.api.host.stream.CardConfiguration;
import com.google.android.libraries.feed.common.functional.Consumer;
import com.google.android.libraries.feed.common.time.testing.FakeClock;
import com.google.search.now.ui.piet.ImagesProto.Image;
import com.google.search.now.ui.piet.ImagesProto.ImageSource;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.RobolectricTestRunner;

import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeUnit;

/** Tests for {@link PietImageLoader}. */
@RunWith(RobolectricTestRunner.class)
public class PietImageLoaderTest {
    private static final int DEFAULT_CORNER_RADIUS = 10;
    private static final int WIDTH_PX = 50;
    private static final int HEIGHT_PX = 150;

    @Mock
    private CardConfiguration cardConfiguration;
    @Mock
    private ImageLoaderApi imageLoaderApi;

    private FakeClock clock;
    private PietImageLoader pietImageLoader;

    @Before
    public void setUp() {
        initMocks(this);

        when(cardConfiguration.getDefaultCornerRadius()).thenReturn(DEFAULT_CORNER_RADIUS);

        clock = new FakeClock();
        clock.set(TimeUnit.MINUTES.toMillis(1));
        pietImageLoader = new PietImageLoader(imageLoaderApi);
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

        pietImageLoader.getImage(imageBuilder.build(), WIDTH_PX, HEIGHT_PX, consumer);
        verify(imageLoaderApi).loadDrawable(urls, WIDTH_PX, HEIGHT_PX, consumer);
    }
}
