// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profile_card;

import static org.chromium.chrome.browser.profile_card.ProfileCardProperties.AVATAR_BITMAP;
import static org.chromium.chrome.browser.profile_card.ProfileCardProperties.DESCRIPTION;
import static org.chromium.chrome.browser.profile_card.ProfileCardProperties.IS_VISIBLE;
import static org.chromium.chrome.browser.profile_card.ProfileCardProperties.POST_DATA_LIST;
import static org.chromium.chrome.browser.profile_card.ProfileCardProperties.POST_FREQUENCY;
import static org.chromium.chrome.browser.profile_card.ProfileCardProperties.TITLE;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

class ProfileCardViewBinder {
    /*
     * Bind the given model to the given view, updating the payload in propertyKey.
     * @param model The model to use.
     * @param view The View to use.
     * @param propertyKey The key for the property to update for.
     */
    public static void bind(PropertyModel model, ProfileCardView view, PropertyKey propertyKey) {
        if (AVATAR_BITMAP == propertyKey) {
            view.setAvatarBitmap(model.get(AVATAR_BITMAP));
        } else if (TITLE == propertyKey) {
            view.setTitle(model.get(TITLE));
        } else if (DESCRIPTION == propertyKey) {
            view.setDescription(model.get(DESCRIPTION));
        } else if (POST_FREQUENCY == propertyKey) {
            view.setPostFrequency(model.get(POST_FREQUENCY));
        } else if (POST_DATA_LIST == propertyKey) {
            view.setPosts(model.get(POST_DATA_LIST));
        } else if (IS_VISIBLE == propertyKey) {
            view.setVisibility(model.get(IS_VISIBLE));
        }
    }
}
