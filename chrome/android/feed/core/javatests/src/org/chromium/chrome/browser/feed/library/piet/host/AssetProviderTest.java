// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet.host;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.MockitoAnnotations.initMocks;

import android.app.Activity;
import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.widget.ImageView;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.common.functional.Suppliers;
import org.chromium.components.feed.core.proto.ui.piet.ImagesProto.Image;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests for {@link AssetProvider}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AssetProviderTest {
    @Mock
    ImageLoader mImageLoader;
    @Mock
    StringFormatter mStringFormatter;
    @Mock
    TypefaceProvider mTypefaceProvider;

    private Context mContext;

    @Before
    public void setUp() {
        initMocks(this);

        mContext = Robolectric.buildActivity(Activity.class).get();
    }

    @Test
    public void testBuilder_setsFields() {
        AssetProvider assetProvider = new AssetProvider(mImageLoader, mStringFormatter,
                Suppliers.of(123), Suppliers.of(456L), Suppliers.of(true), Suppliers.of(true),
                mTypefaceProvider);

        assertThat(assetProvider.mImageLoader).isSameInstanceAs(mImageLoader);
        assertThat(assetProvider.mStringFormatter).isSameInstanceAs(mStringFormatter);
        assertThat(assetProvider.mTypefaceProvider).isSameInstanceAs(mTypefaceProvider);
        assertThat(assetProvider.getDefaultCornerRadius()).isEqualTo(123);
        assertThat(assetProvider.getFadeImageThresholdMs()).isEqualTo(456);
        assertThat(assetProvider.isDarkTheme()).isTrue();
        assertThat(assetProvider.isRtL()).isTrue();
    }

    @Test
    public void testNullImageLoader() {
        ImageLoader imageLoader = new NullImageLoader();
        ImageView imageView = new ImageView(mContext);
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
