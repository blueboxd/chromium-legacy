// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.optimization_guide;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.optimization_guide.OptimizationGuideDecision;
import org.chromium.components.optimization_guide.proto.HintsProto.OptimizationType;
import org.chromium.components.optimization_guide.proto.ModelsProto.OptimizationTarget;
import org.chromium.components.optimization_guide.proto.PerformanceHintsMetadataProto.PerformanceHintsMetadata;
import org.chromium.content_public.browser.NavigationHandle;

import java.util.ArrayList;
import java.util.List;

/**
 * Provides access to the optimization guide using the C++ OptimizationGuideKeyedService.
 *
 * An instance of this class must be created, used, and destroyed on the UI thread.
 */
@JNINamespace("optimization_guide::android")
public class OptimizationGuideBridge {
    private long mNativeOptimizationGuideBridge;

    /**
     * Interface to implement to receive decisions from the optimization guide.
     */
    public interface OptimizationGuideCallback {
        void onOptimizationGuideDecision(
                @OptimizationGuideDecision int decision, @Nullable OptimizationMetadata metadata);
    }

    /**
     * Initializes the C++ side of this class, using the Optimization Guide Decider for the last
     * used Profile.
     */
    public OptimizationGuideBridge() {
        ThreadUtils.assertOnUiThread();

        mNativeOptimizationGuideBridge = OptimizationGuideBridgeJni.get().init();
    }

    @VisibleForTesting
    protected OptimizationGuideBridge(long nativeOptimizationGuideBridge) {
        mNativeOptimizationGuideBridge = nativeOptimizationGuideBridge;
    }

    /**
     * Deletes the C++ side of this class. This must be called when this object is no longer needed.
     */
    public void destroy() {
        ThreadUtils.assertOnUiThread();

        if (mNativeOptimizationGuideBridge != 0) {
            OptimizationGuideBridgeJni.get().destroy(mNativeOptimizationGuideBridge);
            mNativeOptimizationGuideBridge = 0;
        }
    }

    /**
     * Registers the optimization types and targets that intend to be queried
     * during the session. It is expected for this to be called after the
     * browser has been initialized.
     */
    public void registerOptimizationTypesAndTargets(
            @Nullable List<OptimizationType> optimizationTypes,
            @Nullable List<OptimizationTarget> optimizationTargets) {
        ThreadUtils.assertOnUiThread();
        assert optimizationTypes != null || optimizationTargets != null;

        if (mNativeOptimizationGuideBridge == 0) return;

        if (optimizationTypes == null) {
            optimizationTypes = new ArrayList<>();
        }
        int[] intOptimizationTypes = new int[optimizationTypes.size()];
        for (int i = 0; i < optimizationTypes.size(); i++) {
            intOptimizationTypes[i] = optimizationTypes.get(i).getNumber();
        }

        if (optimizationTargets == null) {
            optimizationTargets = new ArrayList<>();
        }
        int[] intOptimizationTargets = new int[optimizationTargets.size()];
        for (int i = 0; i < optimizationTargets.size(); i++) {
            intOptimizationTargets[i] = optimizationTargets.get(i).getNumber();
        }
        OptimizationGuideBridgeJni.get().registerOptimizationTypesAndTargets(
                mNativeOptimizationGuideBridge, intOptimizationTypes, intOptimizationTargets);
    }

    /**
     * Invokes {@link callback} with the decision for the URL associated with {@link
     * navigationHandle} and {@link optimizationType} when sufficient information has been
     * collected to make a decision. This should only be called for main frame navigations.
     */
    public void canApplyOptimization(NavigationHandle navigationHandle,
            OptimizationType optimizationType, OptimizationGuideCallback callback) {
        ThreadUtils.assertOnUiThread();
        assert navigationHandle.isInMainFrame();

        if (mNativeOptimizationGuideBridge == 0) {
            callback.onOptimizationGuideDecision(OptimizationGuideDecision.FALSE, null);
            return;
        }

        OptimizationGuideBridgeJni.get().canApplyOptimization(mNativeOptimizationGuideBridge,
                navigationHandle.getUrl(), optimizationType.getNumber(), callback);
    }

    @CalledByNative
    private static void onOptimizationGuideDecision(OptimizationGuideCallback callback,
            @OptimizationGuideDecision int optimizationGuideDecision, Object optimizationMetadata) {
        callback.onOptimizationGuideDecision(
                optimizationGuideDecision, (OptimizationMetadata) optimizationMetadata);
    }

    @CalledByNative
    private static OptimizationMetadata createOptimizationMetadataWithPerformanceHintsMetadata(
            byte[] serializedPerformanceHintsMetadata) {
        OptimizationMetadata optimizationMetadata = new OptimizationMetadata();
        try {
            optimizationMetadata.setPerformanceHintsMetadata(
                    PerformanceHintsMetadata.parseFrom(serializedPerformanceHintsMetadata));
        } catch (com.google.protobuf.InvalidProtocolBufferException e) {
            return null;
        }
        return optimizationMetadata;
    }

    @NativeMethods
    interface Natives {
        long init();
        void destroy(long nativeOptimizationGuideBridge);
        void registerOptimizationTypesAndTargets(long nativeOptimizationGuideBridge,
                int[] optimizationTypes, int[] optimizationTargets);
        void canApplyOptimization(long nativeOptimizationGuideBridge, String url,
                int optimizationType, OptimizationGuideCallback callback);
    }
}
