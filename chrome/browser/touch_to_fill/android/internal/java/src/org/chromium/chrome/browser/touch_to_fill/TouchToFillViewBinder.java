// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill;

import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CREDENTIAL_LIST;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.FORMATTED_URL;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.ORIGIN_SECURE;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.VIEW_EVENT_LISTENER;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.VISIBLE;
import static org.chromium.chrome.browser.util.UrlUtilities.stripScheme;

import android.content.Context;
import android.text.method.PasswordTransformationMethod;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.chrome.browser.touch_to_fill.data.Credential;
import org.chromium.ui.modelutil.ModelListAdapter;
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
    static View createCredentialView(Context context) {
        return LayoutInflater.from(context).inflate(
                R.layout.touch_to_fill_credential_item, null, false);
    }

    /**
     * Called whenever a credential is bound to this view holder. Please note that this method
     * might be called on the same list entry repeatedly, so make sure to always set a default for
     * unused fields.
     * @param model The model containing the data for the view
     * @param view The view to be bound
     * @param propertyKey The key of the property to be bound
     */
    static void bindCredentialView(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == CredentialProperties.FAVICON) {
            ImageView imageView = view.findViewById(R.id.favicon);
            imageView.setImageBitmap(model.get(CredentialProperties.FAVICON));
        } else if (propertyKey == CredentialProperties.CREDENTIAL) {
            Credential credential = model.get(CredentialProperties.CREDENTIAL);

            TextView pslOriginText = view.findViewById(R.id.credential_origin);
            String formattedOrigin = stripScheme(credential.getOriginUrl());
            formattedOrigin =
                    formattedOrigin.replaceFirst("/$", ""); // Strip possibly trailing slash.
            pslOriginText.setText(formattedOrigin);
            pslOriginText.setVisibility(
                    credential.isPublicSuffixMatch() ? View.VISIBLE : View.GONE);

            TextView usernameText = view.findViewById(R.id.username);
            usernameText.setText(credential.getFormattedUsername());

            TextView passwordText = view.findViewById(R.id.password);
            passwordText.setText(credential.getPassword());
            passwordText.setTransformationMethod(new PasswordTransformationMethod());
        } else {
            assert false : "Every possible property update needs to be handled!";
        }
    }

    /**
     * Called whenever a property in the given model changes. It updates the given view accordingly.
     * @param model The observed {@link PropertyModel}. Its data need to be reflected in the view.
     * @param view The {@link TouchToFillView} to update.
     * @param propertyKey The {@link PropertyKey} which changed.
     */
    static void bindTouchToFillView(
            PropertyModel model, TouchToFillView view, PropertyKey propertyKey) {
        if (propertyKey == VIEW_EVENT_LISTENER) {
            view.setEventListener(model.get(VIEW_EVENT_LISTENER));
        } else if (propertyKey == VISIBLE) {
            view.setVisible(model.get(VISIBLE));
        } else if (propertyKey == FORMATTED_URL || propertyKey == ORIGIN_SECURE) {
            if (model.get(ORIGIN_SECURE)) {
                view.setSecureSubtitle(model.get(FORMATTED_URL));
            } else {
                view.setNonSecureSubtitle(model.get(FORMATTED_URL));
            }
        } else if (propertyKey == CREDENTIAL_LIST) {
            ModelListAdapter adapter = new ModelListAdapter(model.get(CREDENTIAL_LIST));
            adapter.registerType(CredentialProperties.DEFAULT_ITEM_TYPE,
                    () -> TouchToFillViewBinder.createCredentialView(view.getContext()),
                    TouchToFillViewBinder::bindCredentialView);
            view.setCredentialListAdapter(adapter);
        } else {
            assert false : "Every possible property update needs to be handled!";
        }
    }

    private TouchToFillViewBinder() {}
}
