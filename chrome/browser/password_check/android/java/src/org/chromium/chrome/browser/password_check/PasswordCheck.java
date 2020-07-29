// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_check;

import android.content.Context;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * This component allows to check for compromised passwords. It provides a settings page which shows
 * the compromised passwords and exposes actions that will help the users to make safer their
 * credentials.
 */
public interface PasswordCheck extends PasswordCheckComponentUi.Delegate {
    @IntDef({CheckStatus.SUCCESS, CheckStatus.RUNNING, CheckStatus.ERROR_OFFLINE,
            CheckStatus.ERROR_NO_PASSWORDS, CheckStatus.ERROR_SIGNED_OUT,
            CheckStatus.ERROR_QUOTA_LIMIT, CheckStatus.ERROR_UNKNOWN})
    @Retention(RetentionPolicy.SOURCE)
    public @interface CheckStatus {
        /** The check was completed without errors. */
        int SUCCESS = 1;
        /** The check is still running. */
        int RUNNING = 2;
        /** The check cannot run because the user is offline. */
        int ERROR_OFFLINE = 3;
        /** The check cannot run because the user has no passwords on this device. */
        int ERROR_NO_PASSWORDS = 4;
        /** The check is cannot run because the user is signed-out. */
        int ERROR_SIGNED_OUT = 5;
        /** The check is cannot run because the user has exceeded their quota. */
        int ERROR_QUOTA_LIMIT = 6;
        /** The check is cannot run for unknown reasons. */
        int ERROR_UNKNOWN = 6;
    }

    /** Observes events and state changes of the password check. */
    interface Observer {
        /**
         * Gets invoked when the compromised credentials are fetched from the disk.
         * After this call, {@link getCompromisedCredentialsCount} returns a valid value.
         */
        void onCompromisedCredentialsFetchCompleted();

        /**
         * Gets invoked when the saved passwords are fetched from the disk.
         * After this call, {@link getSavedPasswordsCount} returns a valid value.
         */
        void onSavedPasswordsFetchCompleted();

        /**
         * Gets invoked once the password check stops running.
         * @param status A {@link CheckStatus} enum value.
         */
        void onPasswordCheckStateChanged(@CheckStatus int status);
    }

    /**
     * Initializes the PasswordCheck UI and launches it.
     * @param context A {@link Context} to create views and retrieve resources.
     * @param passwordCheckReferrer The place which launched the check UI.
     */
    void showUi(Context context, @PasswordCheckReferrer int passwordCheckReferrer);

    /**
     * Cleans up the C++ part, thus removing the compromised credentials from memory.
     */
    void destroy();

    /**
     * Adds a new observer to the list of observers
     * @param obs An {@link Observer} implementation instance.
     * @param callImmediatelyIfReady Invokes {@link onCompromisedCredentialsFetchCompleted} and
     *   {@link onSavedPasswordsFetchCompleted} on the observer if the corresponding data is already
     *   fetched when this is true.
     */
    void addObserver(Observer obs, boolean callImmediatelyIfReady);

    /**
     * Removes a given observer from the observers list if it is there.
     * @param obs An {@link Observer} implementation instance.
     */
    void removeObserver(Observer obs);

    /**
     * @return The latest available number of compromised passwords. If this is invoked before
     * {@link onCompromisedCredentialsFetchCompleted}, the returned value is likely invalid.
     */
    int getCompromisedCredentialsCount();

    /**
     * @return The latest available number of all saved passwords. If this is invoked before
     * {@link onSavedPasswordsFetchCompleted}, the returned value is likely invalid.
     */
    int getSavedPasswordsCount();

    /**
     * Starts the password check, if one is not running already.
     */
    void startCheck();

    /**
     * Stops the password check, if one is running.
     */
    void stopCheck();
}
