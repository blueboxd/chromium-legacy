// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;

import androidx.annotation.Nullable;

import org.chromium.base.BuildInfo;
import org.chromium.base.Callback;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.autofill_assistant.metrics.DropOutReason;
import org.chromium.chrome.browser.directactions.DirectActionHandler;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.metrics.UmaSessionStats;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.chrome.browser.widget.ScrimView;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;

import java.net.URLDecoder;
import java.util.HashMap;
import java.util.Map;

/** Facade for starting Autofill Assistant on a custom tab. */
public class AutofillAssistantFacade {
    /**
     * Prefix for Intent extras relevant to this feature.
     *
     * <p>Intent starting with this prefix are reported to the controller as parameters, except for
     * the ones starting with {@code INTENT_SPECIAL_PREFIX}.
     */
    private static final String INTENT_EXTRA_PREFIX =
            "org.chromium.chrome.browser.autofill_assistant.";

    /** Prefix for intent extras which are not parameters. */
    private static final String INTENT_SPECIAL_PREFIX = INTENT_EXTRA_PREFIX + "special.";

    /** Special parameter that enables the feature. */
    private static final String PARAMETER_ENABLED = "ENABLED";

    /**
     * Identifier used by parameters/or special intent that indicates experiments passed from
     * the caller.
     */
    private static final String EXPERIMENT_IDS_IDENTIFIER = "EXPERIMENT_IDS";

    /** Intent extra name for csv list of experiment ids. */
    private static final String EXPERIMENT_IDS_NAME =
            INTENT_SPECIAL_PREFIX + EXPERIMENT_IDS_IDENTIFIER;

    /**
     * Synthetic field trial names and group names should match those specified in
     * google3/analysis/uma/dashboards/
     * .../variations/generate_server_hashes.py and
     * .../website/components/variations_dash/variations_histogram_entry.js.
     */
    private static final String TRIGGERED_SYNTHETIC_TRIAL = "AutofillAssistantTriggered";
    private static final String ENABLED_GROUP = "Enabled";

    private static final String EXPERIMENTS_SYNTHETIC_TRIAL = "AutofillAssistantExperimentsTrial";

    /** Returns true if conditions are satisfied to attempt to start Autofill Assistant. */
    private static boolean isConfigured(@Nullable Bundle intentExtras) {
        return getBooleanParameter(intentExtras, PARAMETER_ENABLED);
    }

    /**
     * Starts Autofill Assistant.
     * @param activity {@link ChromeActivity} the activity on which the Autofill Assistant is being
     *         started. This must be a launch activity holding the correct intent for starting.
     */
    public static void start(ChromeActivity activity) {
        start(activity, activity.getInitialIntent().getExtras(),
                activity.getInitialIntent().getDataString());
    }

    /**
     * Starts Autofill Assistant.
     * @param activity {@link ChromeActivity} the activity on which the Autofill Assistant is being
     *         started.
     * @param bundleExtras {@link Bundle} the extras which were used to start the Autofill
     *         Assistant.
     * @param initialUrl the initial URL the Autofill Assistant should be started on.
     */
    public static void start(ChromeActivity activity, Bundle bundleExtras, String initialUrl) {
        // Register synthetic trial as soon as possible.
        UmaSessionStats.registerSyntheticFieldTrial(TRIGGERED_SYNTHETIC_TRIAL, ENABLED_GROUP);
        // Synthetic trial for experiments.
        String experimentIds = getExperimentIds(bundleExtras);
        if (!experimentIds.isEmpty()) {
            for (String experimentId : experimentIds.split(",")) {
                UmaSessionStats.registerSyntheticFieldTrial(
                        EXPERIMENTS_SYNTHETIC_TRIAL, experimentId);
            }
        }

        // Have an "attempted starts" baseline for the drop out histogram.
        AutofillAssistantMetrics.recordDropOut(DropOutReason.AA_START);
        waitForTabWithWebContents(activity, tab -> {
            AutofillAssistantModuleEntryProvider.INSTANCE.getModuleEntry(
                    tab, (moduleEntry) -> {
                        if (moduleEntry == null) {
                            AutofillAssistantMetrics.recordDropOut(
                                    DropOutReason.DFM_INSTALL_FAILED);
                            return;
                        }

                        Map<String, String> parameters = extractParameters(bundleExtras);
                        parameters.remove(PARAMETER_ENABLED);
                        moduleEntry.start(tab, tab.getWebContents(),
                                !AutofillAssistantPreferencesUtil.getShowOnboarding(), initialUrl,
                                parameters, experimentIds, bundleExtras);
                    });
        });
    }

    /**
     * Checks whether direct actions provided by Autofill Assistant should be available - assuming
     * that direct actions are available at all.
     */
    public static boolean areDirectActionsAvailable(@ActivityType int activityType) {
        return BuildInfo.isAtLeastQ()
                && (activityType == ActivityType.CUSTOM_TAB || activityType == ActivityType.TABBED)
                && ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_ASSISTANT_DIRECT_ACTIONS)
                && ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_ASSISTANT);
    }

    /**
     * Returns a {@link DirectActionHandler} for making dynamic actions available under Android Q.
     *
     * <p>This should only be called if {@link #areDirectActionsAvailable} returns true. This method
     * can also return null if autofill assistant is not available for some other reasons.
     */
    public static DirectActionHandler createDirectActionHandler(Context context,
            BottomSheetController bottomSheetController, ScrimView scrimView,
            TabModelSelector tabModelSelector) {
        // TODO(b/134740534): Consider restricting signature of createDirectActionHandler() to get
        // only getCurrentTab instead of a TabModelSelector.
        return new AutofillAssistantDirectActionHandler(context, bottomSheetController, scrimView,
                tabModelSelector::getCurrentTab, AutofillAssistantModuleEntryProvider.INSTANCE);
    }

    /**
     * In M74 experiment ids might come from parameters. This function merges both exp ids from
     * special intent and parameters.
     * @return Comma-separated list of active experiment ids.
     */
    private static String getExperimentIds(@Nullable Bundle bundleExtras) {
        if (bundleExtras == null) {
            return "";
        }

        StringBuilder experiments = new StringBuilder();
        Map<String, String> parameters = extractParameters(bundleExtras);
        if (parameters.containsKey(EXPERIMENT_IDS_IDENTIFIER)) {
            experiments.append(parameters.get(EXPERIMENT_IDS_IDENTIFIER));
        }

        String experimentsFromIntent = IntentUtils.safeGetString(bundleExtras, EXPERIMENT_IDS_NAME);
        if (experimentsFromIntent != null) {
            if (experiments.length() > 0 && !experiments.toString().endsWith(",")) {
                experiments.append(",");
            }
            experiments.append(experimentsFromIntent);
        }
        return experiments.toString();
    }

    /** Return the value if the given boolean parameter from the extras. */
    private static boolean getBooleanParameter(@Nullable Bundle extras, String parameterName) {
        return extras != null
                && IntentUtils.safeGetBoolean(extras, INTENT_EXTRA_PREFIX + parameterName, false);
    }

    /** Returns a map containing the extras starting with {@link #INTENT_EXTRA_PREFIX}. */
    private static Map<String, String> extractParameters(@Nullable Bundle extras) {
        Map<String, String> result = new HashMap<>();
        if (extras != null) {
            for (String key : extras.keySet()) {
                try {
                    if (key.startsWith(INTENT_EXTRA_PREFIX)
                            && !key.startsWith(INTENT_SPECIAL_PREFIX)) {
                        result.put(key.substring(INTENT_EXTRA_PREFIX.length()),
                                URLDecoder.decode(extras.get(key).toString(), "UTF-8"));
                    }
                } catch (java.io.UnsupportedEncodingException e) {
                    throw new IllegalStateException("UTF-8 encoding not available.", e);
                }
            }
        }
        return result;
    }

    /** Provides the callback with a tab that has a web contents, waits if necessary. */
    private static void waitForTabWithWebContents(ChromeActivity activity, Callback<Tab> callback) {
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

    public static boolean isAutofillAssistantEnabled(Intent intent) {
        // Check for configuration first for early return to prevent test failures on checking
        // feature flags.
        return AutofillAssistantFacade.isConfigured(intent.getExtras())
                && ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_ASSISTANT);
    }

    public static boolean isAutofillAssistantByIntentTriggeringEnabled(Intent intent) {
        // Check for configuration first for early return to prevent test failures on checking
        // feature flags.
        return AutofillAssistantFacade.isAutofillAssistantEnabled(intent)
                && ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_ASSISTANT_CHROME_ENTRY);
    }
}
