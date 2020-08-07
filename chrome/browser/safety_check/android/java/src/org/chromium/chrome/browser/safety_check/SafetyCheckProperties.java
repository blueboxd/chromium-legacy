// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_check;

import androidx.annotation.IntDef;

import org.chromium.chrome.browser.password_check.PasswordCheckUIStatus;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableLongPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

class SafetyCheckProperties {
    /** State of the passwords check, one of the {@link PasswordsState} values. */
    static final WritableIntPropertyKey PASSWORDS_STATE = new WritableIntPropertyKey();
    /** State of the Safe Browsing check, one of the {@link SafeBrowsingState} values. */
    static final WritableIntPropertyKey SAFE_BROWSING_STATE = new WritableIntPropertyKey();
    /** State of the updates check, one of the {@link UpdatesState} values. */
    static final WritableIntPropertyKey UPDATES_STATE = new WritableIntPropertyKey();
    /** Listener for the passwords element click events. */
    static final WritableObjectPropertyKey PASSWORDS_CLICK_LISTENER =
            new WritableObjectPropertyKey();
    /** Listener for the Safe Browsing element click events. */
    static final WritableObjectPropertyKey SAFE_BROWSING_CLICK_LISTENER =
            new WritableObjectPropertyKey();
    /** Listener for the updates element click events. */
    static final WritableObjectPropertyKey UPDATES_CLICK_LISTENER = new WritableObjectPropertyKey();
    /** Listener for Safety check button click events. */
    static final WritableObjectPropertyKey SAFETY_CHECK_BUTTON_CLICK_LISTENER =
            new WritableObjectPropertyKey();
    /** Timestamp of the last run, a Long object. */
    static final WritableLongPropertyKey LAST_RUN_TIMESTAMP = new WritableLongPropertyKey();

    @IntDef({PasswordsState.UNCHECKED, PasswordsState.CHECKING, PasswordsState.SAFE,
            PasswordsState.COMPROMISED_EXIST, PasswordsState.OFFLINE, PasswordsState.NO_PASSWORDS,
            PasswordsState.SIGNED_OUT, PasswordsState.QUOTA_LIMIT, PasswordsState.ERROR})
    @Retention(RetentionPolicy.SOURCE)
    public @interface PasswordsState {
        int UNCHECKED = 0;
        int CHECKING = 1;
        int SAFE = 2;
        int COMPROMISED_EXIST = 3;
        int OFFLINE = 4;
        int NO_PASSWORDS = 5;
        int SIGNED_OUT = 6;
        int QUOTA_LIMIT = 7;
        int ERROR = 8;
    }

    static @PasswordsState int passwordsStatefromErrorState(@PasswordCheckUIStatus int state) {
        switch (state) {
            case PasswordCheckUIStatus.ERROR_OFFLINE:
                return PasswordsState.OFFLINE;
            case PasswordCheckUIStatus.ERROR_NO_PASSWORDS:
                return PasswordsState.NO_PASSWORDS;
            case PasswordCheckUIStatus.ERROR_SIGNED_OUT:
                return PasswordsState.SIGNED_OUT;
            case PasswordCheckUIStatus.ERROR_QUOTA_LIMIT:
            case PasswordCheckUIStatus.ERROR_QUOTA_LIMIT_ACCOUNT_CHECK:
                return PasswordsState.QUOTA_LIMIT;
            case PasswordCheckUIStatus.CANCELED:
            case PasswordCheckUIStatus.ERROR_UNKNOWN:
                return PasswordsState.ERROR;
            default:
                assert false : "Unknown PasswordCheckUIStatus value.";
        }
        // Never reached.
        return 0;
    }

    @IntDef({SafeBrowsingState.UNCHECKED, SafeBrowsingState.CHECKING,
            SafeBrowsingState.ENABLED_STANDARD, SafeBrowsingState.ENABLED_ENHANCED,
            SafeBrowsingState.DISABLED, SafeBrowsingState.DISABLED_BY_ADMIN,
            SafeBrowsingState.ERROR})
    @Retention(RetentionPolicy.SOURCE)
    public @interface SafeBrowsingState {
        int UNCHECKED = 0;
        int CHECKING = 1;
        int ENABLED_STANDARD = 2;
        int ENABLED_ENHANCED = 3;
        int DISABLED = 4;
        int DISABLED_BY_ADMIN = 5;
        int ERROR = 6;
    }

    static @SafeBrowsingState int safeBrowsingStateFromNative(@SafeBrowsingStatus int status) {
        switch (status) {
            case SafeBrowsingStatus.CHECKING:
                return SafeBrowsingState.CHECKING;
            case SafeBrowsingStatus.ENABLED:
            case SafeBrowsingStatus.ENABLED_STANDARD:
                return SafeBrowsingState.ENABLED_STANDARD;
            case SafeBrowsingStatus.ENABLED_ENHANCED:
                return SafeBrowsingState.ENABLED_ENHANCED;
            case SafeBrowsingStatus.DISABLED:
                return SafeBrowsingState.DISABLED;
            case SafeBrowsingStatus.DISABLED_BY_ADMIN:
                return SafeBrowsingState.DISABLED_BY_ADMIN;
            case SafeBrowsingStatus.DISABLED_BY_EXTENSION:
                assert false : "Safe Browsing cannot be disabled by extension on Android.";
                return 0;
            default:
                assert false : "Unknown SafeBrowsingStatus value.";
        }
        // Never reached.
        return 0;
    }

    @IntDef({UpdatesState.UNCHECKED, UpdatesState.CHECKING, UpdatesState.UPDATED,
            UpdatesState.OUTDATED, UpdatesState.OFFLINE, UpdatesState.ERROR})
    @Retention(RetentionPolicy.SOURCE)
    public @interface UpdatesState {
        int UNCHECKED = 0;
        int CHECKING = 1;
        int UPDATED = 2;
        int OUTDATED = 3;
        int OFFLINE = 4;
        int ERROR = 5;
    }

    static final PropertyKey[] ALL_KEYS = new PropertyKey[] {PASSWORDS_STATE, SAFE_BROWSING_STATE,
            UPDATES_STATE, PASSWORDS_CLICK_LISTENER, SAFE_BROWSING_CLICK_LISTENER,
            UPDATES_CLICK_LISTENER, SAFETY_CHECK_BUTTON_CLICK_LISTENER, LAST_RUN_TIMESTAMP};

    static PropertyModel createSafetyCheckModel() {
        return new PropertyModel.Builder(ALL_KEYS)
                .with(PASSWORDS_STATE, PasswordsState.UNCHECKED)
                .with(SAFE_BROWSING_STATE, SafeBrowsingState.UNCHECKED)
                .with(UPDATES_STATE, UpdatesState.UNCHECKED)
                .with(LAST_RUN_TIMESTAMP, 0)
                .build();
    }
}
