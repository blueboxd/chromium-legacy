// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.content.Intent;
import android.net.Uri;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.signin.SigninTestUtil;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests of {@link LensUtils}.
 * TODO(https://crbug.com/1054738): Reimplement LensUtilsTest as robolectric tests
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
public class LensUtilsTest {
    @Rule
    public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    /**
     * Test {@link LensUtils#getShareWithGoogleLensIntent()} method when user is signed
     * in.
     */
    @Test
    @SmallTest
    public void getShareWithGoogleLensIntentSignedInTest() {
        SigninTestUtil.signIn(SigninTestUtil.addTestAccount("test@gmail.com"));

        Intent intentNoUri =
                getShareWithGoogleLensIntentOnUiThread(Uri.EMPTY, /* isIncognito= */ false, 1234L);
        Assert.assertEquals("Intent without image has incorrect URI", "googleapp://lens",
                intentNoUri.getData().toString());
        Assert.assertEquals("Intent without image has incorrect action", Intent.ACTION_VIEW,
                intentNoUri.getAction());

        final String contentUrl = "content://image-url";
        Intent intentWithContentUri = getShareWithGoogleLensIntentOnUiThread(
                Uri.parse(contentUrl), /* isIncognito= */ false, 1234L);
        Assert.assertEquals("Intent with image has incorrect URI",
                "googleapp://lens?LensBitmapUriKey=content%3A%2F%2Fimage-url&AccountNameUriKey="
                        + "test%40gmail.com&IncognitoUriKey=false&ActivityLaunchTimestampNanos=1234",
                intentWithContentUri.getData().toString());
        Assert.assertEquals("Intent with image has incorrect action", Intent.ACTION_VIEW,
                intentWithContentUri.getAction());
    }

    /**
     * Test {@link LensUtils#getShareWithGoogleLensIntent()} method when user is
     * incognito.
     */
    @Test
    @SmallTest
    public void getShareWithGoogleLensIntentIncognitoTest() {
        SigninTestUtil.signIn(SigninTestUtil.addTestAccount("test@gmail.com"));
        Intent intentNoUri =
                getShareWithGoogleLensIntentOnUiThread(Uri.EMPTY, /* isIncognito= */ true, 1234L);
        Assert.assertEquals("Intent without image has incorrect URI", "googleapp://lens",
                intentNoUri.getData().toString());
        Assert.assertEquals("Intent without image has incorrect action", Intent.ACTION_VIEW,
                intentNoUri.getAction());

        final String contentUrl = "content://image-url";
        Intent intentWithContentUri = getShareWithGoogleLensIntentOnUiThread(
                Uri.parse(contentUrl), /* isIncognito= */ true, 1234L);
        // The account name should not be included in the intent because the uesr is incognito.
        Assert.assertEquals("Intent with image has incorrect URI",
                "googleapp://lens?LensBitmapUriKey=content%3A%2F%2Fimage-url&AccountNameUriKey="
                        + "&IncognitoUriKey=true&ActivityLaunchTimestampNanos=1234",
                intentWithContentUri.getData().toString());
        Assert.assertEquals("Intent with image has incorrect action", Intent.ACTION_VIEW,
                intentWithContentUri.getAction());
    }

    /**
     * Test {@link LensUtils#getShareWithGoogleLensIntent()} method when user is not
     * signed in.
     */
    @Test
    @SmallTest
    public void getShareWithGoogleLensIntentNotSignedInTest() {
        Intent intentNoUri =
                getShareWithGoogleLensIntentOnUiThread(Uri.EMPTY, /* isIncognito= */ false, 1234L);
        Assert.assertEquals("Intent without image has incorrect URI", "googleapp://lens",
                intentNoUri.getData().toString());
        Assert.assertEquals("Intent without image has incorrect action", Intent.ACTION_VIEW,
                intentNoUri.getAction());

        final String contentUrl = "content://image-url";
        Intent intentWithContentUri = getShareWithGoogleLensIntentOnUiThread(
                Uri.parse(contentUrl), /* isIncognito= */ false, 1234L);
        Assert.assertEquals("Intent with image has incorrect URI",
                "googleapp://lens?LensBitmapUriKey=content%3A%2F%2Fimage-url&AccountNameUriKey="
                        + "&IncognitoUriKey=false&ActivityLaunchTimestampNanos=1234",
                intentWithContentUri.getData().toString());
        Assert.assertEquals("Intent with image has incorrect action", Intent.ACTION_VIEW,
                intentWithContentUri.getAction());
    }

    /**
     * Test {@link LensUtils#getShareWithGoogleLensIntent()} method when the timestamp was
     * unexpectedly 0.
     */
    @Test
    @SmallTest
    public void getShareWithGoogleLensIntentZeroTimestampTest() {
        final String contentUrl = "content://image-url";
        Intent intentWithContentUriZeroTimestamp = getShareWithGoogleLensIntentOnUiThread(
                Uri.parse(contentUrl), /* isIncognito= */ false, 0L);
        Assert.assertEquals("Intent with image has incorrect URI",
                "googleapp://lens?LensBitmapUriKey=content%3A%2F%2Fimage-url&AccountNameUriKey="
                        + "&IncognitoUriKey=false&ActivityLaunchTimestampNanos=0",
                intentWithContentUriZeroTimestamp.getData().toString());
    }

    private Intent getShareWithGoogleLensIntentOnUiThread(
            Uri imageUri, boolean isIncognito, long currentTimeNanos) {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                ()
                        -> LensUtils.getShareWithGoogleLensIntent(
                                imageUri, isIncognito, currentTimeNanos));
    }
}