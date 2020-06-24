// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy.settings;

import android.os.Bundle;

import androidx.preference.PreferenceFragmentCompat;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.net.SecureDnsManagementMode;
import org.chromium.chrome.browser.privacy.settings.SecureDnsProviderPreference.State;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.net.SecureDnsMode;

/**
 * Fragment to manage Secure DNS preference.  It consists of a toggle switch and,
 * if the switch is enabled, a SecureDnsControl.
 */
public class SecureDnsSettings extends PreferenceFragmentCompat {
    // Must match keys in secure_dns_settings.xml.
    private static final String PREF_SECURE_DNS_SWITCH = "secure_dns_switch";
    private static final String PREF_SECURE_DNS_PROVIDER = "secure_dns_provider";

    private PrivacyPreferencesManager mManager;
    private ChromeSwitchPreference mSecureDnsSwitch;
    private SecureDnsProviderPreference mSecureDnsProviderPreference;

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        getActivity().setTitle(R.string.settings_secure_dns_title);
        SettingsUtils.addPreferencesFromResource(this, R.xml.secure_dns_settings);

        mManager = PrivacyPreferencesManager.getInstance();

        // Set up preferences inside the activity.
        mSecureDnsSwitch = (ChromeSwitchPreference) findPreference(PREF_SECURE_DNS_SWITCH);
        mSecureDnsSwitch.setManagedPreferenceDelegate(
                (ChromeManagedPreferenceDelegate) preference -> mManager.isSecureDnsModeManaged());
        mSecureDnsSwitch.setOnPreferenceChangeListener((preference, enabled) -> {
            storePreferenceState((boolean) enabled, mSecureDnsProviderPreference.getState());
            loadPreferenceState();
            return true;
        });

        if (!mManager.isSecureDnsModeManaged()) {
            // If the mode isn't managed directly, we still need to disable the controls
            // if we detect a managed system configuration, or any parental control software.
            // However, we don't want to show the managed setting icon in this case, because the
            // setting is not directly controlled by a policy.
            @SecureDnsManagementMode
            int managementMode = mManager.getSecureDnsManagementMode();
            if (managementMode != SecureDnsManagementMode.NO_OVERRIDE) {
                mSecureDnsSwitch.setEnabled(false);
                boolean parentalControls =
                        managementMode == SecureDnsManagementMode.DISABLED_PARENTAL_CONTROLS;
                mSecureDnsSwitch.setSummaryOff(parentalControls
                                ? R.string.settings_secure_dns_disabled_for_parental_control
                                : R.string.settings_secure_dns_disabled_for_managed_environment);
            }
        }

        mSecureDnsProviderPreference =
                (SecureDnsProviderPreference) findPreference(PREF_SECURE_DNS_PROVIDER);
        mSecureDnsProviderPreference.setOnPreferenceChangeListener((preference, value) -> {
            State controlState = (State) value;
            boolean valid = storePreferenceState(mSecureDnsSwitch.isChecked(), controlState);
            if (valid != controlState.valid) {
                mSecureDnsProviderPreference.setState(controlState.withValid(valid));
                // Cancel the change to controlState.
                return false;
            }
            return true;
        });

        // Update preference views and state.
        loadPreferenceState();
    }

    /**
     * @param enabled Whether the toggle switch is enabled
     * @param controlState The state from SecureDnsControl.
     * @return True if the state was successfully stored.
     */
    private boolean storePreferenceState(boolean enabled, State controlState) {
        if (!enabled) {
            mManager.setSecureDnsMode(SecureDnsMode.OFF);
            mManager.setDnsOverHttpsTemplates("");
        } else if (!controlState.secure) {
            mManager.setSecureDnsMode(SecureDnsMode.AUTOMATIC);
            mManager.setDnsOverHttpsTemplates("");
        } else {
            if (controlState.template.isEmpty()
                    || !mManager.setDnsOverHttpsTemplates(controlState.template)) {
                return false;
            }
            mManager.setSecureDnsMode(SecureDnsMode.SECURE);
        }
        return true;
    }

    private void loadPreferenceState() {
        @SecureDnsMode
        int mode = mManager.getSecureDnsMode();
        boolean enabled = mode != SecureDnsMode.OFF;
        boolean enforced = mManager.isSecureDnsModeManaged()
                || mManager.getSecureDnsManagementMode() != SecureDnsManagementMode.NO_OVERRIDE;
        mSecureDnsSwitch.setChecked(enabled);
        mSecureDnsProviderPreference.setEnabled(enabled && !enforced);

        boolean secure = mode == SecureDnsMode.SECURE;
        String template = mManager.getDnsOverHttpsTemplates();
        boolean valid = true; // States loaded from storage are presumed valid.
        mSecureDnsProviderPreference.setState(new State(secure, template, valid));
    }

    @Override
    public void onResume() {
        super.onResume();
        loadPreferenceState();
    }
}
