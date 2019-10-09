// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill;

import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CREDENTIAL_LIST;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.FORMATTED_URL;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.VIEW_EVENT_LISTENER;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.VISIBLE;

import android.text.method.PasswordTransformationMethod;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.chromium.chrome.browser.touch_to_fill.data.Credential;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Provides functions that map {@link TouchToFillProperties} changes in a {@link PropertyModel} to
 * the suitable method in {@link TouchToFillView}.
 */
class TouchToFillViewBinder {
    /**
     * Factory used to create a new View inside the ListView inside the TouchToFillView.
     * @param parent The parent {@link ViewGroup} of the new item.
     */
    static View createCredentialView(ViewGroup parent) {
        return LayoutInflater.from(parent.getContext())
                .inflate(R.layout.touch_to_fill_credential_item, parent, false);
    }

    /**
     * Called whenever a credential is bound to this view holder. Please note that this method
     * might be called on the same list entry repeatedly, so make sure to always set a default for
     * unused fields.
     * @param view The view to be bound.
     * @param credential The {@link Credential} whose data needs to be displayed.
     */
    static void bindCredentialView(View view, Credential credential) {
        TextView usernameText = view.findViewById(R.id.username);
        usernameText.setText(credential.getFormattedUsername());

        TextView passwordText = view.findViewById(R.id.password);
        passwordText.setText(credential.getPassword());
        passwordText.setTransformationMethod(new PasswordTransformationMethod());
    }

    /**
     * Called whenever a property in the given model changes. It updates the given view accordingly.
     * @param model The observed {@link PropertyModel}. Its data need to be reflected in the view.
     * @param view The {@link TouchToFillView} to update.
     * @param propertyKey The {@link PropertyKey} which changed.
     */
    static void bind(PropertyModel model, TouchToFillView view, PropertyKey propertyKey) {
        if (propertyKey == VIEW_EVENT_LISTENER) {
            view.setEventListener(model.get(VIEW_EVENT_LISTENER));
        } else if (propertyKey == VISIBLE) {
            view.setVisible(model.get(VISIBLE));
        } else if (propertyKey == FORMATTED_URL) {
            view.setFormattedUrl(model.get(FORMATTED_URL));
        } else if (propertyKey == CREDENTIAL_LIST) {
            // No binding required. Single items are bound via bindCredentialView.
        } else {
            assert false : "Every possible property update needs to be handled!";
        }
    }

    private TouchToFillViewBinder() {}
}
