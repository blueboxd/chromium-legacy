// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_check;

import org.chromium.chrome.browser.password_check.BulkLeakCheckServiceState;

/**
 * Represents and stores the internal state of a Safety check.
 */
public class SafetyCheckModel {
    /**
     * Stores the state related to the Safe Browsing element.
     */
    public enum SafeBrowsing implements SafetyCheckElement {
        UNCHECKED(R.string.safety_check_unchecked),
        CHECKING(R.string.safety_check_checking),
        ENABLED_STANDARD(R.string.safety_check_safe_browsing_enabled_standard),
        ENABLED_ENHANCED(R.string.safety_check_safe_browsing_enabled_enhanced),
        DISABLED(R.string.safety_check_safe_browsing_disabled),
        DISABLED_BY_ADMIN(R.string.safety_check_safe_browsing_disabled_by_admin),
        ERROR(R.string.safety_check_error);

        private final int mStatusString;

        private SafeBrowsing(int statusString) {
            mStatusString = statusString;
        }

        /**
         * @return The resource ID for the corresponding status string.
         */
        @Override
        public int getStatusString() {
            return mStatusString;
        }

        /**
         * android:key element in XML corresponding to Safe Browsing.
         */
        public static final String KEY = "safe_browsing";

        /**
         * Converts the C++ enum state to the internal representation.
         * @param status Safe Browsing Status from C++.
         * @return A SafeBrowsing enum element.
         */
        public static SafeBrowsing fromSafeBrowsingStatus(@SafeBrowsingStatus int status) {
            switch (status) {
                case SafeBrowsingStatus.CHECKING:
                    return CHECKING;
                case SafeBrowsingStatus.ENABLED:
                case SafeBrowsingStatus.ENABLED_STANDARD:
                    return ENABLED_STANDARD;
                case SafeBrowsingStatus.ENABLED_ENHANCED:
                    return ENABLED_ENHANCED;
                case SafeBrowsingStatus.DISABLED:
                    return DISABLED;
                case SafeBrowsingStatus.DISABLED_BY_ADMIN:
                    return DISABLED_BY_ADMIN;
                default:
                    return ERROR;
            }
        }
    }

    /**
     * Stores the state related to the Passwords element.
     */
    public enum Passwords implements SafetyCheckElement {
        UNCHECKED(R.string.safety_check_unchecked),
        CHECKING(R.string.safety_check_checking),
        SAFE,
        COMPROMISED_EXIST,
        OFFLINE,
        NO_PASSWORDS(R.string.safety_check_passwords_no_passwords),
        SIGNED_OUT,
        QUOTA_LIMIT,
        ERROR(R.string.safety_check_error);

        private final int mStatusString;

        private Passwords() {
            mStatusString = 0;
        }

        private Passwords(int statusString) {
            mStatusString = statusString;
        }

        /**
         * @return The resource ID for the corresponding status string.
         */
        @Override
        public int getStatusString() {
            return mStatusString;
        }

        /**
         * android:key element in XML corresponding to Safe Browsing.
         */
        public static final String KEY = "passwords";

        public static Passwords fromErrorState(@BulkLeakCheckServiceState int state) {
            switch (state) {
                case BulkLeakCheckServiceState.SIGNED_OUT:
                    return SIGNED_OUT;
                case BulkLeakCheckServiceState.QUOTA_LIMIT:
                    return QUOTA_LIMIT;
                default:
                    return ERROR;
            }
        }
    }

    private SafetyCheckSettingsFragment mView;
    private SafeBrowsing mSafeBrowsing;
    private Passwords mPasswords;

    /**
     * Creates a new model for Safety check.
     * @param view A Settings Fragment for Safety check.
     */
    public SafetyCheckModel(SafetyCheckSettingsFragment view) {
        mView = view;
    }

    /**
     * Updates the model and the view with the "Checking" state.
     */
    public void setCheckingState() {
        mSafeBrowsing = SafeBrowsing.CHECKING;
        mPasswords = Passwords.CHECKING;
        mView.updateElementStatus(SafeBrowsing.KEY, mSafeBrowsing.getStatusString());
        mView.updateElementStatus(Passwords.KEY, mPasswords.getStatusString());
    }

    /**
     * Updates the model and the view with the results of a Safe Browsing check.
     * @param status The status of Safe Browsing.
     */
    public void updateSafeBrowsingStatus(@SafeBrowsingStatus int status) {
        mSafeBrowsing = SafeBrowsing.fromSafeBrowsingStatus(status);
        mView.updateElementStatus(SafeBrowsing.KEY, mSafeBrowsing.getStatusString());
    }

    /**
     * Updates the model and the view with the results of a successful passwords
     * check.
     * @param hasPasswords Whether any passwords are saved.
     * @param numLeaked Number of leaked passwords.
     */
    public void updatePasswordsStatusOnSucess(boolean hasPasswords, int numLeaked) {
        if (!hasPasswords) {
            mPasswords = Passwords.NO_PASSWORDS;
        } else if (numLeaked == 0) {
            mPasswords = Passwords.SAFE;
        } else {
            mPasswords = Passwords.COMPROMISED_EXIST;
        }
        mView.updateElementStatus(Passwords.KEY, mPasswords.getStatusString());
    }

    /**
     * Updates the model and the view with the results of a failed passwords
     * check.
     * @param state State of the leak check service reflecting an error.
     */
    public void updatePasswordsStatusOnError(@BulkLeakCheckServiceState int state) {
        mPasswords = Passwords.fromErrorState(state);
        mView.updateElementStatus(Passwords.KEY, mPasswords.getStatusString());
    }
}
