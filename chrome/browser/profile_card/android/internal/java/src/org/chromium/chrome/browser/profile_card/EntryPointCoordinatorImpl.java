// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profile_card;

import android.content.Context;
import android.graphics.Bitmap;
import android.view.View;

/**
 * Implements EntryPointCoordinator.
 * Talks to other components and decides when to show/destroy the profile card entry point.
 * Initiates and shows the profile card.
 */
public class EntryPointCoordinatorImpl implements EntryPointCoordinator {
    private Bitmap mImageBitmap;
    private EntryPointView mView;
    private ProfileCardCoordinatorImpl mProfileCardCoordinator;

    @Override
    public void init(Context context, Bitmap bitmap) {
        mView = new EntryPointView(context);
        mImageBitmap = bitmap;
        mView.setIconBitmap(mImageBitmap);
        mView.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                showProfileCard();
            }
        });
    }

    @Override
    public void show() {
        mView.setVisibility(true);
    }

    @Override
    public void hide() {
        mView.setVisibility(false);
    }

    void showProfileCard() {
        mProfileCardCoordinator.init(mView, ProfileCardUtil.getProfileCardData());
        mProfileCardCoordinator.show();
    }
}
