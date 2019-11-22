// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.piet.host;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.MockitoAnnotations.initMocks;

import android.app.Activity;
import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.widget.ImageView;

import com.google.android.libraries.feed.common.functional.Suppliers;
import com.google.search.now.ui.piet.ImagesProto.Image;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;

/** Tests for {@link AssetProvider}. */
@RunWith(RobolectricTestRunner.class)
public class AssetProviderTest {
    @Mock
    ImageLoader imageLoader;
    @Mock
    StringFormatter stringFormatter;
    @Mock
    TypefaceProvider typefaceProvider;

    private Context context;

    @Before
    public void setUp() {
        initMocks(this);

        context = Robolectric.buildActivity(Activity.class).get();
    }

    @Test
    public void testBuilder_setsFields() {
        AssetProvider assetProvider = new AssetProvider(imageLoader, stringFormatter,
                Suppliers.of(123), Suppliers.of(456L), Suppliers.of(true), Suppliers.of(true),
                typefaceProvider);

        assertThat(assetProvider.imageLoader).isSameInstanceAs(imageLoader);
        assertThat(assetProvider.stringFormatter).isSameInstanceAs(stringFormatter);
        assertThat(assetProvider.typefaceProvider).isSameInstanceAs(typefaceProvider);
        assertThat(assetProvider.getDefaultCornerRadius()).isEqualTo(123);
        assertThat(assetProvider.getFadeImageThresholdMs()).isEqualTo(456);
        assertThat(assetProvider.isDarkTheme()).isTrue();
        assertThat(assetProvider.isRtL()).isTrue();
    }

    @Test
    public void testNullImageLoader() {
        ImageLoader imageLoader = new NullImageLoader();
        ImageView imageView = new ImageView(context);
        imageView.setImageDrawable(new ColorDrawable(Color.RED));

        imageLoader.getImage(
                Image.getDefaultInstance(), 1, 2, drawable -> imageView.setImageDrawable(drawable));

        assertThat(imageView.getDrawable()).isNull();
    }

    @Test
    public void testNullTypefaceProvider() {
        TypefaceProvider typefaceProvider = new NullTypefaceProvider();
        final Object[] consumedObject = {""};

        // Make sure the object isn't already null, or we'll get a false positive.
        assertThat(consumedObject[0]).isNotNull();
        // The consumer passed in just saves the value that is consumed, so we can check that it's
        // null.
        typefaceProvider.getTypeface(
                "GOOGLE_SANS_MEDIUM", false, typeface -> consumedObject[0] = typeface);
        assertThat(consumedObject[0]).isNull();
    }

    @Test
    public void testEmptyStringFormatter() {
        StringFormatter stringFormatter = new EmptyStringFormatter();

        assertThat(stringFormatter.getRelativeElapsedString(123456)).isEmpty();
    }
}
