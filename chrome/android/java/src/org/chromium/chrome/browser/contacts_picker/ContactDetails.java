// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contacts_picker;

import android.content.res.Resources;

import androidx.annotation.Nullable;

import org.chromium.chrome.R;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * A class to keep track of the metadata associated with a contact.
 */
public class ContactDetails implements Comparable<ContactDetails> {
    /**
     * A container class for delivering contact details in abbreviated form
     * (where only the first email and phone numbers are returned and the rest
     * is indicated with "+n more" strings).
     */
    public static class AbbreviatedContactDetails {
        public String primaryEmail;
        public String overflowEmailCount;
        public String primaryTelephoneNumber;
        public String overflowTelephoneNumberCount;
    }

    // The unique id for the contact.
    private final String mId;

    // The display name for this contact.
    private final String mDisplayName;

    // The list of emails registered for this contact.
    private final List<String> mEmails;

    // The list of phone numbers registered for this contact.
    private final List<String> mPhoneNumbers;

    /**
     * The ContactDetails constructor.
     * @param id The unique identifier of this contact.
     * @param displayName The display name of this contact.
     * @param emails The emails registered for this contact.
     * @param phoneNumbers The phone numbers registered for this contact.
     */
    public ContactDetails(
            String id, String displayName, List<String> emails, List<String> phoneNumbers) {
        mDisplayName = displayName;
        mEmails = emails != null ? new ArrayList<String>(emails) : new ArrayList<String>();
        mPhoneNumbers = phoneNumbers != null ? new ArrayList<String>(phoneNumbers)
                                             : new ArrayList<String>();
        mId = id;
    }

    public List<String> getDisplayNames() {
        return Arrays.asList(mDisplayName);
    }

    public List<String> getEmails() {
        return mEmails;
    }

    public List<String> getPhoneNumbers() {
        return mPhoneNumbers;
    }

    public String getDisplayName() {
        return mDisplayName;
    }

    public String getId() {
        return mId;
    }

    /**
     * Accessor for the abbreviated display name (first letter of first name and first letter of
     * last name).
     * @return The display name, abbreviated to two characters.
     */
    public String getDisplayNameAbbreviation() {
        // Display the two letter abbreviation of the display name.
        String displayChars = "";
        if (mDisplayName.length() > 0) {
            displayChars += mDisplayName.charAt(0);
            String[] parts = mDisplayName.split(" ");
            if (parts.length > 1) {
                displayChars += parts[parts.length - 1].charAt(0);
            }
        }

        return displayChars;
    }

    /**
     * Accessor for the list of contact details (emails and phone numbers). Returned as strings
     * separated by newline).
     * @param includeEmails Whether to include emails in the returned results.
     * @param includeTels Whether to include telephones in the returned results.
     * @return A string containing all the contact details registered for this contact.
     */
    public String getContactDetailsAsString(boolean includeEmails, boolean includeTels) {
        int count = 0;
        StringBuilder builder = new StringBuilder();
        if (includeEmails) {
            for (String email : mEmails) {
                if (count++ > 0) {
                    builder.append("\n");
                }
                builder.append(email);
            }
        }
        if (includeTels) {
            for (String phoneNumber : mPhoneNumbers) {
                if (count++ > 0) {
                    builder.append("\n");
                }
                builder.append(phoneNumber);
            }
        }

        return builder.toString();
    }

    /**
     * Accessor for the list of contact details (emails and phone numbers).
     * @param includeEmails Whether to include emails in the returned results.
     * @param includeTels Whether to include telephones in the returned results.
     * @param resources The resources to use for fetching the string. Must be provided.
     * @return The contact details registered for this contact.
     */
    public AbbreviatedContactDetails getAbbreviatedContactDetails(
            boolean includeEmails, boolean includeTels, @Nullable Resources resources) {
        AbbreviatedContactDetails results = new AbbreviatedContactDetails();

        results.overflowEmailCount = "";
        if (!includeEmails || mEmails.size() == 0) {
            results.primaryEmail = "";
        } else {
            results.primaryEmail = mEmails.get(0);
            int totalAddresses = mEmails.size();
            if (totalAddresses > 1) {
                int hiddenAddresses = totalAddresses - 1;
                results.overflowEmailCount = resources.getQuantityString(
                        R.plurals.contacts_picker_more_details, hiddenAddresses, hiddenAddresses);
            }
        }

        results.overflowTelephoneNumberCount = "";
        if (!includeTels || mPhoneNumbers.size() == 0) {
            results.primaryTelephoneNumber = "";
        } else {
            results.primaryTelephoneNumber = mPhoneNumbers.get(0);
            int totalNumbers = mPhoneNumbers.size();
            if (totalNumbers > 1) {
                int hiddenNumbers = totalNumbers - 1;
                results.overflowTelephoneNumberCount = resources.getQuantityString(
                        R.plurals.contacts_picker_more_details, hiddenNumbers, hiddenNumbers);
            }
        }

        return results;
    }

    /**
     * A comparison function (results in a full name ascending sorting).
     * @param other The other ContactDetails object to compare it with.
     * @return 0, 1, or -1, depending on which is bigger.
     */
    @Override
    public int compareTo(ContactDetails other) {
        return other.mDisplayName.compareTo(mDisplayName);
    }

    @Override
    public int hashCode() {
        Object[] values = {mId, mDisplayName};
        return Arrays.hashCode(values);
    }

    @Override
    public boolean equals(@Nullable Object object) {
        if (object == null) return false;
        if (object == this) return true;
        if (!(object instanceof ContactDetails)) return false;

        ContactDetails otherInfo = (ContactDetails) object;
        return mId.equals(otherInfo.mId);
    }
}
