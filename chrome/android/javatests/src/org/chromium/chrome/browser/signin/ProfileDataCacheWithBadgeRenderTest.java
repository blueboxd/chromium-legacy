// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.support.test.filters.MediumTest;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;

import androidx.annotation.DrawableRes;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.signin.ProfileDataSource;
import org.chromium.components.signin.test.util.FakeProfileDataSource;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.DummyUiActivityTestCase;
import org.chromium.ui.widget.ChromeImageView;

import java.io.IOException;

/**
 * Tests for ProfileDataCache with a badge. Leverages RenderTest instead of reimplementing
 * bitmap comparison to simplify access to the compared images on buildbots (via result_details).
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class ProfileDataCacheWithBadgeRenderTest extends DummyUiActivityTestCase {
    @Rule
    public ChromeRenderTestRule mRenderTestRule = new ChromeRenderTestRule();

    private FrameLayout mContentView;
    private ImageView mImageView;
    private FakeProfileDataSource mProfileDataSource;
    private ProfileDataCache mProfileDataCache;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Activity activity = getActivity();
            mContentView = new FrameLayout(activity);
            mImageView = new ChromeImageView(activity);
            mContentView.addView(mImageView, ViewGroup.LayoutParams.WRAP_CONTENT,
                    ViewGroup.LayoutParams.WRAP_CONTENT);
            activity.setContentView(mContentView);

            mProfileDataSource = new FakeProfileDataSource();
        });
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testProfileDataCacheWithChildBadge() throws IOException {
        setUpProfileDataCache(R.drawable.ic_account_child_20dp);

        mRenderTestRule.render(mImageView, "profile_data_cache_with_child_badge");
    }

    private void setUpProfileDataCache(@DrawableRes int badgeResId) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mProfileDataCache = ProfileDataCache.createProfileDataCache(
                    getActivity(), badgeResId, mProfileDataSource);
            // ProfileDataCache only populates the cache when an observer is added.
            mProfileDataCache.addObserver(accountId -> {});

            String accountName = "test@example.com";
            ProfileDataSource.ProfileData profileData = new ProfileDataSource.ProfileData(
                    accountName, createAvatar(), "Full Name", "Given Name");
            mProfileDataSource.setProfileData(accountName, profileData);
            mImageView.setImageDrawable(
                    mProfileDataCache.getProfileDataOrDefault(accountName).getImage());
        });
    }

    /**
     * Creates a simple dummy bitmap to use as the avatar picture.
     */
    private Bitmap createAvatar() {
        final int avatarSize =
                getActivity().getResources().getDimensionPixelSize(R.dimen.user_picture_size);
        Assert.assertTrue("avatarSize must not be 0", avatarSize > 0);
        Bitmap result = Bitmap.createBitmap(avatarSize, avatarSize, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(result);
        canvas.drawColor(Color.RED);

        Paint paint = new Paint();
        paint.setAntiAlias(true);

        paint.setColor(Color.BLUE);
        canvas.drawCircle(0, 0, avatarSize, paint);

        return result;
    }
}
