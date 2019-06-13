// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import org.chromium.base.BundleUtils;
import org.chromium.base.Callback;
import org.chromium.base.SysUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.modules.ModuleInstallUi;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.module_installer.ModuleInstaller;

/**
 * Manages the loading of autofill assistant DFM, and provides implementation of
 * AutofillAssistantModuleEntry.
 */
public class AutofillAssistantModuleEntryProvider {
    /**
     * Returns AutofillAssistantModuleEntry by using it as argument to the
     * passed in callback, or null if DFM loading fails.
     */
    /* package */ static void getModuleEntry(
            ChromeActivity activity, Callback<AutofillAssistantModuleEntry> callback) {
        getTab(activity, tab -> {
            // Required to access resources in DFM using this activity as context.
            ModuleInstaller.initActivity(activity);
            if (AutofillAssistantModule.isInstalled()) {
                callback.onResult(createEntry(tab));
                return;
            }
            loadDynamicModuleWithUi(activity, tab, callback);
        });
    }

    /**
     * Maybe trigger a deferred install of the module.
     *
     * <p>This is public so that it can be used in the Chrome upgrade package. The conditions for
     * eligibility are:
     *
     * <ul>
     *   <li>This is a Bundle build.
     *   <li>The autofill_assistant DFM is not installed yet.
     *   <li>The Android version is L+.
     *   <li>The device has high disk capacity.
     * </ul>
     */
    public static void maybeInstallDeferred() {
        if (!BundleUtils.isBundle()) return;
        if (AutofillAssistantModule.isInstalled()) return;
        if (android.os.Build.VERSION.SDK_INT < android.os.Build.VERSION_CODES.LOLLIPOP) return;
        if (!SysUtils.isHighEndDiskDevice()) return;
        AutofillAssistantModule.installDeferred();
    }

    private static AutofillAssistantModuleEntry createEntry(Tab tab) {
        AutofillAssistantModuleEntryFactory factory = AutofillAssistantModule.getImpl();
        return factory.createEntry(tab.getWebContents());
    }

    private static void loadDynamicModuleWithUi(
            ChromeActivity activity, Tab tab, Callback<AutofillAssistantModuleEntry> callback) {
        ModuleInstallUi ui = new ModuleInstallUi(tab, R.string.autofill_assistant_module_title,
                new ModuleInstallUi.FailureUiListener() {
                    @Override
                    public void onRetry() {
                        loadDynamicModuleWithUi(activity, tab, callback);
                    }

                    @Override
                    public void onCancel() {
                        callback.onResult(null);
                    }
                });
        // Shows toast informing user about install start.
        ui.showInstallStartUi();
        ModuleInstaller.install("autofill_assistant", (success) -> {
            if (success) {
                // Clean install of chrome will have issues here without initializing
                // after installation of DFM.
                ModuleInstaller.initActivity(activity);
                // Don't show success UI from DFM, transition to autobot UI directly.
                callback.onResult(createEntry(tab));
                return;
            }
            // Show inforbar to ask user if they want to retry or cancel.
            ui.showInstallFailureUi();
        });
    }

    private static void getTab(ChromeActivity activity, Callback<Tab> callback) {
        if (activity.getActivityTab() != null
                && activity.getActivityTab().getWebContents() != null) {
            callback.onResult(activity.getActivityTab());
            return;
        }

        // The tab is not yet available. We need to register as listener and wait for it.
        activity.getActivityTabProvider().addObserverAndTrigger(
                new ActivityTabProvider.HintlessActivityTabObserver() {
                    @Override
                    public void onActivityTabChanged(Tab tab) {
                        if (tab == null) return;
                        activity.getActivityTabProvider().removeObserver(this);
                        assert tab.getWebContents() != null;
                        callback.onResult(tab);
                    }
                });
    }
}
