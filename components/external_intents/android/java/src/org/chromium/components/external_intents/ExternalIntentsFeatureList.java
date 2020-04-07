// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.external_intents;

import org.chromium.base.FeatureList;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.MainDex;
import org.chromium.base.annotations.NativeMethods;

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
        assert FeatureList.isNativeInitialized();
        return ExternalIntentsFeatureListJni.get().isEnabled(featureName);
    }

    /** Alphabetical: */
    public static final String INTENT_BLOCK_EXTERNAL_FORM_REDIRECT_NO_GESTURE =
            "IntentBlockExternalFormRedirectsNoGesture";

    @NativeMethods
    interface Natives {
        boolean isEnabled(String featureName);
    }
}
