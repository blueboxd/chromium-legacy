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
import org.chromium.chrome.browser.flags.ChromeFeatureList;
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

        Intent intentNoUri = getShareWithGoogleLensIntentOnUiThread(Uri.EMPTY,
                /* isIncognito= */ false, 1234L, /* srcUrl */ "", /* titleOrAltText */ "");
        Assert.assertEquals("Intent without image has incorrect URI", "googleapp://lens",
                intentNoUri.getData().toString());
        Assert.assertEquals("Intent without image has incorrect action", Intent.ACTION_VIEW,
                intentNoUri.getAction());

        final String contentUrl = "content://image-url";
        Intent intentWithContentUri = getShareWithGoogleLensIntentOnUiThread(Uri.parse(contentUrl),
                /* isIncognito= */ false, 1234L, /* srcUrl */ "", /* titleOrAltText */ "");
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
        Intent intentNoUri = getShareWithGoogleLensIntentOnUiThread(Uri.EMPTY,
                /* isIncognito= */ true, 1234L, /* srcUrl */ "", /* titleOrAltText */ "");
        Assert.assertEquals("Intent without image has incorrect URI", "googleapp://lens",
                intentNoUri.getData().toString());
        Assert.assertEquals("Intent without image has incorrect action", Intent.ACTION_VIEW,
                intentNoUri.getAction());

        final String contentUrl = "content://image-url";
        Intent intentWithContentUri = getShareWithGoogleLensIntentOnUiThread(Uri.parse(contentUrl),
                /* isIncognito= */ true, 1234L, /* srcUrl */ "", /* titleOrAltText */ "");
        // The account name should not be included in the intent because the uesr is incognito.
        Assert.assertEquals("Intent with image has incorrect URI",
                "googleapp://lens?LensBitmapUriKey=content%3A%2F%2Fimage-url&AccountNameUriKey="
                        + "&IncognitoUriKey=true&ActivityLaunchTimestampNanos=1234",
                intentWithContentUri.getData().toString());
        Assert.assertEquals("Intent with image has incorrect action", Intent.ACTION_VIEW,
                intentWithContentUri.getAction());
    }

    /**
     * Test {@link LensUtils#getShareWithGoogleLensIntent()} method and that variations
     * are added to the URI.
     */
    @Test
    @SmallTest
    public void getShareWithGoogleLensIntentWithVariationsTest() {
        LensUtils.setFakeVariationsForTesting(" 123 456 ");
        SigninTestUtil.signIn(SigninTestUtil.addTestAccount("test@gmail.com"));

        final String contentUrl = "content://image-url";
        Intent intentWithContentUri = getShareWithGoogleLensIntentOnUiThread(Uri.parse(contentUrl),
                /* isIncognito= */ false, 1234L, /* srcUrl */ "",
                /* titleOrAltText */ "");
        // The account name should not be included in the intent because the uesr is incognito.
        Assert.assertEquals("Intent with image has incorrect URI",
                "googleapp://lens?LensBitmapUriKey=content%3A%2F%2Fimage-url&AccountNameUriKey="
                        + "test%40gmail.com&IncognitoUriKey=false&ActivityLaunchTimestampNanos="
                        + "1234&Gid=123%20456",
                intentWithContentUri.getData().toString());
        Assert.assertEquals("Intent with image has incorrect action", Intent.ACTION_VIEW,
                intentWithContentUri.getAction());
    }

    /**
     * Test {@link LensUtils#getShareWithGoogleLensIntent()} method and that variations
     * are not sent when the user is incognito.
     */
    @Test
    @SmallTest
    public void getShareWithGoogleLensIntentWithVariationsIncognitoTest() {
        LensUtils.setFakeVariationsForTesting(" 123 456 ");
        SigninTestUtil.signIn(SigninTestUtil.addTestAccount("test@gmail.com"));

        final String contentUrl = "content://image-url";
        Intent intentWithContentUri = getShareWithGoogleLensIntentOnUiThread(Uri.parse(contentUrl),
                /* isIncognito= */ true, 1234L, /* srcUrl */ "",
                /* titleOrAltText */ "");
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
        Intent intentNoUri = getShareWithGoogleLensIntentOnUiThread(Uri.EMPTY,
                /* isIncognito= */ false, 1234L, /* srcUrl */ "", /* titleOrAltText */ "");
        Assert.assertEquals("Intent without image has incorrect URI", "googleapp://lens",
                intentNoUri.getData().toString());
        Assert.assertEquals("Intent without image has incorrect action", Intent.ACTION_VIEW,
                intentNoUri.getAction());

        final String contentUrl = "content://image-url";
        Intent intentWithContentUri = getShareWithGoogleLensIntentOnUiThread(Uri.parse(contentUrl),
                /* isIncognito= */ false, 1234L, /* srcUrl */ "", /* titleOrAltText */ "");
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
        Intent intentWithContentUriZeroTimestamp =
                getShareWithGoogleLensIntentOnUiThread(Uri.parse(contentUrl),
                        /* isIncognito= */ false, 0L, /* srcUrl */ "", /* titleOrAltText */ "");
        Assert.assertEquals("Intent with image has incorrect URI",
                "googleapp://lens?LensBitmapUriKey=content%3A%2F%2Fimage-url&AccountNameUriKey="
                        + "&IncognitoUriKey=false&ActivityLaunchTimestampNanos=0",
                intentWithContentUriZeroTimestamp.getData().toString());
    }

    private Intent getShareWithGoogleLensIntentOnUiThread(Uri imageUri, boolean isIncognito,
            long currentTimeNanos, String srcUrl, String titleOrAltText) {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                ()
                        -> LensUtils.getShareWithGoogleLensIntent(
                                imageUri, isIncognito, currentTimeNanos, srcUrl, titleOrAltText));
    }

    /**
     * Test {@link LensUtils#getShareWithGoogleLensIntent()} method when alt text is available but
     * not enabled by finch.
     */
    @Test
    @SmallTest
    public void getShareWithGoogleLensIntentAltDisabledNotAddedTest() {
        final String contentUrl = "content://image-url";
        Intent intentWithContentUri = getShareWithGoogleLensIntentOnUiThread(Uri.parse(contentUrl),
                /* isIncognito= */ false, 1234L, /* srcUrl */ "",
                /* titleOrAltText */ "An image description.");
        // The account name should not be included in the intent because the uesr is incognito.
        Assert.assertEquals("Intent with image has incorrect URI",
                "googleapp://lens?LensBitmapUriKey=content%3A%2F%2Fimage-url&AccountNameUriKey="
                        + "&IncognitoUriKey=false&ActivityLaunchTimestampNanos=1234",
                intentWithContentUri.getData().toString());
        Assert.assertEquals("Intent with image has incorrect action", Intent.ACTION_VIEW,
                intentWithContentUri.getAction());
    }

    /**
     * Test {@link LensUtils#getShareWithGoogleLensIntent()} method when alt text is available and
     * enabled by finch, but incognito.
     */
    @CommandLineFlags.Add({"enable-features="
                    + ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS + "<FakeStudyName",
            "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:sendAlt/true"})
    @Test
    @SmallTest
    public void
    getShareWithGoogleLensIntentAltEnabledIncognitoNotAddedTest() {
        final String contentUrl = "content://image-url";
        Intent intentWithContentUri = getShareWithGoogleLensIntentOnUiThread(Uri.parse(contentUrl),
                /* isIncognito= */ true, 1234L, /* srcUrl */ "",
                /* titleOrAltText */ "An image description.");
        // The account name should not be included in the intent because the uesr is incognito.
        Assert.assertEquals("Intent with image has incorrect URI",
                "googleapp://lens?LensBitmapUriKey=content%3A%2F%2Fimage-url&AccountNameUriKey="
                        + "&IncognitoUriKey=true&ActivityLaunchTimestampNanos=1234",
                intentWithContentUri.getData().toString());
        Assert.assertEquals("Intent with image has incorrect action", Intent.ACTION_VIEW,
                intentWithContentUri.getAction());
    }

    /**
     * Test {@link LensUtils#getShareWithGoogleLensIntent()} method when alt text is available and
     * enabled by finch.
     */
    @CommandLineFlags.Add({"enable-features="
                    + ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS + "<FakeStudyName",
            "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:sendAlt/true"})
    @Test
    @SmallTest
    public void
    getShareWithGoogleLensIntentAltEnabledAddedTest() {
        final String contentUrl = "content://image-url";
        Intent intentWithContentUri = getShareWithGoogleLensIntentOnUiThread(Uri.parse(contentUrl),
                /* isIncognito= */ false, 1234L, /* srcUrl */ "",
                /* titleOrAltText */ "An image description.");
        // The account name should not be included in the intent because the uesr is incognito.
        Assert.assertEquals("Intent with image has incorrect URI",
                "googleapp://lens?LensBitmapUriKey=content%3A%2F%2Fimage-url&AccountNameUriKey="
                        + "&IncognitoUriKey=false&ActivityLaunchTimestampNanos=1234&ImageAlt="
                        + "An%20image%20description.",
                intentWithContentUri.getData().toString());
        Assert.assertEquals("Intent with image has incorrect action", Intent.ACTION_VIEW,
                intentWithContentUri.getAction());
    }

    /**
     * Test {@link LensUtils#getShareWithGoogleLensIntent()} method when image src is available but
     * not enabled by finch.
     */
    @Test
    @SmallTest
    public void getShareWithGoogleLensIntentSrcDisabledNotAddedTest() {
        final String contentUrl = "content://image-url";
        Intent intentWithContentUri = getShareWithGoogleLensIntentOnUiThread(Uri.parse(contentUrl),
                /* isIncognito= */ false, 1234L, /* srcUrl */ "http://www.google.com?key=val",
                /* titleOrAltText */ "");
        // The account name should not be included in the intent because the uesr is incognito.
        Assert.assertEquals("Intent with image has incorrect URI",
                "googleapp://lens?LensBitmapUriKey=content%3A%2F%2Fimage-url&AccountNameUriKey="
                        + "&IncognitoUriKey=false&ActivityLaunchTimestampNanos=1234",
                intentWithContentUri.getData().toString());
        Assert.assertEquals("Intent with image has incorrect action", Intent.ACTION_VIEW,
                intentWithContentUri.getAction());
    }

    /**
     * Test {@link LensUtils#getShareWithGoogleLensIntent()} method when image src is available and
     * enabled by finch.
     */
    @CommandLineFlags.Add({"enable-features="
                    + ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS + "<FakeStudyName",
            "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:sendSrc/true"})
    @Test
    @SmallTest
    public void
    getShareWithGoogleLensIntentSrcEnabledAndAddedTest() {
        final String contentUrl = "content://image-url";
        Intent intentWithContentUri = getShareWithGoogleLensIntentOnUiThread(Uri.parse(contentUrl),
                /* isIncognito= */ false, 1234L, /* srcUrl */ "http://www.google.com?key=val",
                /* titleOrAltText */ "");
        // The account name should not be included in the intent because the uesr is incognito.
        Assert.assertEquals("Intent with image has incorrect URI",
                "googleapp://lens?LensBitmapUriKey=content%3A%2F%2Fimage-url&AccountNameUriKey="
                        + "&IncognitoUriKey=false&ActivityLaunchTimestampNanos=1234&ImageSrc="
                        + "http%3A%2F%2Fwww.google.com%3Fkey%3Dval",
                intentWithContentUri.getData().toString());
        Assert.assertEquals("Intent with image has incorrect action", Intent.ACTION_VIEW,
                intentWithContentUri.getAction());
    }

    /**
     * Test {@link LensUtils#getShareWithGoogleLensIntent()} method when image src is available,
     * enabled by finch, but user is incognito.
     */
    @CommandLineFlags.Add({"enable-features="
                    + ChromeFeatureList.CONTEXT_MENU_SEARCH_WITH_GOOGLE_LENS + "<FakeStudyName",
            "force-fieldtrials=FakeStudyName/Enabled",
            "force-fieldtrial-params=FakeStudyName.Enabled:sendSrc/true"})
    @Test
    @SmallTest
    public void
    getShareWithGoogleLensIntentSrcEnabledIncognitoNotAddedTest() {
        final String contentUrl = "content://image-url";
        Intent intentWithContentUri = getShareWithGoogleLensIntentOnUiThread(Uri.parse(contentUrl),
                /* isIncognito= */ true, 1234L, /* srcUrl */ "http://www.google.com?key=val",
                /* titleOrAltText */ "");
        // The account name should not be included in the intent because the uesr is incognito.
        Assert.assertEquals("Intent with image has incorrect URI",
                "googleapp://lens?LensBitmapUriKey=content%3A%2F%2Fimage-url&AccountNameUriKey="
                        + "&IncognitoUriKey=true&ActivityLaunchTimestampNanos=1234",
                intentWithContentUri.getData().toString());
        Assert.assertEquals("Intent with image has incorrect action", Intent.ACTION_VIEW,
                intentWithContentUri.getAction());
    }
}