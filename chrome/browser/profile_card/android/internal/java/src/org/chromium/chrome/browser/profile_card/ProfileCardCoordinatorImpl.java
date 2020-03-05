// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profile_card;

import android.view.View;

import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.widget.ViewRectProvider;

/**
 * Implements ProfileCardCoordinator.
 * Talks to other components and decides when to show/update the profile card.
 */
public class ProfileCardCoordinatorImpl implements ProfileCardCoordinator {
    private PropertyModel mModel;
    private ProfileCardView mView;
    private ProfileCardMediator mMediator;
    private PropertyModelChangeProcessor mModelChangeProcessor;
    private CreatorMetadata mCreatorMetadata;

    @Override
    public void init(View anchorView, CreatorMetadata creatorMetadata) {
        ViewRectProvider rectProvider = new ViewRectProvider(anchorView);
        mView = new ProfileCardView(anchorView.getContext());
        mCreatorMetadata = creatorMetadata;
        mModel = new PropertyModel(ProfileCardProperties.ALL_KEYS);
        mModelChangeProcessor =
                PropertyModelChangeProcessor.create(mModel, mView, ProfileCardViewBinder::bind);
        mMediator = new ProfileCardMediator(mModel, creatorMetadata);
    }

    @Override
    public void show() {
        mMediator.show();
    }

    @Override
    public void hide() {
        mMediator.hide();
    }
}
