// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_check;

import androidx.annotation.IntDef;

import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Properties defined here reflect the visible state of the PasswordCheck subcomponents.
 */
class PasswordCheckProperties {
    static final PropertyModel.ReadableObjectPropertyKey<ListModel<MVCListAdapter.ListItem>> ITEMS =
            new PropertyModel.ReadableObjectPropertyKey<>("items");

    static final PropertyKey[] ALL_KEYS = {ITEMS};

    static PropertyModel createDefaultModel() {
        return new PropertyModel.Builder(ALL_KEYS).with(ITEMS, new ListModel<>()).build();
    }

    /**
     * Properties for a compromised credential entry.
     */
    static class CompromisedCredentialProperties {
        static final PropertyModel
                .ReadableObjectPropertyKey<CompromisedCredential> COMPROMISED_CREDENTIAL =
                new PropertyModel.ReadableObjectPropertyKey<>("compromised_credential");

        static final PropertyKey[] ALL_KEYS = {COMPROMISED_CREDENTIAL};

        private CompromisedCredentialProperties() {}
    }

    /**
     * Properties defining the header (banner logo and status line).
     */
    static class HeaderProperties {
        static final PropertyModel.WritableIntPropertyKey CHECK_STATUS =
                new PropertyModel.WritableIntPropertyKey("check_status");

        static final PropertyKey[] ALL_KEYS = {CHECK_STATUS};

        private HeaderProperties() {}
    }

    @IntDef({ItemType.HEADER, ItemType.COMPROMISED_CREDENTIAL})
    @Retention(RetentionPolicy.SOURCE)
    @interface ItemType {
        /**
         * The header at the top of the password check settings screen.
         */
        int HEADER = 1;

        /**
         * A section containing a user's name and password.
         */
        int COMPROMISED_CREDENTIAL = 2;
    }

    @IntDef({CheckStatus.SUCCESS, CheckStatus.RUNNING})
    @Retention(RetentionPolicy.SOURCE)
    @interface CheckStatus {
        /** The check was completed without errors. */
        int SUCCESS = 1;
        /** The check is still running. */
        int RUNNING = 2;
        /** The check cannot run because the user is offline. */
        int ERROR_OFFLINE = 3;
        /** The check cannot run because the user has no passwords on this device. */
        int ERROR_NO_PASSWORDS = 4;
        /** The check is cannot run because the user is signed-out. */
        int ERROR_SIGNED_OUT = 5;
        /** The check is cannot run because the user has exceeded their quota. */
        int ERROR_QUOTA_LIMIT = 6;
        /** The check is cannot run for unknown reasons. */
        int ERROR_UNKNOWN = 6;
    }

    /**
     * Returns the sheet item type for a given item.
     * @param item An {@link MVCListAdapter.ListItem}.
     * @return The {@link ItemType} of the given list item.
     */
    static @ItemType int getItemType(MVCListAdapter.ListItem item) {
        return item.type;
    }

    private PasswordCheckProperties() {}
}
