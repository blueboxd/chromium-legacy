// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.content.Context;
import android.content.Intent;
import android.support.test.InstrumentationRegistry;

import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.SiteSettingsActivity;
import org.chromium.weblayer.WebLayer;

/**
 * ActivityTestRule for SiteSettingsActivity.
 *
 * Test can use this ActivityTestRule to launch SiteSettingsActivity.
 */
public class SiteSettingsActivityTestRule extends WebLayerActivityTestRule<SiteSettingsActivity> {
    public SiteSettingsActivityTestRule() {
        super(SiteSettingsActivity.class);
    }

    public SiteSettingsActivity launchCategoryListWithProfile(String profileName) {
        Context appContext = InstrumentationRegistry.getInstrumentation()
                                     .getTargetContext()
                                     .getApplicationContext();
        Intent siteSettingsIntent = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            // We load WebLayer here so it gets initialized with the test/instrumentation Context,
            // which has an in-memory SharedPreferences. Without this call, WebLayer gets
            // initialized with the Application as its appContext, which breaks tests because a
            // SharedPreferences xml file is persisted to disk.
            WebLayer.loadSync(appContext);
            return SiteSettingsActivity.createIntentForCategoryList(appContext, profileName);
        });
        return launchActivity(siteSettingsIntent);
    }
}
