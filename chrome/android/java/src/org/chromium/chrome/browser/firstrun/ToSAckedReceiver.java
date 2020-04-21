// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;

import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.init.ProcessInitializationHandler;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.components.signin.AccountManagerFacadeProvider;
import org.chromium.components.signin.AccountUtils;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.util.List;
import java.util.Set;

/**
 * This receiver is notified when a user goes through the Setup Wizard and acknowledges
 * the Chrome ToS so that we don't show the ToS string during our first run.
 */
public class ToSAckedReceiver extends BroadcastReceiver {
    static final String EXTRA_ACCOUNT_NAME = "TosAckedReceiver.account";

    @Override
    public void onReceive(Context context, Intent intent) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) return;

        Bundle args = intent.getExtras();
        if (args == null) return;
        String accountName = args.getString(EXTRA_ACCOUNT_NAME, null);
        if (accountName == null) return;

        SharedPreferencesManager prefs = SharedPreferencesManager.getInstance();
        prefs.addToStringSet(ChromePreferenceKeys.TOS_ACKED_ACCOUNTS, accountName);
    }

    /**
     * Checks whether any of the current google accounts has seen the ToS in setup wizard.
     * @return Whether or not the the ToS has been seen.
     */
    public static boolean checkAnyUserHasSeenToS() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) return false;

        Set<String> toSAckedAccounts = SharedPreferencesManager.getInstance().readStringSet(
                ChromePreferenceKeys.TOS_ACKED_ACCOUNTS, null);
        if (toSAckedAccounts == null || toSAckedAccounts.isEmpty()) return false;
        PostTask.runSynchronously(UiThreadTaskTraits.DEFAULT,
                () -> { ProcessInitializationHandler.getInstance().initializePreNative(); });
        List<String> accountNames = AccountUtils.toAccountNames(
                AccountManagerFacadeProvider.getInstance().tryGetGoogleAccounts());
        if (accountNames.isEmpty()) return false;
        for (int k = 0; k < accountNames.size(); k++) {
            if (toSAckedAccounts.contains(accountNames.get(k))) return true;
        }
        return false;
    }
}
