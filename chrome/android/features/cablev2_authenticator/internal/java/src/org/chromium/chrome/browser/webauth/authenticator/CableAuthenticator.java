// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauth.authenticator;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.IntentSender;
import android.net.Uri;
import android.os.Bundle;

import com.google.android.gms.fido.Fido;
import com.google.android.gms.fido.fido2.Fido2PrivilegedApiClient;
import com.google.android.gms.fido.fido2.api.common.Attachment;
import com.google.android.gms.fido.fido2.api.common.AttestationConveyancePreference;
import com.google.android.gms.fido.fido2.api.common.AuthenticatorAssertionResponse;
import com.google.android.gms.fido.fido2.api.common.AuthenticatorAttestationResponse;
import com.google.android.gms.fido.fido2.api.common.AuthenticatorErrorResponse;
import com.google.android.gms.fido.fido2.api.common.AuthenticatorResponse;
import com.google.android.gms.fido.fido2.api.common.AuthenticatorSelectionCriteria;
import com.google.android.gms.fido.fido2.api.common.BrowserPublicKeyCredentialCreationOptions;
import com.google.android.gms.fido.fido2.api.common.PublicKeyCredential;
import com.google.android.gms.fido.fido2.api.common.PublicKeyCredentialCreationOptions;
import com.google.android.gms.fido.fido2.api.common.PublicKeyCredentialDescriptor;
import com.google.android.gms.fido.fido2.api.common.PublicKeyCredentialParameters;
import com.google.android.gms.fido.fido2.api.common.PublicKeyCredentialRpEntity;
import com.google.android.gms.fido.fido2.api.common.PublicKeyCredentialType;
import com.google.android.gms.fido.fido2.api.common.PublicKeyCredentialUserEntity;
import com.google.android.gms.tasks.Task;

import org.chromium.base.Log;

import java.util.ArrayList;
import java.util.List;

/**
 * CableAuthenticator implements makeCredential and getAssertion operations on top of the Privileged
 * FIDO2 API.
 */
class CableAuthenticator {
    private static final String TAG = "CableAuthenticator";
    private static final String FIDO2_KEY_CREDENTIAL_EXTRA = "FIDO2_CREDENTIAL_EXTRA";
    private static final int FIDO2_REGISTER_INTENT_CODE = 1;
    private static final int CTAP2_OK = 0;
    private static final int CTAP2_ERR_OPERATION_DENIED = 0x27;

    private final Context mContext;
    private final CableAuthenticatorUI mUi;
    private final BLEHandler mBleHandler;

    private long mClientAddress;

    public CableAuthenticator(Context context, CableAuthenticatorUI ui) {
        mContext = context;
        mUi = ui;
        mBleHandler = new BLEHandler(this);
        if (!mBleHandler.start()) {
            // TODO: handle the case where exporting the GATT server fails.
        }
    }

    public void makeCredential(long clientAddress) {
        // TODO: handle concurrent requests
        assert mClientAddress == 0;
        mClientAddress = clientAddress;
        Fido2PrivilegedApiClient client = Fido.getFido2PrivilegedApiClient(mContext);
        if (client == null) {
            Log.i(TAG, "getFido2PrivilegedApiClient failed");
            return;
        }
        Log.i(TAG, "have fido client");

        // TODO: use actual request parameters passed from native code
        List<PublicKeyCredentialParameters> parameters = new ArrayList<>();
        parameters.add(new PublicKeyCredentialParameters(
                PublicKeyCredentialType.PUBLIC_KEY.toString(), -7));
        AuthenticatorSelectionCriteria selection = new AuthenticatorSelectionCriteria.Builder()
                                                           .setAttachment(Attachment.PLATFORM)
                                                           .setRequireResidentKey(false)
                                                           .build();
        List<PublicKeyCredentialDescriptor> excludeCredentials =
                new ArrayList<PublicKeyCredentialDescriptor>();
        byte[] dummy = new byte[32];
        PublicKeyCredentialCreationOptions credentialCreationOptions =
                new PublicKeyCredentialCreationOptions.Builder()
                        .setRp(new PublicKeyCredentialRpEntity("example.com", "Example", null))
                        .setUser(
                                new PublicKeyCredentialUserEntity(dummy, "user", null, "User Name"))
                        .setChallenge(dummy)
                        .setParameters(parameters)
                        .setTimeoutSeconds(Double.valueOf(20))
                        .setExcludeList(excludeCredentials)
                        .setAuthenticatorSelection(selection)
                        .setAttestationConveyancePreference(AttestationConveyancePreference.NONE)
                        .build();
        BrowserPublicKeyCredentialCreationOptions browserRequestOptions =
                new BrowserPublicKeyCredentialCreationOptions.Builder()
                        .setPublicKeyCredentialCreationOptions(credentialCreationOptions)
                        .setOrigin(Uri.parse("https://example.com"))
                        .build();
        Task<PendingIntent> result = client.getRegisterPendingIntent(browserRequestOptions);
        result.addOnSuccessListener(pedingIntent -> {
                  Log.i(TAG, "got pending");
                  try {
                      mUi.startIntentSenderForResult(pedingIntent.getIntentSender(),
                              123, // REGISTER_REQUEST_CODE,
                              null, // fillInIntent,
                              0, // flagsMask,
                              0, // flagsValue,
                              0, // extraFlags,
                              Bundle.EMPTY);
                  } catch (IntentSender.SendIntentException e) {
                      Log.e(TAG, "intent failure");
                  }
              }).addOnFailureListener(e -> { Log.e(TAG, "intent failure" + e); });

        Log.i(TAG, "op done");
    }

    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        Log.i(TAG, "onActivityResult " + requestCode + " " + resultCode);

        if (data == null) {
            Log.e(TAG, "Received a null intent.");
            // The user cancelled the request.
            mBleHandler.onAuthenticatorAttestationResponse(
                    mClientAddress, CTAP2_ERR_OPERATION_DENIED);
            return;
        }

        switch (resultCode) {
            case Activity.RESULT_CANCELED:
                Log.e(TAG, "CANCELED.");
                return;
            case Activity.RESULT_OK:
                Log.e(TAG, "OK.");
                break;
            default:
                // Something went wrong.
                Log.e(TAG, "Failed with result code" + resultCode);
                return;
        }

        if (data.hasExtra(FIDO2_KEY_CREDENTIAL_EXTRA)) {
            Log.e(TAG, "cred extra");
            PublicKeyCredential publicKeyCredential = PublicKeyCredential.deserializeFromBytes(
                    data.getByteArrayExtra(FIDO2_KEY_CREDENTIAL_EXTRA));
            AuthenticatorResponse response = publicKeyCredential.getResponse();
            if (response instanceof AuthenticatorErrorResponse) {
                AuthenticatorErrorResponse error = (AuthenticatorErrorResponse) response;
                Log.i(TAG,
                        "error response: " + error.getErrorMessage() + " "
                                + String.valueOf(error.getErrorCodeAsInt()));
            } else if (response instanceof AuthenticatorAttestationResponse) {
                Log.i(TAG, "attestation response");
                mBleHandler.onAuthenticatorAttestationResponse(mClientAddress, CTAP2_OK);
            } else if (response instanceof AuthenticatorAssertionResponse) {
                Log.i(TAG, "assertion response");
            } else {
                Log.i(TAG, "unknown response");
            }
        } else if (data.hasExtra(Fido.FIDO2_KEY_ERROR_EXTRA)) {
            Log.e(TAG, "error extra");
        } else if (data.hasExtra(Fido.FIDO2_KEY_RESPONSE_EXTRA)) {
            Log.e(TAG, "resp extra");
        } else {
            // Something went wrong.
            Log.e(TAG,
                    "The response is missing FIDO2_KEY_RESPONSE_EXTRA "
                            + "and FIDO2_KEY_CREDENTIAL_EXTRA.");
        }
    }

    public void onQRCode(String value) {
        mBleHandler.onQRCode(value);
    }

    public void close() {
        mBleHandler.close();
    }
}
