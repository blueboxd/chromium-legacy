// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_settings;

import org.chromium.base.FeatureList;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.MainDex;
import org.chromium.base.annotations.NativeMethods;

/**
 * Provides an API for querying the status of Content Settings features.
 */
// TODO(crbug.com/1060097): Remove/update this once a generalized FeatureList exists.
@JNINamespace("content_settings")
@MainDex
public class ContentSettingsFeatureList {
    public static final String IMPROVED_COOKIE_CONTROLS = "ImprovedCookieControls";
    public static final String IMPROVED_COOKIE_CONTROLS_FOR_THIRD_PARTY_COOKIE_BLOCKING =
            "ImprovedCookieControlsForThirdPartyCookieBlocking";

    private ContentSettingsFeatureList() {}

    /**
     * @deprecated Use {@link FeatureList#isInitialized()} instead
     * @return Whether the native FeatureList has been initialized. If this method returns false,
     *         none of the methods in this class that require native access should be called.
     */
    // TODO(crbug.com/1060097): Migrate callers to the FeatureList equivalent function.
    @Deprecated
    public static boolean isInitialized() {
        return FeatureList.isInitialized();
    }

    /**
     * Returns whether the specified feature is enabled or not.
     *
     * Note: Features queried through this API must be added to the array
     * |kFeaturesExposedToJava| in
     * //components/content_settings/android/content_settings_feature_list.cc
     *
     * @param featureName The name of the feature to query.
     * @return Whether the feature is enabled or not.
     */
    public static boolean isEnabled(String featureName) {
        Boolean testValue = FeatureList.getTestValueForFeature(featureName);
        if (testValue != null) return testValue;
        assert FeatureList.isNativeInitialized();
        return ContentSettingsFeatureListJni.get().isEnabled(featureName);
    }

    @NativeMethods
    interface Natives {
        boolean isEnabled(String featureName);
    }
}
