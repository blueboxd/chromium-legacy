// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.payment;

import android.content.Context;
import android.support.annotation.Nullable;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.payments.AutofillContact;
import org.chromium.chrome.browser.payments.ContactEditor;
import org.chromium.chrome.browser.payments.ui.ContactDetailsSection;

import java.util.ArrayList;
import java.util.List;

/**
 * The contact details section of the Autofill Assistant payment request.
 */
public class AssistantPaymentRequestContactDetailsSection
        extends AssistantPaymentRequestSection<AutofillContact> {
    private ContactEditor mEditor;
    private boolean mIgnoreProfileChangeNotifications;

    AssistantPaymentRequestContactDetailsSection(Context context, ViewGroup parent) {
        super(context, parent, R.layout.autofill_assistant_contact_summary,
                R.layout.autofill_assistant_contact_full,
                context.getResources().getDimensionPixelSize(
                        R.dimen.autofill_assistant_payment_request_title_padding),
                context.getString(R.string.payments_contact_details_label),
                context.getString(R.string.payments_add_contact),
                context.getString(R.string.payments_add_contact));
    }

    public void setEditor(ContactEditor editor) {
        mEditor = editor;
        if (mEditor == null) {
            return;
        }

        for (AutofillContact contact : getItems()) {
            addAutocompleteInformationToEditor(contact);
        }
    }

    @Override
    protected void createOrEditItem(@Nullable AutofillContact oldItem) {
        if (mEditor != null) {
            mEditor.edit(oldItem, newItem -> {
                assert (newItem != null && newItem.isComplete());
                mIgnoreProfileChangeNotifications = true;
                addOrUpdateItem(newItem, true);
                mIgnoreProfileChangeNotifications = false;
            }, cancel -> {});
        }
    }

    @Override
    protected void updateFullView(View fullView, AutofillContact contact) {
        if (contact == null) {
            return;
        }
        TextView fullViewText = fullView.findViewById(R.id.contact_full);
        String description = "";
        if (contact.getPayerName() != null) {
            description += contact.getPayerName();
        }
        if (contact.getPayerEmail() != null) {
            if (!description.isEmpty()) {
                description += "\n";
            }
            description += contact.getPayerEmail();
        }
        fullViewText.setText(description);
        hideIfEmpty(fullViewText);
        fullView.findViewById(R.id.incomplete_error)
                .setVisibility(contact.isComplete() ? View.GONE : View.VISIBLE);
    }

    @Override
    protected void updateSummaryView(View summaryView, AutofillContact contact) {
        if (contact == null) {
            return;
        }
        TextView contactSummaryView = summaryView.findViewById(R.id.contact_summary);

        String description = "";
        if (contact.getPayerEmail() != null) {
            description = contact.getPayerEmail();
        } else if (contact.getPayerName() != null) {
            description = contact.getPayerName();
        }
        contactSummaryView.setText(description);
        hideIfEmpty(contactSummaryView);

        TextView contactIncompleteView = summaryView.findViewById(R.id.incomplete_error);
        contactIncompleteView.setVisibility(contact.isComplete() ? View.GONE : View.VISIBLE);
    }

    /**
     * The Chrome profiles have changed externally. This will rebuild the UI with the new/changed
     * set of profiles, while keeping the selected item if possible.
     */
    void onProfilesChanged(List<AutofillProfile> profiles, boolean requestPayerEmail,
            boolean requestPayerName, boolean requestPayerPhone) {
        if (mIgnoreProfileChangeNotifications) {
            return;
        }

        if (!requestPayerEmail && !requestPayerName && !requestPayerPhone) {
            return;
        }

        // Note: we create a temporary editor (necessary for converting profiles to contacts)
        // instead of using mEditor, which may be null.
        ContactEditor tempEditor =
                new ContactEditor(requestPayerName, requestPayerPhone, requestPayerEmail, false);
        AutofillContact previouslySelectedContact = mSelectedOption;

        // Convert profiles into a list of |AutofillContact|.
        int selectedContactIndex = -1;
        ContactDetailsSection sectionInformation =
                new ContactDetailsSection(mContext, profiles, tempEditor, null);
        List<AutofillContact> contacts = new ArrayList<>();
        for (int i = 0; i < sectionInformation.getSize(); i++) {
            AutofillContact contact = (AutofillContact) sectionInformation.getItem(i);
            if (contact == null) {
                continue;
            }
            contacts.add(contact);
            if (previouslySelectedContact != null
                    && TextUtils.equals(
                            contact.getIdentifier(), previouslySelectedContact.getIdentifier())) {
                selectedContactIndex = i;
            }
        }

        // Replace current set of items, keep selection if possible.
        setItems(contacts, selectedContactIndex);
    }

    @Override
    protected void addOrUpdateItem(AutofillContact contact, boolean select) {
        super.addOrUpdateItem(contact, select);
        addAutocompleteInformationToEditor(contact);
    }

    private void addAutocompleteInformationToEditor(AutofillContact contact) {
        if (mEditor == null) {
            return;
        }
        mEditor.addEmailAddressIfValid(contact.getPayerEmail());
        mEditor.addPayerNameIfValid(contact.getPayerName());
        mEditor.addPhoneNumberIfValid(contact.getPayerPhone());
    }
}