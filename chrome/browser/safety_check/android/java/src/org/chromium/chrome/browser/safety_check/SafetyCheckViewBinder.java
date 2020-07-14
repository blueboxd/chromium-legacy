// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_check;

import android.view.View;

import org.chromium.chrome.browser.safety_check.SafetyCheckProperties.PasswordsState;
import org.chromium.chrome.browser.safety_check.SafetyCheckProperties.SafeBrowsingState;
import org.chromium.chrome.browser.safety_check.SafetyCheckProperties.UpdatesState;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

class SafetyCheckViewBinder {
    private static final String PASSWORDS_KEY = "passwords";
    private static final String SAFE_BROWSING_KEY = "safe_browsing";
    private static final String UPDATES_KEY = "updates";

    private static int getStringForPasswords(@PasswordsState int state) {
        switch (state) {
            case PasswordsState.UNCHECKED:
                return R.string.safety_check_unchecked;
            case PasswordsState.CHECKING:
                return R.string.safety_check_checking;
            case PasswordsState.NO_PASSWORDS:
                return R.string.safety_check_passwords_no_passwords;
            case PasswordsState.SIGNED_OUT:
            case PasswordsState.QUOTA_LIMIT:
            case PasswordsState.OFFLINE:
            case PasswordsState.ERROR:
                return R.string.safety_check_error;
            case PasswordsState.SAFE:
            case PasswordsState.COMPROMISED_EXIST:
                // TODO(crbug.com/1070620): update the strings for all states once available.
                return 0;
            default:
                assert false : "Unknown PasswordsState value.";
        }
        // Not reached.
        return 0;
    }

    private static int getStringForSafeBrowsing(@SafeBrowsingState int state) {
        switch (state) {
            case SafeBrowsingState.UNCHECKED:
                return R.string.safety_check_unchecked;
            case SafeBrowsingState.CHECKING:
                return R.string.safety_check_checking;
            case SafeBrowsingState.ENABLED_STANDARD:
                return R.string.safety_check_safe_browsing_enabled_standard;
            case SafeBrowsingState.ENABLED_ENHANCED:
                return R.string.safety_check_safe_browsing_enabled_enhanced;
            case SafeBrowsingState.DISABLED:
                return R.string.safety_check_safe_browsing_disabled;
            case SafeBrowsingState.DISABLED_BY_ADMIN:
                return R.string.safety_check_safe_browsing_disabled_by_admin;
            case SafeBrowsingState.ERROR:
                return R.string.safety_check_error;
            default:
                assert false : "Unknown SafeBrowsingState value.";
        }
        // Not reached.
        return 0;
    }

    private static int getStringForUpdates(@UpdatesState int state) {
        switch (state) {
            case UpdatesState.UNCHECKED:
                return R.string.safety_check_unchecked;
            case UpdatesState.CHECKING:
                return R.string.safety_check_checking;
            case UpdatesState.UPDATED:
                return R.string.safety_check_updates_updated;
            case UpdatesState.OUTDATED:
                return R.string.safety_check_updates_outdated;
            case UpdatesState.OFFLINE:
                return R.string.safety_check_updates_offline;
            case UpdatesState.ERROR:
                return R.string.safety_check_updates_error;
            default:
                assert false : "Unknown UpdatesState value.";
        }
        // Not reached.
        return 0;
    }

    static void bind(
            PropertyModel model, SafetyCheckSettingsFragment view, PropertyKey propertyKey) {
        if (SafetyCheckProperties.PASSWORDS_STATE == propertyKey) {
            view.updateElementStatus(PASSWORDS_KEY,
                    getStringForPasswords(model.get(SafetyCheckProperties.PASSWORDS_STATE)));
        } else if (SafetyCheckProperties.SAFE_BROWSING_STATE == propertyKey) {
            view.updateElementStatus(SAFE_BROWSING_KEY,
                    getStringForSafeBrowsing(model.get(SafetyCheckProperties.SAFE_BROWSING_STATE)));
        } else if (SafetyCheckProperties.UPDATES_STATE == propertyKey) {
            view.updateElementStatus(UPDATES_KEY,
                    getStringForUpdates(model.get(SafetyCheckProperties.UPDATES_STATE)));
        } else if (SafetyCheckProperties.SAFETY_CHECK_BUTTON_CLICK_LISTENER == propertyKey) {
            view.getCheckButton().setOnClickListener((View.OnClickListener) model.get(
                    SafetyCheckProperties.SAFETY_CHECK_BUTTON_CLICK_LISTENER));
        } else {
            assert false : "Unhandled property detected in SafetyCheckViewBinder!";
        }
    }
}
