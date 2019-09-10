// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.safe_browsing;

import android.support.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Java interface that a SafeBrowsingApiHandler must implement when used with
 * {@code SafeBrowsingApiBridge}
 */
public interface SafeBrowsingApiHandler {
    // Implementors must provide a no-arg constructor to be instantiated via reflection.

    /**
     * Observer to be notified when the SafeBrowsingApiHandler determines the verdict for a url.
     */
    public interface Observer {
        // Note: |checkDelta| is the time the remote call took in microseconds.
        void onUrlCheckDone(long callbackId, @SafeBrowsingResult int resultStatus, String metadata,
                long checkDelta);
    }

    // Possible values for resultStatus. Native side has the same definitions.
    @IntDef({SafeBrowsingResult.INTERNAL_ERROR, SafeBrowsingResult.SUCCESS,
            SafeBrowsingResult.TIMEOUT})
    @Retention(RetentionPolicy.SOURCE)
    @interface SafeBrowsingResult {
        int INTERNAL_ERROR = -1;
        int SUCCESS = 0;
        int TIMEOUT = 1;
    }

    /**
     * Verifies that SafeBrowsingApiHandler can operate and initializes if feasible.
     * Should be called on the same sequence as |startUriLookup|.
     *
     * @param observer The object on which to call the callback functions when URL checking
     * is complete.
     *
     * @return whether Safe Browsing is supported for this installation.
     */
    public boolean init(Observer result);

    /**
     * Verifies that SafeBrowsingApiHandler can operate and initializes if feasible.
     * Should be called on the same sequence as |startUriLookup|.
     *
     * @param observer The object on which to call the callback functions when URL checking
     * is complete.
     * @param enableLocalBlacklists specifies if the feature to use local blacklists is enabled.
     *
     * @return whether Safe Browsing is supported for this installation.
     */
    public boolean init(Observer result, boolean enableLocalBlacklists);

    /**
     * Returns the Safety Net ID of the device. Checks to make sure that the feature to report
     * telemetry for APK downloads is enabled. This should not be called for AW.
     *
     * @return the Safety Net ID of the device.
     */
    public String getSafetyNetId();

    /**
     * Start a URI-lookup to determine if it matches one of the specified threats.
     * This is called on every URL resource Chrome loads, on the same sequence as |init|.
     */
    public void startUriLookup(long callbackId, String uri, int[] threatsOfInterest);

    /**
     * Start a check to determine if a uri is in a set of allowlists. If true, password protection
     * service will assume the url to be safe and skip it. This feature is not applicable to Android
     * Webview, because there is no saved password or GAIA password in AW.
     *
     * @param uri The uri from a password protection event(user focuses on password form
     *      * or user reuses their password)
     * @param threatsOfInterest determines the type of allowlists that the uri will be matched to.
     *
     * @return true if the uri is found in the set of allowlists. Otherwise, false.
     */
    // TODO(xinghuilu@): remove default once downstream implementation is patched
    default boolean startAllowlistLookup(String uri, int[] threatsOfInterest) {
        return false;
    }
}
