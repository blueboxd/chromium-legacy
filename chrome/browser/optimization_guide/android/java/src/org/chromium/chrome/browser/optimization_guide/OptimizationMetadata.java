// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.optimization_guide;

import androidx.annotation.Nullable;

import org.chromium.components.optimization_guide.proto.PerformanceHintsMetadataProto.PerformanceHintsMetadata;

/**
 * Struct to contain optimization metadata.
 */
public class OptimizationMetadata {
    private PerformanceHintsMetadata mPerformanceHintsMetadata;

    @Nullable
    public PerformanceHintsMetadata getPerformanceHintsMetadata() {
        return this.mPerformanceHintsMetadata;
    }
    public void setPerformanceHintsMetadata(PerformanceHintsMetadata performanceHintsMetadata) {
        this.mPerformanceHintsMetadata = performanceHintsMetadata;
    }
}
