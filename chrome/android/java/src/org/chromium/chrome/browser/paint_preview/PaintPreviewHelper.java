// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.paint_preview;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.paint_preview.services.PaintPreviewTabServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

/**
 * Handles initialization of the Paint Preview tab observers.
 */
public class PaintPreviewHelper {
    /**
     * Observes a {@link TabModelSelector} to monitor for initialization completion.
     * @param activity The ChromeActivity that corresponds to the tabModelSelector.
     * @param tabModelSelector The TabModelSelector to observe.
     */
    public static void observeTabModelSelector(
            ChromeActivity activity, TabModelSelector tabModelSelector) {
        if (!CachedFeatureFlags.isEnabled(ChromeFeatureList.PAINT_PREVIEW_SHOW_ON_STARTUP)) return;

        // TODO(crbug/1074428): verify this doesn't cause a memory leak if the user exits Chrome
        // prior to onTabStateInitialized being called.
        tabModelSelector.addObserver(new EmptyTabModelSelectorObserver() {
            @Override
            public void onTabStateInitialized() {
                // Avoid running the audit in multi-window mode as otherwise we will delete
                // data that is possibly in use by the other Activity's TabModelSelector.
                PaintPreviewTabServiceFactory.getServiceInstance().onRestoreCompleted(
                        tabModelSelector, /*runAudit=*/
                        !MultiWindowUtils.getInstance().areMultipleChromeInstancesRunning(
                                activity));
                tabModelSelector.removeObserver(this);
            }
        });
    }

    /**
     * Attemps to display the Paint Preview representation of for the given Tab.
     * @param onShown The callback for when the Paint Preview is shown.
     * @param onDismissed The callback for when the Paint Preview is dismissed.
     * @return Whether the Paint Preview started to initialize or is already initializating.
     * Note that if the Paint Preview is already showing, this will return false.
     */
    public static boolean showPaintPreviewOnRestore(
            Tab tab, Runnable onShown, Runnable onDismissed) {
        if (!CachedFeatureFlags.isEnabled(ChromeFeatureList.PAINT_PREVIEW_SHOW_ON_STARTUP)) {
            return false;
        }

        return TabbedPaintPreviewPlayer.get(tab).maybeShow(onShown, onDismissed);
    }
}
