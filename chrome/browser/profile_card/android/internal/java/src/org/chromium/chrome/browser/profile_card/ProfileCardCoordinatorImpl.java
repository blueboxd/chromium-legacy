// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profile_card.internal;

import android.view.View;

import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.Log;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.widget.ViewRectProvider;

/**
 * Implements ProfileCardCoordinator.
 * Talks to other components and decides when to show/update the profile card.
 */
public class ProfileCardCoordinatorImpl implements ProfileCardCoordinator {
    private static final String TAG = "ShowError";

    private final PropertyModel mModel;
    private final ProfileCardView mView;
    private final ProfileCardMediator mMediator;
    private final PropertyModelChangeProcessor mModelChangeProcessor;
    private final JSONObject mProfileCardData;

    @Override
    public void update(View anchorView, JSONObject profileCardData) {
        if (mProfileCardData == profileCardData) return;
        ViewRectProvider rectProvider = new ViewRectProvider(anchorView);
        mView = new ProfileCardView(anchorView.getContext(), anchorView, /*stringId=*/"",
                /*accessibilityStringId=*/"", rectProvider);
        mProfileCardData = profileCardData;
        mModel = new PropertyModel(ProfileCardProperties.ALL_KEYS);
        mModelChangeProcessor =
                PropertyModelChangeProcessor.create(mModel, mView, ProfileCardViewBinder::bind);
        mMediator = new ProfileCardMediator(mModel, profileCardData);
    }

    @Override
    public void show() {
        try {
            mMediator.show();
        } catch (JSONException e) {
            e.printStackTrace();
            Log.e(TAG, "Failed to show profile card.");
        }
    }
}
