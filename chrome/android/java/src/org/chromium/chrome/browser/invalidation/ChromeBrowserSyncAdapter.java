// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.invalidation;

import android.accounts.Account;
import android.content.AbstractThreadedSyncAdapter;
import android.content.ContentProviderClient;
import android.content.ContentResolver;
import android.content.Context;
import android.content.SyncResult;
import android.os.Bundle;

import org.chromium.chrome.browser.signin.IdentityServicesProvider;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;

/**
 * A Sync adapter that integrates with Android's OS-level Sync settings.
 */
public class ChromeBrowserSyncAdapter extends AbstractThreadedSyncAdapter {
    public ChromeBrowserSyncAdapter(Context context) {
        super(context, false);
    }

    @Override
    public void onPerformSync(Account account, Bundle extras, String authority,
            ContentProviderClient provider, SyncResult syncResult) {
        // Make sure to only report Chrome as "syncable" if the given account matches our
        // signed-in account.
        if (extras.getBoolean(ContentResolver.SYNC_EXTRAS_INITIALIZE)) {
            Account signedInAccount = CoreAccountInfo.getAndroidAccountFrom(
                    IdentityServicesProvider.get().getIdentityManager().getPrimaryAccountInfo(
                            ConsentLevel.SYNC));
            if (account.equals(signedInAccount)) {
                ContentResolver.setIsSyncable(account, authority, 1);
            } else {
                ContentResolver.setIsSyncable(account, authority, 0);
            }
            return;
        }

        // Else: Nothing to do; we ignore explicit requests to perform a sync, since Chrome Sync
        // knows when it should perform a sync. We could consider calling
        // ProfileSyncService.triggerRefresh() to honor explicit requests.
    }
}
