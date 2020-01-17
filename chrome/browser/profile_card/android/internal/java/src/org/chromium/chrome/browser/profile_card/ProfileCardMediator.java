// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profile_card.internal;

import static org.chromium.chrome.features.profile_card.ProfileCardProperties.DESCRIPTION;
import static org.chromium.chrome.features.profile_card.ProfileCardProperties.IS_DIALOG_VISIBLE;
import static org.chromium.chrome.features.profile_card.ProfileCardProperties.TITLE;

import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.ui.modelutil.PropertyModel;

/**
 * A mediator for the ProfileCard component, responsible for communicating
 * with the components' coordinator.
 * Reacts to changes in the backend and updates the model.
 * Receives events from the view (callback) and notifies the backend.
 */
class ProfileCardMediator {
    private static final String NAME = "name";
    private static final String DESCRIPTION_OBJ = "description";
    private static final String TEXT = "text";

    private final PropertyModel mModel;
    private final JSONObject mProfileCardData;

    ProfileCardMediator(PropertyModel model, JSONObject profileCardData) {
        mModel = model;
        mProfileCardData = profileCardData;
    }

    public void show() throws JSONException {
        // Set all properties.
        mModel.set(TITLE, mProfileCardData.getString(NAME));
        mModel.set(DESCRIPTION, mProfileCardData.getJSONObject(DESCRIPTION_OBJ).getString(TEXT));
        mModel.set(IS_DIALOG_VISIBLE, true);
    }
}
