// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.dynamicmodule;

import android.content.Context;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.MetricsUtils.HistogramDelta;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/**
 * Test for recording memory footprint for a loaded module
 */

@RunWith(ChromeJUnit4ClassRunner.class)
public class ModuleMetricsTest {
    private static final String PROPORTIONAL_SET_HISTOGRAM_NAME =
            "CustomTabs.DynamicModule.ProportionalSet.OnModuleLoad";

    private static final String RESIDENT_SET_HISTOGRAM_NAME =
            "CustomTabs.DynamicModule.ResidentSet.OnModuleLoad";

    @Before
    public void setUp() {
        LibraryLoader.getInstance().ensureInitialized(LibraryProcessType.PROCESS_BROWSER);
    }

    @Test
    @SmallTest
    public void testModuleMetrics() {
        int samplesRecordedForProportionalSet =
                RecordHistogram.getHistogramTotalCountForTesting(PROPORTIONAL_SET_HISTOGRAM_NAME);

        int samplesRecordedForResidentSet =
                RecordHistogram.getHistogramTotalCountForTesting(RESIDENT_SET_HISTOGRAM_NAME);

        HistogramDelta histogramProportionalSet =
                new HistogramDelta(PROPORTIONAL_SET_HISTOGRAM_NAME, 0);

        HistogramDelta histogramResidentSet = new HistogramDelta(RESIDENT_SET_HISTOGRAM_NAME, 0);

        Context context = InstrumentationRegistry.getInstrumentation()
                                  .getTargetContext()
                                  .getApplicationContext();

        ModuleMetrics.recordCodeMemoryFootprint(context.getPackageName(), "OnModuleLoad");

        Assert.assertEquals(samplesRecordedForProportionalSet + 1,
                RecordHistogram.getHistogramTotalCountForTesting(PROPORTIONAL_SET_HISTOGRAM_NAME));

        Assert.assertEquals(0, histogramProportionalSet.getDelta());

        Assert.assertEquals(samplesRecordedForResidentSet + 1,
                RecordHistogram.getHistogramTotalCountForTesting(RESIDENT_SET_HISTOGRAM_NAME));

        Assert.assertEquals(0, histogramResidentSet.getDelta());
    }
}
