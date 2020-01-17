// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profile_card.internal;

import static org.chromium.chrome.features.profile_card.ProfileCardProperties.DESCRIPTION;
import static org.chromium.chrome.features.profile_card.ProfileCardProperties.IS_DIALOG_VISIBLE;
import static org.chromium.chrome.features.profile_card.ProfileCardProperties.TITLE;

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
        if (TITLE == propertyKey) {
            view.setTitle(model.get(TITLE));
        } else if (DESCRIPTION == propertyKey) {
            view.setDescription(model.get(DESCRIPTION));
        } else if (IS_DIALOG_VISIBLE == propertyKey) {
            view.setVisibility(model.get(IS_DIALOG_VISIBLE));
        }
    }
}
