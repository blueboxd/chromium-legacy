// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profile_card;

import static org.chromium.chrome.browser.profile_card.ProfileCardProperties.AVATAR_BITMAP;
import static org.chromium.chrome.browser.profile_card.ProfileCardProperties.DESCRIPTION;
import static org.chromium.chrome.browser.profile_card.ProfileCardProperties.IS_VISIBLE;
import static org.chromium.chrome.browser.profile_card.ProfileCardProperties.POST_FREQUENCY;
import static org.chromium.chrome.browser.profile_card.ProfileCardProperties.TITLE;

import org.chromium.ui.modelutil.PropertyModel;

/**
 * A mediator for the ProfileCard component, responsible for communicating
 * with the components' coordinator.
 * Reacts to changes in the backend and updates the model.
 * Receives events from the view (callback) and notifies the backend.
 */
class ProfileCardMediator {
    private final PropertyModel mModel;
    private final CreatorMetadata mCreatorMetadata;

    ProfileCardMediator(PropertyModel model, CreatorMetadata creatorMetadata) {
        mModel = model;
        mCreatorMetadata = creatorMetadata;
        mModel.set(AVATAR_BITMAP, mCreatorMetadata.getAvatarBitmap());
        mModel.set(TITLE, mCreatorMetadata.getTitle());
        mModel.set(DESCRIPTION, mCreatorMetadata.getDescription());
        mModel.set(POST_FREQUENCY, mCreatorMetadata.getPostFrequency());
    }

    public void show() {
        mModel.set(IS_VISIBLE, true);
    }

    public void hide() {
        mModel.set(IS_VISIBLE, false);
    }
}
