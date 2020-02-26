// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;

import android.content.Intent;
import android.net.Uri;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.signin.base.CoreAccountId;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.identitymanager.IdentityManagerJni;
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

    @Rule
    public final JniMocker mocker = new JniMocker();

    @Mock
    private IdentityManager.Natives mIdentityManagerNativesMock;

    private final CoreAccountInfo mCoreAccountInfo =
            new CoreAccountInfo(new CoreAccountId("gaia_id"), "test@gmail.com", "gaia_id");

    @Before
    public void setUp() {
        initMocks(this);
        mocker.mock(IdentityManagerJni.TEST_HOOKS, mIdentityManagerNativesMock);
    }

    /**
     * Test {@link LensUtils#getShareWithGoogleLensIntent()} method when user is signed
     * in.
     */
    @Test
    @SmallTest
    public void getShareWithGoogleLensIntentSignedInTest() {
        when(mIdentityManagerNativesMock.getPrimaryAccountInfo(anyLong()))
                .thenReturn(mCoreAccountInfo);
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
        when(mIdentityManagerNativesMock.getPrimaryAccountInfo(anyLong()))
                .thenReturn(mCoreAccountInfo);
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