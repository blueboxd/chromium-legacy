// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.content.Context;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.annotations.UsedByReflection;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.autofill_assistant.metrics.OnBoarding;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.widget.ScrimView;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.content_public.browser.WebContents;

import java.util.Map;

/**
 * Implementation of {@link AutofillAssistantModuleEntry}. This is the entry point into the
 * assistant DFM.
 */
@UsedByReflection("AutofillAssistantModuleEntryProvider.java")
public class AutofillAssistantModuleEntryImpl implements AutofillAssistantModuleEntry {
    @Override
    public void start(BottomSheetController bottomSheetController,
            ChromeFullscreenManager fullscreenManager, CompositorViewHolder compositorViewHolder,
            ScrimView scrimView, Context context, @NonNull WebContents webContents,
            boolean skipOnboarding, boolean isChromeCustomTab, @NonNull String initialUrl,
            Map<String, String> parameters, String experimentIds, @Nullable String callerAccount,
            @Nullable String userName) {
        if (skipOnboarding) {
            AutofillAssistantMetrics.recordOnBoarding(OnBoarding.OB_NOT_SHOWN);
            AutofillAssistantClient.fromWebContents(webContents)
                    .start(initialUrl, parameters, experimentIds, callerAccount, userName,
                            isChromeCustomTab,
                            /* onboardingCoordinator= */ null);
            return;
        }

        AssistantOnboardingCoordinator onboardingCoordinator =
                new AssistantOnboardingCoordinator(experimentIds, parameters, context,
                        bottomSheetController, fullscreenManager, compositorViewHolder, scrimView);
        onboardingCoordinator.show(accepted -> {
            if (!accepted) return;

            AutofillAssistantClient.fromWebContents(webContents)
                    .start(initialUrl, parameters, experimentIds, callerAccount, userName,
                            isChromeCustomTab, onboardingCoordinator);
        });
    }

    @Override
    public AutofillAssistantActionHandler createActionHandler(Context context,
            BottomSheetController bottomSheetController, ChromeFullscreenManager fullscreenManager,
            CompositorViewHolder compositorViewHolder, ActivityTabProvider activityTabProvider,
            ScrimView scrimView) {
        return new AutofillAssistantActionHandlerImpl(context, bottomSheetController,
                fullscreenManager, compositorViewHolder, activityTabProvider, scrimView);
    }
}
