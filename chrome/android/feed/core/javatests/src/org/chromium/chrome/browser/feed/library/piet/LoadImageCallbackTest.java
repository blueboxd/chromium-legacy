// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet;

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

import org.junit.Before;
import org.junit.Ignore;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.chrome.browser.feed.library.common.time.testing.FakeClock;
import org.chromium.chrome.browser.feed.library.piet.host.AssetProvider;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests of the {@link LoadImageCallback}. */
@RunWith(LocalRobolectricTestRunner.class)
@org.robolectric.annotation.Config(sdk = 27, manifest = org.robolectric.annotation.Config.NONE)
public class LoadImageCallbackTest {
    private static final long FADE_IMAGE_THRESHOLD_MS = 682L;
    private static final Integer NO_OVERLAY_COLOR = 0;
    private static final boolean FADE_IMAGE = true;
    private static final boolean NO_FADE_IMAGE = false;

    @Mock
    private HostProviders mHostProviders;
    @Mock
    private AssetProvider mAssetProvider;
    @Mock
    private FrameContext mFrameContext;

    private final Drawable mInitialDrawable = new ColorDrawable(Color.RED);
    private final Drawable mFinalDrawable =
            new BitmapDrawable(Bitmap.createBitmap(12, 34, Config.ARGB_8888));
    private final FakeClock mClock = new FakeClock();
    private AdapterParameters mParameters;
    private Context mContext;

    @Before
    public void setup() {
        initMocks(this);
        mContext = Robolectric.buildActivity(Activity.class).get();

        when(mHostProviders.getAssetProvider()).thenReturn(mAssetProvider);
        when(mAssetProvider.getFadeImageThresholdMs()).thenReturn(FADE_IMAGE_THRESHOLD_MS);
        when(mFrameContext.getFrameView()).thenReturn(new FrameLayout(mContext));

        mParameters = new AdapterParameters(mContext, null, mHostProviders, null,
                mock(ElementAdapterFactory.class), mock(TemplateBinder.class), mClock);
    }

    @Test
    @Ignore("crbug.com/1024945 Test fails, needs debugging")
    public void testInitialDrawable_fromImageView() {
        ImageView imageView = new ImageView(mContext);
        imageView.setImageDrawable(mInitialDrawable);

        LoadImageCallback callback = new LoadImageCallback(imageView, ScaleType.CENTER,
                NO_OVERLAY_COLOR, FADE_IMAGE, mParameters, mFrameContext);

        mClock.advance(FADE_IMAGE_THRESHOLD_MS + 1);
        callback.accept(mFinalDrawable);

        assertThat(imageView.getDrawable()).isInstanceOf(TransitionDrawable.class);

        TransitionDrawable drawable = (TransitionDrawable) imageView.getDrawable();

        assertThat(drawable.getDrawable(0)).isSameInstanceAs(mInitialDrawable);
        assertThat(drawable.getDrawable(1)).isSameInstanceAs(mFinalDrawable);
    }

    @Test
    public void testInitialDrawable_transparent() {
        ImageView imageView = new ImageView(mContext);

        LoadImageCallback callback = new LoadImageCallback(imageView, ScaleType.CENTER,
                NO_OVERLAY_COLOR, FADE_IMAGE, mParameters, mFrameContext);

        mClock.advance(FADE_IMAGE_THRESHOLD_MS + 1);
        callback.accept(mFinalDrawable);

        assertThat(imageView.getDrawable()).isInstanceOf(TransitionDrawable.class);

        TransitionDrawable drawable = (TransitionDrawable) imageView.getDrawable();

        assertThat(((ColorDrawable) drawable.getDrawable(0)).getColor())
                .isEqualTo(Color.TRANSPARENT);
    }

    @Test
    @Ignore("crbug.com/1024945 Test fails, needs debugging")
    public void testQuickLoad_doesntFade_fadingDisabled() {
        ImageView imageView = new ImageView(mContext);

        LoadImageCallback callback = new LoadImageCallback(imageView, ScaleType.CENTER,
                NO_OVERLAY_COLOR, NO_FADE_IMAGE, mParameters, mFrameContext);

        mClock.advance(FADE_IMAGE_THRESHOLD_MS + 1);
        callback.accept(mFinalDrawable);

        assertThat(imageView.getDrawable()).isSameInstanceAs(mFinalDrawable);
    }

    @Test
    @Ignore("crbug.com/1024945 Test fails, needs debugging")
    public void testQuickLoad_doesntFade_loadsBeforeTimeout() {
        ImageView imageView = new ImageView(mContext);

        LoadImageCallback callback = new LoadImageCallback(imageView, ScaleType.CENTER,
                NO_OVERLAY_COLOR, FADE_IMAGE, mParameters, mFrameContext);

        mClock.advance(FADE_IMAGE_THRESHOLD_MS - 1);
        callback.accept(mFinalDrawable);

        assertThat(imageView.getDrawable()).isSameInstanceAs(mFinalDrawable);
    }

    @Test
    @Ignore("crbug.com/1024945 Test fails, needs debugging")
    public void testDelayedTask() {
        ImageView imageView = new ImageView(mContext);
        LoadImageCallback callback = new LoadImageCallback(imageView, ScaleType.CENTER,
                NO_OVERLAY_COLOR, FADE_IMAGE, mParameters, mFrameContext);
        mClock.advance(FADE_IMAGE_THRESHOLD_MS + 1);
        callback.accept(mFinalDrawable);

        // Starts as transition drawable.
        assertThat(imageView.getDrawable()).isInstanceOf(TransitionDrawable.class);

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        // After the delayed task is run, the drawable is set to the mFinalDrawable instead of the
        // TransitionDrawable to allow for garbage collection of the TransitionDrawable and
        // initialDrawable.
        assertThat(imageView.getDrawable()).isSameInstanceAs(mFinalDrawable);
    }

    @Test
    public void testSetsScaleType() {
        ImageView imageView = new ImageView(mContext);
        LoadImageCallback callback = new LoadImageCallback(imageView, ScaleType.CENTER,
                NO_OVERLAY_COLOR, NO_FADE_IMAGE, mParameters, mFrameContext);
        callback.accept(mFinalDrawable);

        assertThat(imageView.getScaleType()).isEqualTo(ScaleType.CENTER);
    }

    @Test
    public void testSetsOverlayColor() {
        int color = 0xFEEDFACE;
        ImageView imageView = new ImageView(mContext);
        LoadImageCallback callback = new LoadImageCallback(
                imageView, ScaleType.CENTER, color, NO_FADE_IMAGE, mParameters, mFrameContext);
        callback.accept(mFinalDrawable);

        assertThat(imageView.getDrawable().getColorFilter())
                .isEqualTo(new PorterDuffColorFilter(color, Mode.SRC_IN));
    }

    @Test
    @Ignore("crbug.com/1024945 Test fails, needs debugging")
    public void testSetsOverlayColor_null() {
        ImageView imageView = new ImageView(mContext);
        LoadImageCallback callback = new LoadImageCallback(imageView, ScaleType.CENTER,
                NO_OVERLAY_COLOR, NO_FADE_IMAGE, mParameters, mFrameContext);
        callback.accept(mFinalDrawable);

        assertThat(imageView.getDrawable().getColorFilter()).isNull();
    }
}
