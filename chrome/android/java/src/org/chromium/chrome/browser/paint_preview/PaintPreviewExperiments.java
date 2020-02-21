// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.paint_preview;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.paintpreview.browser.PaintPreviewUtils;
import org.chromium.content_public.browser.WebContents;

import java.util.Random;

/**
 * Contains experiments related to the Paint Preview feature.
 */
public class PaintPreviewExperiments {
    private static Random sRandom = new Random();

    /**
     * Param name for field trial param that determines capture experiment trigger probability
     * threshold.
     **/
    private static final String CAPTURE_EXPERIMENT_TRIGGER_PROBABILITY_THRESHOLD =
            "capture_trigger_probability_threshold";

    /**
     * Runs the Paint Preview capture experiment if the experiment is enabled and a probability
     * threshold is met. The probability threshold is determined by a field trial param.
     * @param contents The WebContents of the page to capture.
     */
    public static void runCaptureExperiment(WebContents contents) {
        // ChromeFeatureList and the Paint Preview component rely on native.
        if (!LibraryLoader.getInstance().isInitialized()) {
            return;
        }

        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.PAINT_PREVIEW_CAPTURE_EXPERIMENT)) {
            return;
        }

        double captureTriggerProbabilityThreshold =
                ChromeFeatureList.getFieldTrialParamByFeatureAsDouble(
                        ChromeFeatureList.PAINT_PREVIEW_CAPTURE_EXPERIMENT,
                        CAPTURE_EXPERIMENT_TRIGGER_PROBABILITY_THRESHOLD, 0);
        if (sRandom.nextDouble() >= captureTriggerProbabilityThreshold) {
            return;
        }

        PaintPreviewUtils.capturePaintPreview(contents);
    }
}
