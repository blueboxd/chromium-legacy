// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import android.app.Activity;
import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.ViewGroup;

import org.chromium.chrome.browser.ntp.cards.SignInPromo;
import org.chromium.chrome.browser.signin.ProfileDataCache;
import org.chromium.chrome.browser.signin.SigninPromoController;
import org.chromium.chrome.browser.signin.SigninPromoUtil;
import org.chromium.chrome.start_surface.R;
import org.chromium.components.signin.metrics.SigninAccessPoint;

/** The coordinator to control the loading feed surface. */
public class FeedLoadingCoordinator {
    private final Context mContext;
    private final ViewGroup mParentView;

    public FeedLoadingCoordinator(
            Activity activity, ViewGroup parentView, boolean isBackgroundDark) {
        mParentView = parentView;
        mContext = new ContextThemeWrapper(activity,
                (isBackgroundDark ? org.chromium.chrome.feed.R.style.Dark
                                  : org.chromium.chrome.feed.R.style.Light));
    }

    public void setUpLoadingView() {
        FeedLoadingLayout feedLoadingView =
                (FeedLoadingLayout) LayoutInflater.from(mContext).inflate(
                        R.layout.feed_loading_layout, null, false);
        mParentView.addView(feedLoadingView);

        if (SignInPromo.shouldCreatePromo()) {
            SigninPromoController signinPromoController =
                    new SigninPromoController(SigninAccessPoint.NTP_CONTENT_SUGGESTIONS);
            int imageSize =
                    mContext.getResources().getDimensionPixelSize(R.dimen.user_picture_size);
            ProfileDataCache profileDataCache = new ProfileDataCache(mContext, imageSize);
            SigninPromoUtil.setupPromoViewFromCache(signinPromoController, profileDataCache,
                    feedLoadingView.getSigninPromoView(), null);
        }
    }
}
