// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.content.Context;
import android.os.Bundle;
import android.support.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.widget.ScrimView;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;

import java.util.Collections;
import java.util.HashMap;
import java.util.Map;
import java.util.Set;

/**
 * A handler that provides Autofill Assistant actions for a specific activity.
 */
class AutofillAssistantActionHandlerImpl implements AutofillAssistantActionHandler {
    private final Context mContext;
    private final BottomSheetController mBottomSheetController;
    private final ScrimView mScrimView;
    private final GetCurrentTab mGetCurrentTab;

    AutofillAssistantActionHandlerImpl(Context context, BottomSheetController bottomSheetController,
            ScrimView scrimView, GetCurrentTab getCurrentTab) {
        mContext = context;
        mBottomSheetController = bottomSheetController;
        mScrimView = scrimView;
        mGetCurrentTab = getCurrentTab;
    }

    @Override
    public void listActions(String userName, String experimentIds, Bundle arguments,
            Callback<Set<String>> callback) {
        if (!AutofillAssistantPreferencesUtil.isAutofillOnboardingAccepted()) {
            callback.onResult(Collections.emptySet());
            return;
        }
        AutofillAssistantClient client = getOrCreateClient();
        if (client == null) {
            callback.onResult(Collections.emptySet());
            return;
        }

        client.listDirectActions(userName, experimentIds, toArgumentMap(arguments), callback);
    }

    @Override
    public void performOnboarding(String experimentIds, Callback<Boolean> callback) {
        AssistantOnboardingCoordinator coordinator = new AssistantOnboardingCoordinator(
                experimentIds, mContext, mBottomSheetController, mGetCurrentTab.get());
        coordinator.show(accepted -> {
            coordinator.hide();
            callback.onResult(accepted);
        });
    }

    @Override
    public void performAction(
            String name, String experimentIds, Bundle arguments, Callback<Boolean> callback) {
        AutofillAssistantClient client = getOrCreateClient();
        if (client == null) {
            callback.onResult(false);
            return;
        }

        Callback<AssistantOnboardingCoordinator> afterOnboarding = (onboardingCoordinator) -> {
            Map<String, String> argumentMap = toArgumentMap(arguments);
            if (name.isEmpty()) {
                callback.onResult(client.start(/* initialUrl= */ "", argumentMap, experimentIds,
                        Bundle.EMPTY, onboardingCoordinator));
                return;
            }
            callback.onResult(client.performDirectAction(
                    name, experimentIds, argumentMap, onboardingCoordinator));
        };

        if (!AutofillAssistantPreferencesUtil.isAutofillOnboardingAccepted()) {
            AssistantOnboardingCoordinator coordinator = new AssistantOnboardingCoordinator(
                    experimentIds, mContext, mBottomSheetController, mGetCurrentTab.get());
            coordinator.show(accepted -> {
                if (!accepted) {
                    coordinator.hide();
                    callback.onResult(false);
                    return;
                }
                afterOnboarding.onResult(coordinator);
            });
            return;
        }
        afterOnboarding.onResult(null);
    }

    /**
     * Returns a client for the current tab or {@code null} if there's no current tab or the current
     * tab doesn't have an associated browser content.
     */
    @Nullable
    private AutofillAssistantClient getOrCreateClient() {
        Tab tab = mGetCurrentTab.get();

        if (tab == null || tab.getWebContents() == null) return null;

        return AutofillAssistantClient.fromWebContents(tab.getWebContents());
    }

    /** Extracts string arguments from a bundle. */
    private Map<String, String> toArgumentMap(Bundle bundle) {
        Map<String, String> map = new HashMap<>();
        for (String key : bundle.keySet()) {
            String value = bundle.getString(key);
            if (value != null) {
                map.put(key, value);
            }
        }
        return map;
    }
}
