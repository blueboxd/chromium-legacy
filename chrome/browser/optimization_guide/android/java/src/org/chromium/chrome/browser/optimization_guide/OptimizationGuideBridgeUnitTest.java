// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.optimization_guide;

import static org.mockito.AdditionalMatchers.aryEq;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.multidex.ShadowMultiDex;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.JniMocker;
import org.chromium.components.optimization_guide.OptimizationGuideDecision;
import org.chromium.components.optimization_guide.proto.HintsProto.OptimizationType;
import org.chromium.components.optimization_guide.proto.ModelsProto.OptimizationTarget;
import org.chromium.content_public.browser.NavigationHandle;

import java.util.Arrays;

/**
 * Unit tests for OptimizationGuideBridge.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowMultiDex.class})
public class OptimizationGuideBridgeUnitTest {
    private static final String TEST_URL = "https://testurl.com/";
    @Rule
    public JniMocker mocker = new JniMocker();

    @Mock
    OptimizationGuideBridge.Natives mOptimizationGuideBridgeJniMock;

    @Mock
    OptimizationGuideBridge.OptimizationGuideCallback mCallbackMock;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mocker.mock(OptimizationGuideBridgeJni.TEST_HOOKS, mOptimizationGuideBridgeJniMock);
    }

    @Test
    @Feature({"OptimizationHints"})
    public void testRegisterOptimizationTypesAndTargets() {
        OptimizationGuideBridge bridge = new OptimizationGuideBridge(1);
        bridge.registerOptimizationTypesAndTargets(
                Arrays.asList(new OptimizationType[] {
                        OptimizationType.PERFORMANCE_HINTS, OptimizationType.DEFER_ALL_SCRIPT}),
                Arrays.asList(new OptimizationTarget[] {
                        OptimizationTarget.OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD}));
        verify(mOptimizationGuideBridgeJniMock, times(1))
                .registerOptimizationTypesAndTargets(
                        eq(1L), aryEq(new int[] {6, 5}), aryEq(new int[] {1}));
    }

    @Test
    @Feature({"OptimizationHints"})
    public void testRegisterOptimizationTypesAndTargets_withoutNativeBridge() {
        OptimizationGuideBridge bridge = new OptimizationGuideBridge(0);
        bridge.registerOptimizationTypesAndTargets(
                Arrays.asList(new OptimizationType[] {
                        OptimizationType.PERFORMANCE_HINTS, OptimizationType.DEFER_ALL_SCRIPT}),
                Arrays.asList(new OptimizationTarget[] {
                        OptimizationTarget.OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD}));
        verify(mOptimizationGuideBridgeJniMock, never())
                .registerOptimizationTypesAndTargets(anyLong(), any(int[].class), any(int[].class));
    }

    @Test
    @Feature({"OptimizationHints"})
    public void testRegisterOptimizationTypesAndTargets_noOptimizationTypes() {
        OptimizationGuideBridge bridge = new OptimizationGuideBridge(1);
        bridge.registerOptimizationTypesAndTargets(null,
                Arrays.asList(new OptimizationTarget[] {
                        OptimizationTarget.OPTIMIZATION_TARGET_PAINFUL_PAGE_LOAD}));
        verify(mOptimizationGuideBridgeJniMock, times(1))
                .registerOptimizationTypesAndTargets(
                        eq(1L), aryEq(new int[0]), aryEq(new int[] {1}));
    }

    @Test
    @Feature({"OptimizationHints"})
    public void testRegisterOptimizationTypesAndTargets_noOptimizationTargets() {
        OptimizationGuideBridge bridge = new OptimizationGuideBridge(1);
        bridge.registerOptimizationTypesAndTargets(
                Arrays.asList(new OptimizationType[] {
                        OptimizationType.PERFORMANCE_HINTS, OptimizationType.DEFER_ALL_SCRIPT}),
                null);
        verify(mOptimizationGuideBridgeJniMock, times(1))
                .registerOptimizationTypesAndTargets(
                        eq(1L), aryEq(new int[] {6, 5}), aryEq(new int[0]));
    }

    @Test
    @Feature({"OptimizationHints"})
    public void testCanApplyOptimization_withoutNativeBridge() {
        OptimizationGuideBridge bridge = new OptimizationGuideBridge(0);
        NavigationHandle navHandle = new NavigationHandle(0, TEST_URL, true, false, false);

        bridge.canApplyOptimization(navHandle, OptimizationType.PERFORMANCE_HINTS, mCallbackMock);

        verify(mOptimizationGuideBridgeJniMock, never())
                .canApplyOptimization(anyLong(), anyString(), anyInt(),
                        any(OptimizationGuideBridge.OptimizationGuideCallback.class));
        verify(mCallbackMock)
                .onOptimizationGuideDecision(eq(OptimizationGuideDecision.FALSE), isNull());
    }

    @Test
    @Feature({"OptimizationHints"})
    public void testCanApplyOptimization() {
        OptimizationGuideBridge bridge = new OptimizationGuideBridge(1);
        NavigationHandle navHandle = new NavigationHandle(0, TEST_URL, true, false, false);

        bridge.canApplyOptimization(navHandle, OptimizationType.PERFORMANCE_HINTS, mCallbackMock);

        verify(mOptimizationGuideBridgeJniMock, times(1))
                .canApplyOptimization(eq(1L), eq(TEST_URL), eq(6), eq(mCallbackMock));
    }
}
