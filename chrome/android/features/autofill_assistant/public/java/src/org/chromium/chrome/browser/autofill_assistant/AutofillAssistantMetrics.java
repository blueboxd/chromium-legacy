// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.autofill_assistant.metrics.DropOutReason;
import org.chromium.chrome.browser.autofill_assistant.metrics.FeatureModuleInstallation;
import org.chromium.chrome.browser.autofill_assistant.metrics.OnBoarding;

/**
 * Records user actions and histograms related to Autofill Assistant.
 *
 * All enums are auto generated from
 * components/autofill_assistant/browser/metrics.h.
 */
/* package */ class AutofillAssistantMetrics {
    /**
     * Records the reason for a drop out.
     */
    /* package */ static void recordDropOut(@DropOutReason int reason) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.AutofillAssistant.DropOutReason", reason, DropOutReason.MAX_VALUE + 1);
    }

    /**
     * Records the onboarding related action.
     */
    /* package */ static void recordOnBoarding(@OnBoarding int metric) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.AutofillAssistant.OnBoarding", metric, OnBoarding.MAX_VALUE + 1);
    }

    /**
     * Records the feature module installation action.
     */
    /* package */ static void recordFeatureModuleInstallation(
            @FeatureModuleInstallation int metric) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.AutofillAssistant.FeatureModuleInstallation", metric,
                FeatureModuleInstallation.MAX_VALUE + 1);
    }
}
