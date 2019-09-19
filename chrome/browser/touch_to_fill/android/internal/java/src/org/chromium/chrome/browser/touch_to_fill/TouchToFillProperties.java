// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill;

import org.chromium.chrome.browser.touch_to_fill.data.Credential;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Properties defined here reflect the visible state of the TouchToFill-components.
 */
class TouchToFillProperties {
    static final PropertyModel.WritableBooleanPropertyKey VISIBLE =
            new PropertyModel.WritableBooleanPropertyKey("visible");
    static final PropertyModel.WritableObjectPropertyKey<String> FORMATTED_URL =
            new PropertyModel.WritableObjectPropertyKey<>("formatted_url");
    static final PropertyModel.ReadableObjectPropertyKey<ListModel<Credential>> CREDENTIAL_LIST =
            new PropertyModel.ReadableObjectPropertyKey<>("credential_list");
    static final PropertyModel.ReadableObjectPropertyKey<ViewEventListener> VIEW_EVENT_LISTENER =
            new PropertyModel.ReadableObjectPropertyKey<>("view_event_listener");

    static PropertyModel createDefaultModel(ViewEventListener listener) {
        return new PropertyModel
                .Builder(VISIBLE, FORMATTED_URL, CREDENTIAL_LIST, VIEW_EVENT_LISTENER)
                .with(VISIBLE, false)
                .with(CREDENTIAL_LIST, new ListModel<>())
                .with(VIEW_EVENT_LISTENER, listener)
                .build();
    }

    /**
     * This interface is used by the view to communicate events back to the mediator. It abstracts
     * from the view by stripping information like parents, id or context.
     */
    interface ViewEventListener {
        /**
         * Called if the user selected an item from the list.
         * @param position The position that the user selected.
         */
        void onSelectItemAt(int position);

        /** Called if the user dismissed the view. */
        void onDismissed();
    }

    private TouchToFillProperties() {}
}
