// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.flags;

import org.chromium.chrome.browser.preferences.SharedPreferencesManager;

/**
 * A boolean-type {@link CachedFieldTrialParameter}.
 */
public class BooleanCachedFieldTrialParameter extends CachedFieldTrialParameter {
    private boolean mDefaultValue;

    public BooleanCachedFieldTrialParameter(
            String featureName, String variationName, boolean defaultValue) {
        this(featureName, variationName, defaultValue, null);
    }

    public BooleanCachedFieldTrialParameter(String featureName, String variationName,
            boolean defaultValue, String preferenceKeyOverride) {
        super(featureName, variationName, FieldTrialParameterType.BOOLEAN, preferenceKeyOverride);
        mDefaultValue = defaultValue;
    }

    /**
     * @return the value of the field trial parameter that should be used in this run.
     */
    public boolean getValue() {
        return CachedFeatureFlags.getConsistentBooleanValue(
                getSharedPreferenceKey(), getDefaultValue());
    }

    public boolean getDefaultValue() {
        return mDefaultValue;
    }

    @Override
    void cacheToDisk() {
        boolean value = ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                getFeatureName(), getParameterName(), getDefaultValue());
        SharedPreferencesManager.getInstance().writeBoolean(getSharedPreferenceKey(), value);
    }
}
