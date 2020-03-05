// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profile_card;

import android.content.Context;
import android.view.View;

import org.chromium.content_public.browser.NavigationHandle;

/**
 * Implements EntryPointCoordinator.
 * Talks to other components and decides when to show/destroy the profile card entry point.
 * Initiates and shows the profile card.
 */
public class EntryPointCoordinatorImpl implements EntryPointCoordinator {
    private EntryPointView mView;
    private ProfileCardCoordinatorImpl mProfileCardCoordinator;
    private CreatorMetadata mCreatorMetadata;

    @Override
    public void init(Context context) {
        mView = new EntryPointView(context);
        mView.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                showProfileCard();
            }
        });
    }

    @Override
    public void maybeShow(NavigationHandle navigationHandle) {
        if (!ProfileCardUtil.shouldShowEntryPoint()) return;
        ProfileCardUtil.getCreatorMetaData(data -> {
            if (data != null) {
                mCreatorMetadata = data;
                mView.setVisibility(true);
            }
        }, navigationHandle);
    }

    @Override
    public void hide() {
        mView.setVisibility(false);
    }

    void showProfileCard() {
        mProfileCardCoordinator.init(mView, mCreatorMetadata);
        mProfileCardCoordinator.show();
    }
}
