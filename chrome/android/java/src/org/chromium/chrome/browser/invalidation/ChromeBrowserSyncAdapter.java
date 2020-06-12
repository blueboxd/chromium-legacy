// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.invalidation;

import android.accounts.Account;
import android.content.AbstractThreadedSyncAdapter;
import android.content.ContentProviderClient;
import android.content.Context;
import android.content.SyncResult;
import android.os.Bundle;

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
        // Nothing to do; we ignore explicit requests to perform a sync, since Chrome Sync
        // knows when it should perform a sync. We could consider calling
        // ProfileSyncService.triggerRefresh() to honor explicit requests.
    }
}
