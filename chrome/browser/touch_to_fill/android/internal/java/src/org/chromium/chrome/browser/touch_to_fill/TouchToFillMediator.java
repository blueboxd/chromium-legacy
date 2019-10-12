// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill;

import static org.chromium.chrome.browser.touch_to_fill.CredentialProperties.CREDENTIAL;
import static org.chromium.chrome.browser.touch_to_fill.CredentialProperties.DEFAULT_ITEM_TYPE;
import static org.chromium.chrome.browser.touch_to_fill.CredentialProperties.FAVICON;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CREDENTIAL_LIST;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.FORMATTED_URL;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.ORIGIN_SECURE;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.VISIBLE;

import androidx.annotation.Px;

import org.chromium.chrome.browser.touch_to_fill.data.Credential;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * Contains the logic for the TouchToFill component. It sets the state of the model and reacts to
 * events like clicks.
 */
class TouchToFillMediator implements TouchToFillProperties.ViewEventListener {
    private TouchToFillComponent.Delegate mDelegate;
    private PropertyModel mModel;
    private @Px int mDesiredFaviconSize;

    void initialize(TouchToFillComponent.Delegate delegate, PropertyModel model,
            @Px int desiredFaviconSize) {
        assert delegate != null;
        mDelegate = delegate;
        mModel = model;
        mDesiredFaviconSize = desiredFaviconSize;
    }

    void showCredentials(
            String formattedUrl, boolean isOriginSecure, List<Credential> credentials) {
        assert credentials != null;
        mModel.set(FORMATTED_URL, formattedUrl);
        mModel.set(ORIGIN_SECURE, isOriginSecure);
        mModel.set(VISIBLE, true);

        ModelList credentialList = mModel.get(CREDENTIAL_LIST);
        credentialList.clear();
        for (Credential credential : credentials) {
            PropertyModel propertyModel = new PropertyModel.Builder(FAVICON, CREDENTIAL)
                                                  .with(FAVICON, null)
                                                  .with(CREDENTIAL, credential)
                                                  .build();
            credentialList.add(new MVCListAdapter.ListItem(DEFAULT_ITEM_TYPE, propertyModel));
            mDelegate.fetchFavicon(credential.getOriginUrl(), mDesiredFaviconSize,
                    (bitmap) -> propertyModel.set(FAVICON, bitmap));
        }
    }

    @Override
    public void onSelectItemAt(int position) {
        ModelList credentialList = mModel.get(CREDENTIAL_LIST);
        assert position >= 0 && position < credentialList.size();
        mModel.set(VISIBLE, false);
        mDelegate.onCredentialSelected(credentialList.get(position).model.get(CREDENTIAL));
    }

    @Override
    public void onDismissed() {
        if (!mModel.get(VISIBLE)) return; // Dismiss only if not dismissed yet.
        mModel.set(VISIBLE, false);
        mDelegate.onDismissed();
    }
}
