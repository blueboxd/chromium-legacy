// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_check;

import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.CompromisedCredentialProperties.COMPROMISED_CREDENTIAL;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.HeaderProperties.CHECK_STATUS;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.ITEMS;

import org.chromium.chrome.browser.password_check.PasswordCheckProperties.CheckStatus;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * Contains the logic for the PasswordCheck component. It sets the state of the model and reacts to
 * events like clicks.
 */
class PasswordCheckMediator {
    private PropertyModel mModel;

    void initialize(PropertyModel model) {
        mModel = model;
    }

    void onCompromisedCredentialsAvailable(
            @CheckStatus int status, List<CompromisedCredential> credentials) {
        assert credentials != null;
        ListModel<ListItem> items = mModel.get(ITEMS);
        assert items.size() == 0;

        items.add(new ListItem(PasswordCheckProperties.ItemType.HEADER,
                new PropertyModel.Builder(PasswordCheckProperties.HeaderProperties.ALL_KEYS)
                        .with(CHECK_STATUS, status)
                        .build()));

        for (CompromisedCredential credential : credentials) {
            items.add(new ListItem(PasswordCheckProperties.ItemType.COMPROMISED_CREDENTIAL,
                    new PropertyModel
                            .Builder(PasswordCheckProperties.CompromisedCredentialProperties
                                             .ALL_KEYS)
                            .with(COMPROMISED_CREDENTIAL, credential)
                            .build()));
        }
    }
}
