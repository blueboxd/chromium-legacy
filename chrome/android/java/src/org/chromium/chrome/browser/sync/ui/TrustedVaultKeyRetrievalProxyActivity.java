// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.ui;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.Intent;
import android.content.IntentSender;
import android.os.Bundle;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.sync.TrustedVaultClient;

/**
 * {@link TrustedVaultKeyRetrievalProxyActivity} has no own UI and just launches real key retrieval
 * activity (passed via extra). The reason for using this proxy activity is to detect when real key
 * retrieval activity finishes and notify TrustedVaultClient about changed keys.
 */
public class TrustedVaultKeyRetrievalProxyActivity extends Activity {
    private static final String KEY_RETRIEVAL_INTENT_NAME = "key_retrieval";
    private static final String TAG = "SyncUI";
    private static final int REQUEST_CODE_TRUSTED_VAULT_KEY_RETRIEVAL = 1;

    /**
     * Creates an intent that launches an TrustedVaultKeyRetrievalProxyActivity.
     *
     * @param keyRetrievalIntent Actual key retrieval intent, which will be launched by
     * TrustedVaultKeyRetrievalProxyActivity.
     *
     * @return the intent for launching TrustedVaultKeyRetrievalProxyActivity
     */
    public static Intent createKeyRetrievalProxyIntent(PendingIntent keyRetrievalIntent) {
        Intent intent = new Intent(
                ContextUtils.getApplicationContext(), TrustedVaultKeyRetrievalProxyActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);
        intent.putExtra(KEY_RETRIEVAL_INTENT_NAME, keyRetrievalIntent);
        return intent;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        PendingIntent keyRetrievalIntent =
                getIntent().getParcelableExtra(KEY_RETRIEVAL_INTENT_NAME);
        assert keyRetrievalIntent != null;
        try {
            startIntentSenderForResult(keyRetrievalIntent.getIntentSender(),
                    REQUEST_CODE_TRUSTED_VAULT_KEY_RETRIEVAL,
                    /* fillInIntent */ null, /* flagsMask */ 0,
                    /* flagsValues */ 0, /* extraFlags */ 0,
                    /* options */ null);
        } catch (IntentSender.SendIntentException exception) {
            Log.w(TAG, "Error sending key retrieval intent: ", exception);
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        assert requestCode == REQUEST_CODE_TRUSTED_VAULT_KEY_RETRIEVAL;

        // Upon key retrieval completion, the keys in TrustedVaultClient could have changed. This is
        // done even if the user cancelled the flow (i.e. resultCode != RESULT_OK) because it's
        // harmless to issue a redundant notifyKeysChanged().
        // The Chrome browser process might be not alive, there is no need to start it, because new
        // keys will be fetched during its next start automatically.
        if (ChromeBrowserInitializer.getInstance().isFullBrowserInitialized()) {
            TrustedVaultClient.get().notifyKeysChanged();
        }
        finish();
    }
}