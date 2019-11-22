// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.piet;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.graphics.Color;
import android.graphics.PorterDuff.Mode;
import android.graphics.PorterDuffColorFilter;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.TransitionDrawable;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.ImageView.ScaleType;

import com.google.android.libraries.feed.common.time.testing.FakeClock;
import com.google.android.libraries.feed.piet.host.AssetProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.shadows.ShadowLooper;

/** Tests of the {@link LoadImageCallback}. */
@RunWith(RobolectricTestRunner.class)
public class LoadImageCallbackTest {
    private static final long FADE_IMAGE_THRESHOLD_MS = 682L;
    private static final Integer NO_OVERLAY_COLOR;
    private static final boolean FADE_IMAGE = true;
    private static final boolean NO_FADE_IMAGE;

    @Mock
    private HostProviders hostProviders;
    @Mock
    private AssetProvider assetProvider;
    @Mock
    private FrameContext frameContext;

    private final Drawable initialDrawable = new ColorDrawable(Color.RED);
    private final Drawable finalDrawable =
            new BitmapDrawable(Bitmap.createBitmap(12, 34, Config.ARGB_8888));
    private final FakeClock clock = new FakeClock();
    private AdapterParameters parameters;
    private Context context;

    @Before
    public void setup() {
        initMocks(this);
        context = Robolectric.buildActivity(Activity.class).get();

        when(hostProviders.getAssetProvider()).thenReturn(assetProvider);
        when(assetProvider.getFadeImageThresholdMs()).thenReturn(FADE_IMAGE_THRESHOLD_MS);
        when(frameContext.getFrameView()).thenReturn(new FrameLayout(context));

        parameters = new AdapterParameters(context, null, hostProviders, null,
                mock(ElementAdapterFactory.class), mock(TemplateBinder.class), clock);
    }

    @Test
    public void testInitialDrawable_fromImageView() {
        ImageView imageView = new ImageView(context);
        imageView.setImageDrawable(initialDrawable);

        LoadImageCallback callback = new LoadImageCallback(imageView, ScaleType.CENTER,
                NO_OVERLAY_COLOR, FADE_IMAGE, parameters, frameContext);

        clock.advance(FADE_IMAGE_THRESHOLD_MS + 1);
        callback.accept(finalDrawable);

        assertThat(imageView.getDrawable()).isInstanceOf(TransitionDrawable.class);

        TransitionDrawable drawable = (TransitionDrawable) imageView.getDrawable();

        assertThat(drawable.getDrawable(0)).isSameInstanceAs(initialDrawable);
        assertThat(drawable.getDrawable(1)).isSameInstanceAs(finalDrawable);
    }

    @Test
    public void testInitialDrawable_transparent() {
        ImageView imageView = new ImageView(context);

        LoadImageCallback callback = new LoadImageCallback(imageView, ScaleType.CENTER,
                NO_OVERLAY_COLOR, FADE_IMAGE, parameters, frameContext);

        clock.advance(FADE_IMAGE_THRESHOLD_MS + 1);
        callback.accept(finalDrawable);

        assertThat(imageView.getDrawable()).isInstanceOf(TransitionDrawable.class);

        TransitionDrawable drawable = (TransitionDrawable) imageView.getDrawable();

        assertThat(((ColorDrawable) drawable.getDrawable(0)).getColor())
                .isEqualTo(Color.TRANSPARENT);
    }

    @Test
    public void testQuickLoad_doesntFade_fadingDisabled() {
        ImageView imageView = new ImageView(context);

        LoadImageCallback callback = new LoadImageCallback(imageView, ScaleType.CENTER,
                NO_OVERLAY_COLOR, NO_FADE_IMAGE, parameters, frameContext);

        clock.advance(FADE_IMAGE_THRESHOLD_MS + 1);
        callback.accept(finalDrawable);

        assertThat(imageView.getDrawable()).isSameInstanceAs(finalDrawable);
    }

    @Test
    public void testQuickLoad_doesntFade_loadsBeforeTimeout() {
        ImageView imageView = new ImageView(context);

        LoadImageCallback callback = new LoadImageCallback(imageView, ScaleType.CENTER,
                NO_OVERLAY_COLOR, FADE_IMAGE, parameters, frameContext);

        clock.advance(FADE_IMAGE_THRESHOLD_MS - 1);
        callback.accept(finalDrawable);

        assertThat(imageView.getDrawable()).isSameInstanceAs(finalDrawable);
    }

    @Test
    public void testDelayedTask() {
        ImageView imageView = new ImageView(context);
        LoadImageCallback callback = new LoadImageCallback(imageView, ScaleType.CENTER,
                NO_OVERLAY_COLOR, FADE_IMAGE, parameters, frameContext);
        clock.advance(FADE_IMAGE_THRESHOLD_MS + 1);
        callback.accept(finalDrawable);

        // Starts as transition drawable.
        assertThat(imageView.getDrawable()).isInstanceOf(TransitionDrawable.class);

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        // After the delayed task is run, the drawable is set to the finalDrawable instead of the
        // TransitionDrawable to allow for garbage collection of the TransitionDrawable and
        // initialDrawable.
        assertThat(imageView.getDrawable()).isSameInstanceAs(finalDrawable);
    }

    @Test
    public void testSetsScaleType() {
        ImageView imageView = new ImageView(context);
        LoadImageCallback callback = new LoadImageCallback(imageView, ScaleType.CENTER,
                NO_OVERLAY_COLOR, NO_FADE_IMAGE, parameters, frameContext);
        callback.accept(finalDrawable);

        assertThat(imageView.getScaleType()).isEqualTo(ScaleType.CENTER);
    }

    @Test
    public void testSetsOverlayColor() {
        int color = 0xFEEDFACE;
        ImageView imageView = new ImageView(context);
        LoadImageCallback callback = new LoadImageCallback(
                imageView, ScaleType.CENTER, color, NO_FADE_IMAGE, parameters, frameContext);
        callback.accept(finalDrawable);

        assertThat(imageView.getDrawable().getColorFilter())
                .isEqualTo(new PorterDuffColorFilter(color, Mode.SRC_IN));
    }

    @Test
    public void testSetsOverlayColor_null() {
        ImageView imageView = new ImageView(context);
        LoadImageCallback callback = new LoadImageCallback(imageView, ScaleType.CENTER,
                NO_OVERLAY_COLOR, NO_FADE_IMAGE, parameters, frameContext);
        callback.accept(finalDrawable);

        assertThat(imageView.getDrawable().getColorFilter()).isNull();
    }
}
