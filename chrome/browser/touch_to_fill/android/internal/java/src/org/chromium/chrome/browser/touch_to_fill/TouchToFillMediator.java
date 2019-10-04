// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill;

import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CREDENTIAL_LIST;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.FORMATTED_URL;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.VISIBLE;

import org.chromium.chrome.browser.touch_to_fill.data.Credential;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * Contains the logic for the TouchToFill component. It sets the state of the model and reacts to
 * events like clicks.
 */
class TouchToFillMediator implements TouchToFillProperties.ViewEventListener {
    private TouchToFillComponent.Delegate mDelegate;
    private PropertyModel mModel;

    void initialize(TouchToFillComponent.Delegate delegate, PropertyModel model) {
        assert delegate != null;
        mDelegate = delegate;
        mModel = model;
    }

    void showCredentials(String formattedUrl, List<Credential> credentials) {
        assert credentials != null;
        mModel.set(FORMATTED_URL, formattedUrl);
        mModel.set(VISIBLE, true);
        mModel.get(CREDENTIAL_LIST).clear();
        mModel.get(CREDENTIAL_LIST).addAll(credentials);
    }

    @Override
    public void onSelectItemAt(int position) {
        assert position >= 0 && position < mModel.get(CREDENTIAL_LIST).size();
        mModel.set(VISIBLE, false);
        mDelegate.onCredentialSelected(mModel.get(CREDENTIAL_LIST).get(position));
    }

    @Override
    public void onDismissed() {
        if (!mModel.get(VISIBLE)) return; // Dismiss only if not dismissed yet.
        mModel.set(VISIBLE, false);
        mDelegate.onDismissed();
    }
}
