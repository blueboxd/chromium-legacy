// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import org.chromium.chrome.browser.profiles.Profile;

/**
 * Bridge to UnifiedConsentService.
 */
public class UnifiedConsentServiceBridge {
    private UnifiedConsentServiceBridge() {}

    /** Returns whether collection of URL-keyed anonymized data is enabled. */
    public static boolean isUrlKeyedAnonymizedDataCollectionEnabled() {
        return nativeIsUrlKeyedAnonymizedDataCollectionEnabled(Profile.getLastUsedProfile());
    }

    /** Sets whether collection of URL-keyed anonymized data is enabled. */
    public static void setUrlKeyedAnonymizedDataCollectionEnabled(boolean enabled) {
        nativeSetUrlKeyedAnonymizedDataCollectionEnabled(Profile.getLastUsedProfile(), enabled);
    }

    /** Returns whether collection of URL-keyed anonymized data is configured by policy. */
    public static boolean isUrlKeyedAnonymizedDataCollectionManaged() {
        return nativeIsUrlKeyedAnonymizedDataCollectionManaged(Profile.getLastUsedProfile());
    }

    /**
     * Records the sync data types that were turned off during the advanced sync opt-in flow.
     * See C++ unified_consent::metrics::RecordSyncSetupDataTypesHistrogam for details.
     */
    public static void recordSyncSetupDataTypesHistogram() {
        nativeRecordSyncSetupDataTypesHistogram(Profile.getLastUsedProfile());
    }

    private static native boolean nativeIsUrlKeyedAnonymizedDataCollectionEnabled(Profile profile);
    private static native void nativeSetUrlKeyedAnonymizedDataCollectionEnabled(
            Profile profile, boolean enabled);
    private static native boolean nativeIsUrlKeyedAnonymizedDataCollectionManaged(Profile profile);
    private static native void nativeRecordSyncSetupDataTypesHistogram(Profile profile);
}
