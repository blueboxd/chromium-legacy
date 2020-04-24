// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.content.Context;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.widget.ScrimView;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.components.module_installer.builder.ModuleInterface;
import org.chromium.content_public.browser.WebContents;

import java.util.Map;

/**
 * Interface between base module and assistant DFM.
 */
@ModuleInterface(module = "autofill_assistant",
        impl = "org.chromium.chrome.browser.autofill_assistant.AutofillAssistantModuleEntryImpl")
interface AutofillAssistantModuleEntry {
    /**
     * Starts Autofill Assistant on the current tab of the given chrome activity.
     *
     * <p>When started this way, Autofill Assistant appears immediately in the bottom sheet, expects
     * a single autostartable script for the tab's current URL, runs that script until the end and
     * disappears.
     */
    void start(BottomSheetController bottomSheetController,
            ChromeFullscreenManager fullscreenManager, CompositorViewHolder compositorViewHolder,
            ScrimView scrimView, Context context, @NonNull WebContents webContents,
            boolean skipOnboarding, boolean isChromeCustomTab, @NonNull String initialUrl,
            Map<String, String> parameters, String experimentIds, @Nullable String callerAccount,
            @Nullable String userName);
    /**
     * Returns a {@link AutofillAssistantActionHandler} instance tied to the activity owning the
     * given bottom sheet, and scrim view.
     *
     * @param context activity context
     * @param bottomSheetController bottom sheet controller instance of the activity
     * @param fullscreenManager fullscreen manager of the activity
     * @param compositorViewHolder compositor view holder of the activity
     * @param activityTabProvider activity tab provider
     * @param scrimView scrim view of the activity
     */
    AutofillAssistantActionHandler createActionHandler(Context context,
            BottomSheetController bottomSheetController, ChromeFullscreenManager fullscreenManager,
            CompositorViewHolder compositorViewHolder, ActivityTabProvider activityTabProvider,
            ScrimView scrimView);
}
