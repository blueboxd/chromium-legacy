// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.content.Context;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.text.TextUtils;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.directactions.DirectActionHandler;
import org.chromium.chrome.browser.directactions.DirectActionReporter;
import org.chromium.chrome.browser.directactions.DirectActionReporter.Type;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.widget.ScrimView;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;

import java.util.Collections;
import java.util.Set;

/**
 * A handler that provides just enough functionality to allow on-demand loading of the module
 * through direct actions. The actual implementation is in the module.
 */
public class AutofillAssistantDirectActionHandler implements DirectActionHandler {
    private static final String LIST_AA_ACTIONS = "list_assistant_actions";
    private static final String LIST_AA_ACTIONS_RESULT = "names";
    private static final String PERFORM_AA_ACTION = "perform_assistant_action";
    private static final String PERFORM_AA_ACTION_RESULT = "success";
    private static final String ACTION_NAME = "name";
    private static final String EXPERIMENT_IDS = "experiment_ids";
    private static final String ONBOARDING_ACTION = "onboarding";
    private static final String USER_NAME = "user_name";

    /**
     * Set of actions to report when AA is not available, because of preference or
     * lack of DFM.
     */
    private static final Set<String> FALLBACK_ACTION_SET = Collections.singleton(ONBOARDING_ACTION);

    private final Context mContext;
    private final BottomSheetController mBottomSheetController;
    private final ScrimView mScrimView;
    private final GetCurrentTab mGetCurrentTab;
    private final AutofillAssistantModuleEntryProvider mModuleEntryProvider;

    @Nullable
    private AutofillAssistantActionHandler mDelegate;

    AutofillAssistantDirectActionHandler(Context context,
            BottomSheetController bottomSheetController, ScrimView scrimView,
            GetCurrentTab getCurrentTab, AutofillAssistantModuleEntryProvider moduleEntryProvider) {
        mContext = context;
        mBottomSheetController = bottomSheetController;
        mScrimView = scrimView;
        mGetCurrentTab = getCurrentTab;
        mModuleEntryProvider = moduleEntryProvider;
    }

    @Override
    public void reportAvailableDirectActions(DirectActionReporter reporter) {
        if (!AutofillAssistantPreferencesUtil.isAutofillAssistantSwitchOn()) return;

        reporter.addDirectAction(LIST_AA_ACTIONS)
                .withParameter(USER_NAME, Type.STRING, /* required= */ false)
                .withParameter(EXPERIMENT_IDS, Type.STRING, /* required= */ false)
                .withResult(LIST_AA_ACTIONS_RESULT, Type.STRING);

        reporter.addDirectAction(PERFORM_AA_ACTION)
                .withParameter(ACTION_NAME, Type.STRING, /* required= */ false)
                .withParameter(EXPERIMENT_IDS, Type.STRING, /* required= */ false)
                .withResult(PERFORM_AA_ACTION_RESULT, Type.BOOLEAN);
    }

    @Override
    public boolean performDirectAction(
            String actionId, Bundle arguments, Callback<Bundle> callback) {
        if (actionId.equals(LIST_AA_ACTIONS)) {
            listActions(arguments, callback);
            return true;
        }
        if (actionId.equals(PERFORM_AA_ACTION)) {
            performAction(arguments, callback);
            return true;
        }
        return false;
    }

    private void listActions(Bundle arguments, Callback<Bundle> bundleCallback) {
        Callback<Set<String>> namesCallback = (names) -> {
            Bundle bundle = new Bundle();
            bundle.putString(LIST_AA_ACTIONS_RESULT, TextUtils.join(",", names));
            bundleCallback.onResult(bundle);
        };

        if (!AutofillAssistantPreferencesUtil.isAutofillAssistantSwitchOn()) {
            namesCallback.onResult(Collections.emptySet());
            return;
        }

        if (!AutofillAssistantPreferencesUtil.isAutofillOnboardingAccepted()) {
            namesCallback.onResult(FALLBACK_ACTION_SET);
            return;
        }

        String userName = arguments.getString(USER_NAME, "");
        arguments.remove(USER_NAME);

        String experimentIds = arguments.getString(EXPERIMENT_IDS, "");
        arguments.remove(EXPERIMENT_IDS);

        getDelegate(/* installIfNecessary= */ false, (delegate) -> {
            if (delegate == null) {
                namesCallback.onResult(FALLBACK_ACTION_SET);
                return;
            }
            delegate.listActions(userName, experimentIds, arguments, namesCallback);
        });
    }

    private void performAction(Bundle arguments, Callback<Bundle> bundleCallback) {
        Callback<Boolean> booleanCallback = (result) -> {
            Bundle bundle = new Bundle();
            bundle.putBoolean(PERFORM_AA_ACTION_RESULT, result);
            bundleCallback.onResult(bundle);
        };

        if (!AutofillAssistantPreferencesUtil.isAutofillAssistantSwitchOn()) {
            booleanCallback.onResult(false);
            return;
        }

        String name = arguments.getString(ACTION_NAME, "");
        arguments.remove(ACTION_NAME);

        String experimentIds = arguments.getString(EXPERIMENT_IDS, "");
        arguments.remove(EXPERIMENT_IDS);

        getDelegate(/* installIfNecessary= */ true, (delegate) -> {
            if (delegate == null) {
                booleanCallback.onResult(false);
                return;
            }
            if (ONBOARDING_ACTION.equals(name)) {
                delegate.performOnboarding(experimentIds, booleanCallback);
                return;
            }
            delegate.performAction(name, experimentIds, arguments, booleanCallback);
        });
    }

    /**
     * Builds the delegate, if possible, and pass it to the callback.
     *
     * <p>If necessary, this function creates a delegate instance and keeps it in {@link
     * #mDelegate}.
     *
     * @param installIfNecessary if true, install the DFM if necessary
     * @param callback callback to report the delegate to
     */
    private void getDelegate(
            boolean installIfNecessary, Callback<AutofillAssistantActionHandler> callback) {
        if (mDelegate == null) {
            mDelegate = createDelegate(mModuleEntryProvider.getModuleEntryIfInstalled(mContext));
        }
        if (mDelegate != null || !installIfNecessary) {
            callback.onResult(mDelegate);
            return;
        }

        Tab tab = mGetCurrentTab.get();
        if (tab == null) {
            // TODO(b/134741524): Allow DFM loading UI to work with no tabs.
            callback.onResult(null);
            return;
        }
        mModuleEntryProvider.getModuleEntry(mContext, tab, (entry) -> {
            mDelegate = createDelegate(entry);
            callback.onResult(mDelegate);
        });
    }

    /** Creates a delegate from the given {@link AutofillAssistantModuleEntry}, if possible. */
    @Nullable
    private AutofillAssistantActionHandler createDelegate(
            @Nullable AutofillAssistantModuleEntry entry) {
        if (entry == null) return null;

        return entry.createActionHandler(
                mContext, mBottomSheetController, mScrimView, mGetCurrentTab);
    }
}
