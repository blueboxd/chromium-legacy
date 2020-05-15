// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.promo;

import android.app.Activity;
import android.support.test.filters.SmallTest;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.widget.FrameLayout;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.DummyUiActivityTestCase;

import java.util.concurrent.TimeoutException;

/**
 * Tests targeting functionality to track impression on PromoCard component.
 * TODO(wenyufu): Add the test when the primary button is hidden initially.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class PromoCardImpressionTest extends DummyUiActivityTestCase {
    private PropertyModel mModel;
    private PromoCardCoordinator mCoordinator;

    private FrameLayout mContent;

    private final CallbackHelper mPromoSeenCallback = new CallbackHelper();

    private void setUpPromoCard(boolean trackPrimary, boolean hidePromo) {
        mModel = new PropertyModel.Builder(PromoCardProperties.ALL_KEYS)
                         .with(PromoCardProperties.IMPRESSION_SEEN_CALLBACK,
                                 mPromoSeenCallback::notifyCalled)
                         .with(PromoCardProperties.IS_IMPRESSION_ON_PRIMARY_BUTTON, trackPrimary)
                         .with(PromoCardProperties.TITLE, "Title")
                         .with(PromoCardProperties.DESCRIPTION, "Description")
                         .with(PromoCardProperties.PRIMARY_BUTTON_TEXT, "Primary")
                         .with(PromoCardProperties.HAS_SECONDARY_BUTTON, false)
                         .build();

        Activity activity = getActivity();
        mCoordinator = new PromoCardCoordinator(activity, mModel, "impression-test");
        View promoView = mCoordinator.getView();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            if (hidePromo) promoView.setVisibility(View.INVISIBLE);

            // Set the content and add the promo card into the window
            mContent = new FrameLayout(activity);
            activity.setContentView(mContent);

            mContent.addView(promoView,
                    new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
        });
    }

    @Test
    @SmallTest
    public void testImpression_Card_Seen() throws TimeoutException {
        int initCount = mPromoSeenCallback.getCallCount();
        setUpPromoCard(false, false);
        mPromoSeenCallback.waitForCallback("PromoCard is never seen.", initCount);
    }

    @Test
    @SmallTest
    public void testImpression_PrimaryButton_Seen() throws TimeoutException {
        int initCount = mPromoSeenCallback.getCallCount();
        setUpPromoCard(true, false);
        mPromoSeenCallback.waitForCallback("PromoCard's primary button is never seen.", initCount);
    }

    @Test
    @SmallTest
    public void testImpression_Card_Hide() throws TimeoutException {
        int initCount = mPromoSeenCallback.getCallCount();
        setUpPromoCard(false, true);
        Assert.assertEquals(
                "Promo should not be seen yet.", initCount, mPromoSeenCallback.getCallCount());

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mCoordinator.getView().setVisibility(View.VISIBLE); });
        mPromoSeenCallback.waitForCallback("PromoCard is never seen.", initCount);
    }
}
