// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill;

import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CredentialProperties.CREDENTIAL;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CredentialProperties.FAVICON;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CredentialProperties.FORMATTED_ORIGIN;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CredentialProperties.ON_CLICK_LISTENER;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.FORMATTED_URL;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.ORIGIN_SECURE;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.SHEET_ITEMS;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.VISIBLE;

import androidx.annotation.Px;

import org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CredentialProperties;
import org.chromium.chrome.browser.touch_to_fill.data.Credential;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
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

        ListModel<ListItem> sheetItems = mModel.get(SHEET_ITEMS);
        sheetItems.clear();
        for (Credential credential : credentials) {
            PropertyModel propertyModel =
                    new PropertyModel.Builder(CredentialProperties.ALL_KEYS)
                            .with(CREDENTIAL, credential)
                            .with(FORMATTED_ORIGIN,
                                    UrlFormatter.formatUrlForDisplayOmitScheme(
                                            credential.getOriginUrl()))
                            .with(ON_CLICK_LISTENER, this::onSelectedCredential)
                            .build();
            sheetItems.add(new ListItem(TouchToFillProperties.ItemType.CREDENTIAL, propertyModel));
            mDelegate.fetchFavicon(credential.getOriginUrl(), mDesiredFaviconSize,
                    (bitmap) -> propertyModel.set(FAVICON, bitmap));
        }
    }

    private void onSelectedCredential(Credential credential) {
        mModel.set(VISIBLE, false);
        mDelegate.onCredentialSelected(credential);
    }

    @Override
    public void onDismissed() {
        if (!mModel.get(VISIBLE)) return; // Dismiss only if not dismissed yet.
        mModel.set(VISIBLE, false);
        mDelegate.onDismissed();
    }
}
