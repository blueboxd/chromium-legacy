// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.content.Intent;
import android.net.Uri;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/**
 * Tests of {@link LensUtils}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class LensUtilsTest {
    /**
     * Test {@link LensUtils#isAgsaVersionBelowMinimum()} method.
     */
    @Test
    @SmallTest
    public void isAgsaVersionBelowMinimumTest() {
        Assert.assertFalse(LensUtils.isAgsaVersionBelowMinimum("8.19"));
        Assert.assertFalse(LensUtils.isAgsaVersionBelowMinimum("8.19.1"));
        Assert.assertFalse(LensUtils.isAgsaVersionBelowMinimum("8.30"));
        Assert.assertFalse(LensUtils.isAgsaVersionBelowMinimum("9.30"));

        Assert.assertTrue(LensUtils.isAgsaVersionBelowMinimum("8.1"));
        Assert.assertTrue(LensUtils.isAgsaVersionBelowMinimum("7.30"));
        Assert.assertTrue(LensUtils.isAgsaVersionBelowMinimum("8"));
    }

    /**
     * Test {@link LensUtils#getShareWithGoogleLensIntent()} method.
     */
    @Test
    @SmallTest
    public void getShareWithGoogleLensIntentTest() {
        Intent intentNoUri = LensUtils.getShareWithGoogleLensIntent(Uri.EMPTY);
        Assert.assertEquals("googleapp://lens", intentNoUri.getData().toString());
        Assert.assertEquals(Intent.ACTION_VIEW, intentNoUri.getAction());

        final String contentUrl = "content://image-url";
        Intent intentWithContentUri = LensUtils.getShareWithGoogleLensIntent(Uri.parse(contentUrl));
        Assert.assertEquals("googleapp://lens?LensBitmapUriKey=content%3A%2F%2Fimage-url",
                intentWithContentUri.getData().toString());
        Assert.assertEquals(Intent.ACTION_VIEW, intentWithContentUri.getAction());
    }
}