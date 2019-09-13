// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/**
 * The callback used to indicate what action the user took in the picker.
 */
public interface ContactsPickerListener {
    /**
     * A container class for exhcanging contact details.
     */
    public class Contact {
        public final List<String> names;
        public final List<String> emails;
        public final List<String> tel;

        public Contact(
                List<String> contactNames, List<String> contactEmails, List<String> contactTel) {
            names = contactNames;
            emails = contactEmails;
            tel = contactTel;
        }
    }

    /**
     * The action the user took in the picker.
     */
    @IntDef({ContactsPickerAction.CANCEL, ContactsPickerAction.CONTACTS_SELECTED,
            ContactsPickerAction.SELECT_ALL, ContactsPickerAction.UNDO_SELECT_ALL})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ContactsPickerAction {
        int CANCEL = 0;
        int CONTACTS_SELECTED = 1;
        int SELECT_ALL = 2;
        int UNDO_SELECT_ALL = 3;
        int NUM_ENTRIES = 4;
    }

    /**
     * Called when the user has selected an action. For possible actions see above.
     *
     * @param contacts The list of contacts selected.
     * @param percentageShared How big a percentage of the full contact list was shared (for metrics
     *         purposes).
     * @param propertiesRequested The properties requested by the website (names, emails,
     *         telephones).
     */
    void onContactsPickerUserAction(@ContactsPickerAction int action, List<Contact> contacts,
            int percentageShared, int propertiesRequested);
}
