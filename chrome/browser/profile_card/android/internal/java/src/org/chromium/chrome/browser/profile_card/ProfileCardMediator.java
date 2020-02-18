// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profile_card;

import static org.chromium.chrome.browser.profile_card.ProfileCardProperties.AVATAR_BITMAP;
import static org.chromium.chrome.browser.profile_card.ProfileCardProperties.DESCRIPTION;
import static org.chromium.chrome.browser.profile_card.ProfileCardProperties.IS_VISIBLE;
import static org.chromium.chrome.browser.profile_card.ProfileCardProperties.POST_DATA_LIST;
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
    private final ProfileCardData mProfileCardData;

    ProfileCardMediator(PropertyModel model, ProfileCardData profileCardData) {
        mModel = model;
        mProfileCardData = profileCardData;
        mModel.set(AVATAR_BITMAP, mProfileCardData.getAvatarBitmap());
        mModel.set(TITLE, mProfileCardData.getTitle());
        mModel.set(DESCRIPTION, mProfileCardData.getDescription());
        mModel.set(POST_FREQUENCY, mProfileCardData.getPostFrequency());
        mModel.set(POST_DATA_LIST, mProfileCardData.getPostDataList());
    }

    public void show() {
        mModel.set(IS_VISIBLE, true);
    }
}
