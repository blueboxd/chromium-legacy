// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_check;

import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.CompromisedCredentialProperties.COMPROMISED_CREDENTIAL;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.HeaderProperties.CHECK_STATUS;
import static org.chromium.chrome.browser.password_check.PasswordCheckProperties.ITEMS;
import static org.chromium.components.embedder_support.util.UrlUtilities.stripScheme;

import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.chromium.chrome.browser.password_check.PasswordCheckProperties.ItemType;
import org.chromium.chrome.browser.password_check.internal.R;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.RecyclerViewAdapter;
import org.chromium.ui.modelutil.SimpleRecyclerViewMcp;

/**
 * Provides functions that map {@link PasswordCheckProperties} changes in a {@link PropertyModel} to
 * the suitable method in {@link PasswordCheckFragmentView}.
 */
class PasswordCheckViewBinder {
    /**
     * Called whenever a property in the given model changes. It updates the given view accordingly.
     * @param model The observed {@link PropertyModel}. Its data need to be reflected in the view.
     * @param view The {@link PasswordCheckFragmentView} to update.
     * @param propertyKey The {@link PropertyKey} which changed.
     */
    static void bindPasswordCheckView(
            PropertyModel model, PasswordCheckFragmentView view, PropertyKey propertyKey) {
        if (propertyKey == ITEMS) {
            view.getListView().setAdapter(new RecyclerViewAdapter<>(
                    new SimpleRecyclerViewMcp<>(model.get(ITEMS),
                            PasswordCheckProperties::getItemType,
                            PasswordCheckViewBinder::connectPropertyModel),
                    PasswordCheckViewBinder::createViewHolder));
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    /**
     * Factory used to create a new View inside the list inside the PasswordCheckFragmentView.
     * @param parent The parent {@link ViewGroup} of the new item.
     * @param itemType The type of View to create.
     */
    private static PasswordCheckViewHolder createViewHolder(
            ViewGroup parent, @ItemType int itemType) {
        switch (itemType) {
            case ItemType.HEADER:
                return new PasswordCheckViewHolder(parent, R.layout.password_check_header_item,
                        PasswordCheckViewBinder::bindHeaderView);
            case ItemType.COMPROMISED_CREDENTIAL:
                return new PasswordCheckViewHolder(parent,
                        R.layout.password_check_compromised_credential_item,
                        PasswordCheckViewBinder::bindCredentialView);
        }
        assert false : "Cannot create view for ItemType: " + itemType;
        return null;
    }

    /**
     * This method creates a model change processor for each recycler view item when it is created.
     * @param holder A {@link PasswordCheckViewHolder} holding the view and view binder for the MCP.
     * @param item A {@link MVCListAdapter.ListItem} holding the {@link PropertyModel} for the MCP.
     */
    private static void connectPropertyModel(
            PasswordCheckViewHolder holder, MVCListAdapter.ListItem item) {
        holder.setupModelChangeProcessor(item.model);
    }

    /**
     * Called whenever a credential is bound to this view holder. Please note that this method
     * might be called on a recycled view with old data, so make sure to always reset unused
     * properties to default values.
     * @param model The model containing the data for the view
     * @param view The view to be bound
     * @param propertyKey The key of the property to be bound
     */
    private static void bindCredentialView(
            PropertyModel model, View view, PropertyKey propertyKey) {
        CompromisedCredential credential = model.get(COMPROMISED_CREDENTIAL);
        if (propertyKey == COMPROMISED_CREDENTIAL) {
            TextView pslOriginText = view.findViewById(R.id.credential_origin);
            String formattedOrigin = stripScheme(credential.getOriginUrl());
            formattedOrigin =
                    formattedOrigin.replaceFirst("/$", ""); // Strip possibly trailing slash.
            pslOriginText.setText(formattedOrigin);
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    /**
     * Called whenever a property in the given model changes. It updates the given view accordingly.
     * @param model The observed {@link PropertyModel}. Its data need to be reflected in the view.
     * @param view The {@link View} of the header to update.
     * @param key The {@link PropertyKey} which changed.
     */
    private static void bindHeaderView(PropertyModel model, View view, PropertyKey key) {
        if (key == CHECK_STATUS) {
            // TODO(crbug.com/1101256): Set text and illustration based on status.
        } else {
            assert false : "Unhandled update to property:" + key;
        }
    }

    private PasswordCheckViewBinder() {}
}
