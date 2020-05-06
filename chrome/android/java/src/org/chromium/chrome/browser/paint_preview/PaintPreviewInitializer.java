// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.paint_preview;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.paint_preview.services.PaintPreviewTabServiceFactory;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

/**
 * Handles initialization of the Paint Preview tab observers.
 */
public class PaintPreviewInitializer {
    /**
     * Observes a {@link TabModelSelector} to monitor for initialization completion.
     * @param activity The ChromeActivity that corresponds to the tabModelSelector.
     * @param tabModelSelector The TabModelSelector to observe.
     */
    public static void observeTabModelSelector(
            ChromeActivity activity, TabModelSelector tabModelSelector) {
        // TODO(crbug/1074428): verify this doesn't cause a memory leak if the user exits Chrome
        // prior to onTabStateInitialized being called.
        tabModelSelector.addObserver(new EmptyTabModelSelectorObserver() {
            @Override
            public void onTabStateInitialized() {
                if (ChromeFeatureList.isEnabled(ChromeFeatureList.PAINT_PREVIEW_SHOW_ON_STARTUP)) {
                    // Avoid running the audit in multi-window mode as otherwise we will delete
                    // data that is possibly in use by the other Activity's TabModelSelector.
                    PaintPreviewTabServiceFactory.getServiceInstance().onRestoreCompleted(
                            tabModelSelector, /*runAudit=*/
                            !MultiWindowUtils.getInstance().areMultipleChromeInstancesRunning(
                                    activity));
                }
                tabModelSelector.removeObserver(this);
            }
        });
    }
}
