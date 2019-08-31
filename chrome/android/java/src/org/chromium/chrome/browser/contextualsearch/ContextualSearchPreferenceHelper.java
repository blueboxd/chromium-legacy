// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import org.chromium.base.VisibleForTesting;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefChangeRegistrar;

/** Helps Contextual Search manage it's preference and coordinate with Unified Consent. */
class ContextualSearchPreferenceHelper {
    // A Feature param that enables throttling of this user's Resolve requests to limit server QPS.
    // If the param is true, then we fallback onto the behavior previous to Unified Consent.
    public static final String THROTTLE = "throttle";

    // Our singleton instance.
    private static ContextualSearchPreferenceHelper sInstance;

    // Pointer to the native instance of this class.
    private long mNativePointer;

    /** Gets the singleton instance for this class. */
    static ContextualSearchPreferenceHelper getInstance() {
        if (sInstance == null) sInstance = new ContextualSearchPreferenceHelper();
        return sInstance;
    }

    /** Constructs the singleton Unified Consent Helper instance. */
    private ContextualSearchPreferenceHelper() {
        mNativePointer = ContextualSearchPreferenceHelperJni.get().init(
                ContextualSearchPreferenceHelper.this);
        onPreferenceChangedInternal();
        new PrefChangeRegistrar().addObserver(
                Pref.CONTEXTUAL_SEARCH_ENABLED, new PrefChangeRegistrar.PrefObserver() {
                    @Override
                    public void onPreferenceChange() {
                        onPreferenceChangedInternal();
                    }
                });
    }

    /**
     * This method should be called to clean up storage when an instance of this class is
     * no longer in use.  The ContextualSearchPreferenceHelperJni.get().destroy will call the
     * destructor on the native instance.
     */
    void destroy() {
        assert mNativePointer != 0;
        ContextualSearchPreferenceHelperJni.get().destroy(
                mNativePointer, ContextualSearchPreferenceHelper.this);
        mNativePointer = 0;
    }

    /**
     * Determines whether requests for this user should be throttled or not.
     * Checks if the Feature is enabled and has the "throttle" param set, and if the user was
     * previously undecided, and if so then throttling is allowed and the user can be treated as if
     * they are still undecided.
     * @return Whether it's OK to throttle Resolve requests for this user, because they were
     *         previously undecided.
     */
    public boolean canThrottle() {
        boolean isRequestThrottled = ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                                             ChromeFeatureList.CONTEXTUAL_SEARCH_UNITY_INTEGRATION,
                                             ContextualSearchPreferenceHelper.THROTTLE, false)
                && wasUndecided();
        ContextualSearchUma.logUnifiedConsentThrottledRequests(isRequestThrottled);
        return isRequestThrottled;
    }

    /** Updates our preference metadata storage based on what our native member knows. */
    void onPreferenceChangedInternal() {
        updatePreviousPreferenceStorage(
                ContextualSearchPreferenceHelperJni.get().getPreferenceMetadata(
                        mNativePointer, ContextualSearchPreferenceHelper.this));
    }

    /**
     * Updates the metadata storage for the setting previous to Unified Consent.
     * @param previousPreferenceMetadata The value to persist, if not yet known.
     */
    private void updatePreviousPreferenceStorage(
            @ContextualSearchPreviousPreferenceMetadata int previousPreferenceMetadata) {
        if (previousPreferenceMetadata == ContextualSearchPreviousPreferenceMetadata.UNKNOWN
                || !ChromeFeatureList.isEnabled(
                           ChromeFeatureList.CONTEXTUAL_SEARCH_UNITY_INTEGRATION)
                || hasPreUnifiedConsentPreference())
            return;

        recordPreUnifiedConsentPreference(previousPreferenceMetadata);
    }

    /** @return Whether the user was "undecided" before Unified Consent changed their setting. */
    private boolean wasUndecided() {
        boolean isThrottleEligible = ContextualSearchPreviousPreferenceMetadata.WAS_UNDECIDED
                == ChromePreferenceManager.getInstance().readInt(
                           ChromePreferenceManager.CONTEXTUAL_SEARCH_PRE_UNIFIED_CONSENT_PREF);
        ContextualSearchUma.logUnifiedConsentThrottleEligible(isThrottleEligible);
        return isThrottleEligible;
    }

    /** @return Whether we know if Unified Consent changed their setting. */
    private boolean hasPreUnifiedConsentPreference() {
        return ContextualSearchPreviousPreferenceMetadata.UNKNOWN
                != ChromePreferenceManager.getInstance().readInt(
                           ChromePreferenceManager.CONTEXTUAL_SEARCH_PRE_UNIFIED_CONSENT_PREF);
    }

    /** Records the metadata for the setting previous to the change made by Unified Consent. */
    private void recordPreUnifiedConsentPreference(
            @ContextualSearchPreviousPreferenceMetadata int setting) {
        assert setting != ContextualSearchPreviousPreferenceMetadata.UNKNOWN;
        ChromePreferenceManager.getInstance().writeInt(
                ChromePreferenceManager.CONTEXTUAL_SEARCH_PRE_UNIFIED_CONSENT_PREF, setting);
        ContextualSearchUma.logUnifiedConsentPreviousEnabledState(
                setting == ContextualSearchPreviousPreferenceMetadata.WAS_UNDECIDED);
    }

    /**
     * Applies previous metadata as would be recorded by native Unified Consent integration, for use
     * by tests only.
     * @param metadata The metadata expressing the previous user's enabled-state.
     */
    @VisibleForTesting
    void applyUnifiedConsentGivenMetadata(
            @ContextualSearchPreviousPreferenceMetadata int metadata) {
        ChromePreferenceManager.getInstance().writeInt(
                ChromePreferenceManager.CONTEXTUAL_SEARCH_PRE_UNIFIED_CONSENT_PREF, metadata);
    }

    @NativeMethods
    interface Natives {
        long init(ContextualSearchPreferenceHelper caller);
        void destroy(long nativeContextualSearchPreferenceHelper,
                ContextualSearchPreferenceHelper caller);
        int getPreferenceMetadata(long nativeContextualSearchPreferenceHelper,
                ContextualSearchPreferenceHelper caller);
    }
}
