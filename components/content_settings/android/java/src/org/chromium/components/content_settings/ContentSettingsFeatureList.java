// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_settings;

import org.chromium.base.FeatureList;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.MainDex;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.library_loader.LibraryLoader;

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
     * @return Whether the native FeatureList has been initialized. If this method returns false,
     *         none of the methods in this class that require native access should be called.
     */
    public static boolean isInitialized() {
        if (FeatureList.hasTestFeatures()) return true;
        return isNativeInitialized();
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
        assert isNativeInitialized();
        return ContentSettingsFeatureListJni.get().isEnabled(featureName);
    }

    /**
     * @return Whether the native FeatureList is initialized or not.
     */
    private static boolean isNativeInitialized() {
        if (FeatureList.hasTestFeatures()) return true;

        if (!LibraryLoader.getInstance().isInitialized()) return false;
        // Even if the native library is loaded, the C++ FeatureList might not be initialized yet.
        // In that case, accessing it will not immediately fail, but instead cause a crash later
        // when it is initialized. Return whether the native FeatureList has been initialized,
        // so the return value can be tested, or asserted for a more actionable stack trace
        // on failure.
        //
        // The FeatureList is however guaranteed to be initialized by the time
        // AsyncInitializationActivity#finishNativeInitialization is called.
        return ContentSettingsFeatureListJni.get().isInitialized();
    }

    @NativeMethods
    interface Natives {
        boolean isInitialized();
        boolean isEnabled(String featureName);
    }
}
