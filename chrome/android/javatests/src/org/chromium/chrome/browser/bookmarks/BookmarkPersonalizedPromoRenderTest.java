// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.mockito.MockitoAnnotations.initMocks;

import android.view.View;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;
import org.mockito.Mock;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.signin.SigninActivityLauncherImpl;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.BookmarkTestRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.signin.ProfileDataSource;
import org.chromium.components.signin.test.util.FakeProfileDataSource;
import org.chromium.ui.test.util.UiDisableIf;

import java.io.IOException;

/**
 * Tests for the personalized signin promo on the Bookmarks page.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DisableIf.Device(type = {UiDisableIf.TABLET})
public class BookmarkPersonalizedPromoRenderTest {
    // FakeProfileDataSource is required to create the ProfileDataCache entry with sync_off badge
    // for Sync promo.
    private final AccountManagerTestRule mAccountManagerTestRule =
            new AccountManagerTestRule(new FakeProfileDataSource());

    private final ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private final BookmarkTestRule mBookmarkTestRule = new BookmarkTestRule();

    // Bookmarks need the FakeAccountManagerFacade initialized in AccountManagerTestRule.
    @Rule
    public final RuleChain chain = RuleChain.outerRule(mAccountManagerTestRule)
                                           .around(mActivityTestRule)
                                           .around(mBookmarkTestRule);

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus().build();

    @Mock
    private SigninActivityLauncherImpl mMockSigninActivityLauncherImpl;

    @Before
    public void setUp() {
        initMocks(this);
        mAccountManagerTestRule.addAccount(new ProfileDataSource.ProfileData(
                "test@gmail.com", null, "Full Name", "Given Name"));
        SigninActivityLauncherImpl.setLauncherForTest(mMockSigninActivityLauncherImpl);
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    @After
    public void tearDown() {
        SigninActivityLauncherImpl.setLauncherForTest(null);
        BookmarkPromoHeader.forcePromoStateForTests(null);
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testPersonalizedSigninPromoInBookmarkPage() throws IOException {
        BookmarkPromoHeader.forcePromoStateForTests(
                BookmarkPromoHeader.PromoState.PROMO_SIGNIN_PERSONALIZED);
        mBookmarkTestRule.showBookmarkManager(mActivityTestRule.getActivity());
        mRenderTestRule.render(getPersonalizedPromoView(), "bookmark_personalized_signin_promo");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testPersonalizedSyncPromoInBookmarkPage() throws Exception {
        BookmarkPromoHeader.forcePromoStateForTests(
                BookmarkPromoHeader.PromoState.PROMO_SYNC_PERSONALIZED);
        mBookmarkTestRule.showBookmarkManager(mActivityTestRule.getActivity());
        mRenderTestRule.render(getPersonalizedPromoView(), "bookmark_personalized_sync_promo");
    }

    private View getPersonalizedPromoView() {
        BookmarkActivity bookmarkActivity = mBookmarkTestRule.getBookmarkActivity();
        Assert.assertNotNull("BookmarkActivity should not be null", bookmarkActivity);
        return bookmarkActivity.findViewById(R.id.signin_promo_view_container);
    }
}
