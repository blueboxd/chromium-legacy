// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;

/** Autofill Assistant related preferences util class. */
class AutofillAssistantPreferencesUtil {
    // Avoid instatiation by accident.
    private AutofillAssistantPreferencesUtil() {}

    /** Checks whether the Autofill Assistant switch preference in settings is on. */
    static boolean isAutofillAssistantSwitchOn() {
        return SharedPreferencesManager.getInstance().readBoolean(
                ChromePreferenceKeys.AUTOFILL_ASSISTANT_ENABLED, true);
    }

    /** Checks whether the Autofill Assistant onboarding has been accepted. */
    static boolean isAutofillOnboardingAccepted() {
        return SharedPreferencesManager.getInstance().readBoolean(
                       ChromePreferenceKeys.AUTOFILL_ASSISTANT_ONBOARDING_ACCEPTED, false)
                ||
                /* Legacy treatment: users of earlier versions should not have to see the onboarding
                again if they checked the `do not show again' checkbox*/
                SharedPreferencesManager.getInstance().readBoolean(
                        ChromePreferenceKeys.AUTOFILL_ASSISTANT_SKIP_INIT_SCREEN, false);
    }

    /** Checks whether the Autofill Assistant onboarding screen should be shown. */
    static boolean getShowOnboarding() {
        return !isAutofillAssistantSwitchOn() || !isAutofillOnboardingAccepted();
    }

    /**
     * Sets preferences from the initial screen.
     *
     * @param accept Flag indicating whether the ToS have been accepted.
     */
    static void setInitialPreferences(boolean accept) {
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.AUTOFILL_ASSISTANT_ENABLED, accept);
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.AUTOFILL_ASSISTANT_ONBOARDING_ACCEPTED, accept);
    }
}
