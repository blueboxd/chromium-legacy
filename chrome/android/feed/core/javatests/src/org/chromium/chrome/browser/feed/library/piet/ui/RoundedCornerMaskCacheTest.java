// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet.ui;

import static com.google.common.truth.Truth.assertThat;

import static org.chromium.chrome.browser.feed.library.common.testing.RunnableSubject.assertThatRunnable;

import android.graphics.Bitmap;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.piet.ui.RoundedCornerMaskCache.Corner;
import org.chromium.chrome.browser.feed.library.piet.ui.RoundedCornerMaskCache.RoundedCornerBitmaps;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests for the {@link RoundedCornerMaskCache}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class RoundedCornerMaskCacheTest {
    private RoundedCornerMaskCache mCache;

    @Before
    public void setUp() {
        mCache = new RoundedCornerMaskCache();
    }

    @Test
    public void testGetBitmaps_createsNewInstance() {
        RoundedCornerBitmaps masks = mCache.getMasks(16);

        for (int i = 0; i < 4; i++) {
            Bitmap mask = masks.get(i);
            assertThat(mask.getWidth()).isEqualTo(16);
            assertThat(mask.getHeight()).isEqualTo(16);
        }
    }

    @Test
    public void testGetBitmaps_differentRadii() {
        RoundedCornerBitmaps masksFive = mCache.getMasks(5);
        RoundedCornerBitmaps masksTen = mCache.getMasks(10);

        assertThat(masksFive).isNotEqualTo(masksTen);

        Bitmap maskFive = masksFive.get(Corner.TOP_LEFT);
        Bitmap maskTen = masksTen.get(Corner.TOP_LEFT);

        assertThat(maskFive).isNotEqualTo(maskTen);
        assertThat(maskFive.getWidth()).isEqualTo(5);
        assertThat(maskTen.getWidth()).isEqualTo(10);
    }

    @Test
    public void testGetBitmaps_cachesInstance() {
        RoundedCornerBitmaps masks1 = mCache.getMasks(16);

        RoundedCornerBitmaps masks2 = mCache.getMasks(16);

        assertThat(masks1).isSameInstanceAs(masks2);
    }

    @Test
    public void testPurge() {
        RoundedCornerBitmaps masks1 = mCache.getMasks(16);

        mCache.purge();

        RoundedCornerBitmaps masks2 = mCache.getMasks(16);

        assertThat(masks1).isNotSameInstanceAs(masks2);
    }

    @Test
    public void testBadCornerException() {
        RoundedCornerBitmaps masks = mCache.getMasks(16);

        assertThatRunnable(() -> masks.get(999))
                .throwsAnExceptionOfType(IllegalArgumentException.class);
    }

    @Test
    public void testCreatesOnlyRequestedCorners() {
        RoundedCornerBitmaps masks = mCache.getMasks(123);

        assertThat(masks.mMasks[Corner.TOP_LEFT]).isNull();
        assertThat(masks.mMasks[Corner.TOP_RIGHT]).isNull();
        assertThat(masks.mMasks[Corner.BOTTOM_LEFT]).isNull();
        assertThat(masks.mMasks[Corner.BOTTOM_RIGHT]).isNull();

        masks.get(Corner.TOP_RIGHT);

        assertThat(masks.mMasks[Corner.TOP_LEFT]).isNull();
        assertThat(masks.mMasks[Corner.TOP_RIGHT]).isNotNull();
        assertThat(masks.mMasks[Corner.BOTTOM_LEFT]).isNull();
        assertThat(masks.mMasks[Corner.BOTTOM_RIGHT]).isNull();
    }
}
