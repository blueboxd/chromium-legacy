// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.external_intents;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.MainDex;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.library_loader.LibraryLoader;

/**
 * Java accessor for base/feature_list.h state.
 *
 * This class provides methods to access values of feature flags registered in
 * |kFeaturesExposedToJava| in components/external_intents/android/external_intents_feature_list.cc.
 *
 */
@JNINamespace("external_intents")
@MainDex
public abstract class ExternalIntentsFeatureList {
    /** Prevent instantiation. */
    private ExternalIntentsFeatureList() {}

    /**
     * @return Whether the native FeatureList is initialized or not.
     */
    private static boolean isNativeInitialized() {
        if (!LibraryLoader.getInstance().isInitialized()) return false;
        // Even if the native library is loaded, the C++ FeatureList might not be initialized yet.
        // In that case, accessing it will not immediately fail, but instead cause a crash later
        // when it is initialized. Return whether the native FeatureList has been initialized,
        // so the return value can be tested, or asserted for a more actionable stack trace
        // on failure.
        //
        // The FeatureList is however guaranteed to be initialized by the time
        // AsyncInitializationActivity#finishNativeInitialization is called.
        return ExternalIntentsFeatureListJni.get().isInitialized();
    }

    /**
     * Returns whether the specified feature is enabled or not.
     *
     * Note: Features queried through this API must be added to the array
     * |kFeaturesExposedToJava| in
     * components/external_intents/android/external_intents_feature_list.cc.
     *
     * Calling this has the side effect of bucketing this client, which may cause an experiment to
     * be marked as active.
     *
     * Should be called only after native is loaded.
     *
     * @param featureName The name of the feature to query.
     * @return Whether the feature is enabled or not.
     */
    public static boolean isEnabled(String featureName) {
        assert isNativeInitialized();
        return ExternalIntentsFeatureListJni.get().isEnabled(featureName);
    }

    /** Alphabetical: */
    public static final String INTENT_BLOCK_EXTERNAL_FORM_REDIRECT_NO_GESTURE =
            "IntentBlockExternalFormRedirectsNoGesture";

    @NativeMethods
    interface Natives {
        boolean isInitialized();
        boolean isEnabled(String featureName);
    }
}
