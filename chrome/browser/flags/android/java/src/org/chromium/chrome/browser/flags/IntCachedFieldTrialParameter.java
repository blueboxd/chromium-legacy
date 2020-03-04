// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.flags;

import org.chromium.chrome.browser.preferences.SharedPreferencesManager;

/**
 * An int-type {@link CachedFieldTrialParameter}.
 */
public class IntCachedFieldTrialParameter extends CachedFieldTrialParameter {
    private int mDefaultValue;

    public IntCachedFieldTrialParameter(
            String featureName, String variationName, int defaultValue) {
        super(featureName, variationName, FieldTrialParameterType.INT, null);
        mDefaultValue = defaultValue;
    }

    public int getDefaultValue() {
        return mDefaultValue;
    }

    @Override
    void cacheToDisk() {
        int value = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                getFeatureName(), getParameterName(), getDefaultValue());
        SharedPreferencesManager.getInstance().writeInt(getSharedPreferenceKey(), value);
    }
}
