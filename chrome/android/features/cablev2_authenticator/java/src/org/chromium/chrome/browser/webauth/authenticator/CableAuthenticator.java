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
import com.google.android.gms.fido.common.Transport;
import com.google.android.gms.fido.fido2.Fido2PrivilegedApiClient;
import com.google.android.gms.fido.fido2.api.common.Attachment;
import com.google.android.gms.fido.fido2.api.common.AttestationConveyancePreference;
import com.google.android.gms.fido.fido2.api.common.AuthenticatorAssertionResponse;
import com.google.android.gms.fido.fido2.api.common.AuthenticatorAttestationResponse;
import com.google.android.gms.fido.fido2.api.common.AuthenticatorErrorResponse;
import com.google.android.gms.fido.fido2.api.common.AuthenticatorSelectionCriteria;
import com.google.android.gms.fido.fido2.api.common.BrowserPublicKeyCredentialCreationOptions;
import com.google.android.gms.fido.fido2.api.common.BrowserPublicKeyCredentialRequestOptions;
import com.google.android.gms.fido.fido2.api.common.PublicKeyCredential;
import com.google.android.gms.fido.fido2.api.common.PublicKeyCredentialCreationOptions;
import com.google.android.gms.fido.fido2.api.common.PublicKeyCredentialDescriptor;
import com.google.android.gms.fido.fido2.api.common.PublicKeyCredentialParameters;
import com.google.android.gms.fido.fido2.api.common.PublicKeyCredentialRequestOptions;
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
    private static final double TIMEOUT_SECONDS = 20;

    private static final int REGISTER_REQUEST_CODE = 1;
    private static final int SIGN_REQUEST_CODE = 2;

    private static final int CTAP2_OK = 0;
    private static final int CTAP2_ERR_OPERATION_DENIED = 0x27;
    private static final int CTAP2_ERR_UNSUPPORTED_OPTION = 0x2D;
    private static final int CTAP2_ERR_OTHER = 0x7F;

    private final Context mContext;
    private final CableAuthenticatorUI mUi;
    private final Callback mCallback;
    private final BLEHandler mBleHandler;

    private long mClientAddress;

    public enum Result {
        REGISTER_OK,
        REGISTER_ERROR,
        SIGN_OK,
        SIGN_ERROR,
        OTHER,
    }

    // Callbacks notifying the UI of certain CableAuthenticator state changes. These may run on any
    // thread.
    public interface Callback {
        // Invoked when the authenticator has completed a handshake with a client device.
        void onAuthenticatorConnected();
        // Invoked when the authenticator has finished. The UI should be dismissed at this point.
        void onAuthenticatorResult(Result result);
    }

    public CableAuthenticator(Context context, CableAuthenticatorUI ui) {
        mContext = context;
        mUi = ui;
        mCallback = ui;
        mBleHandler = new BLEHandler(this);
        if (!mBleHandler.start()) {
            // TODO: handle the case where exporting the GATT server fails.
        }
    }

    public void notifyAuthenticatorConnected() {
        mCallback.onAuthenticatorConnected();
    }

    public void makeCredential(long clientAddress, String origin, String rpId, byte[] challenge,
            byte[] userId, int[] algorithms, byte[][] excludedCredentialIds,
            boolean residentKeyRequired) {
        // TODO: handle concurrent requests
        assert mClientAddress == 0;
        mClientAddress = clientAddress;
        Fido2PrivilegedApiClient client = Fido.getFido2PrivilegedApiClient(mContext);
        if (client == null) {
            Log.i(TAG, "getFido2PrivilegedApiClient failed");
            return;
        }
        Log.i(TAG, "have fido client");

        List<PublicKeyCredentialParameters> parameters = new ArrayList<>();
        for (int i = 0; i < algorithms.length; i++) {
            try {
                parameters.add(new PublicKeyCredentialParameters(
                        PublicKeyCredentialType.PUBLIC_KEY.toString(), algorithms[i]));
            } catch (IllegalArgumentException e) {
                // The FIDO API will throw IllegalArgumentException for unrecognised algorithms.
                // Since an authenticator ignores unknown algorithms, this exception just needs to
                // be caught and ignored.
            }
        }
        // The GmsCore FIDO2 API does not actually support resident keys yet.
        AuthenticatorSelectionCriteria selection = new AuthenticatorSelectionCriteria.Builder()
                                                           .setAttachment(Attachment.PLATFORM)
                                                           .build();
        List<PublicKeyCredentialDescriptor> excludeCredentials =
                new ArrayList<PublicKeyCredentialDescriptor>();
        for (int i = 0; i < excludedCredentialIds.length; i++) {
            excludeCredentials.add(
                    new PublicKeyCredentialDescriptor(PublicKeyCredentialType.PUBLIC_KEY.toString(),
                            excludedCredentialIds[i], new ArrayList<Transport>()));
        }
        byte[] dummy = new byte[32];
        PublicKeyCredentialCreationOptions credentialCreationOptions =
                new PublicKeyCredentialCreationOptions.Builder()
                        .setRp(new PublicKeyCredentialRpEntity(rpId, "", ""))
                        .setUser(new PublicKeyCredentialUserEntity(userId, "", null, ""))
                        .setChallenge(challenge)
                        .setParameters(parameters)
                        .setTimeoutSeconds(TIMEOUT_SECONDS)
                        .setExcludeList(excludeCredentials)
                        .setAuthenticatorSelection(selection)
                        .setAttestationConveyancePreference(AttestationConveyancePreference.NONE)
                        .build();
        BrowserPublicKeyCredentialCreationOptions browserRequestOptions =
                new BrowserPublicKeyCredentialCreationOptions.Builder()
                        .setPublicKeyCredentialCreationOptions(credentialCreationOptions)
                        .setOrigin(Uri.parse(origin))
                        .build();
        Task<PendingIntent> result = client.getRegisterPendingIntent(browserRequestOptions);
        result.addOnSuccessListener(pedingIntent -> {
                  Log.i(TAG, "got pending");
                  try {
                      mUi.startIntentSenderForResult(pedingIntent.getIntentSender(),
                              REGISTER_REQUEST_CODE,
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

    public void getAssertion(long clientAddress, String origin, String rpId, byte[] challenge,
            byte[][] allowedCredentialIds) {
        // TODO: handle concurrent requests
        assert mClientAddress == 0;
        mClientAddress = clientAddress;

        Fido2PrivilegedApiClient client = Fido.getFido2PrivilegedApiClient(mContext);
        if (client == null) {
            Log.i(TAG, "getFido2PrivilegedApiClient failed");
            return;
        }
        Log.i(TAG, "have fido client");

        List<PublicKeyCredentialDescriptor> allowCredentials =
                new ArrayList<PublicKeyCredentialDescriptor>();
        ArrayList<Transport> transports = new ArrayList<Transport>();
        transports.add(Transport.INTERNAL);
        for (int i = 0; i < allowedCredentialIds.length; i++) {
            allowCredentials.add(
                    new PublicKeyCredentialDescriptor(PublicKeyCredentialType.PUBLIC_KEY.toString(),
                            allowedCredentialIds[i], transports));
        }

        PublicKeyCredentialRequestOptions credentialRequestOptions =
                new PublicKeyCredentialRequestOptions.Builder()
                        .setAllowList(allowCredentials)
                        .setChallenge(challenge)
                        .setRpId(rpId)
                        .setTimeoutSeconds(TIMEOUT_SECONDS)
                        .build();

        BrowserPublicKeyCredentialRequestOptions browserRequestOptions =
                new BrowserPublicKeyCredentialRequestOptions.Builder()
                        .setPublicKeyCredentialRequestOptions(credentialRequestOptions)
                        .setOrigin(Uri.parse(origin))
                        .build();

        Task<PendingIntent> result = client.getSignPendingIntent(browserRequestOptions);
        result.addOnSuccessListener(pedingIntent -> {
                  Log.i(TAG, "got pending");
                  try {
                      mUi.startIntentSenderForResult(pedingIntent.getIntentSender(),
                              SIGN_REQUEST_CODE,
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

        Result result = Result.OTHER;
        switch (requestCode) {
            case REGISTER_REQUEST_CODE:
                if (onRegisterResponse(resultCode, data)) {
                    result = Result.REGISTER_OK;
                } else {
                    result = Result.REGISTER_ERROR;
                }
                break;
            case SIGN_REQUEST_CODE:
                if (onSignResponse(resultCode, data)) {
                    result = Result.SIGN_OK;
                } else {
                    result = Result.SIGN_ERROR;
                }
                break;
            default:
                Log.i(TAG, "invalid requestCode: " + requestCode);
                assert (false);
        }
        mCallback.onAuthenticatorResult(result);
    }

    public boolean onRegisterResponse(int resultCode, Intent data) {
        if (resultCode != Activity.RESULT_OK || data == null) {
            Log.e(TAG, "Failed with result code" + resultCode);
            mBleHandler.onAuthenticatorAssertionResponse(
                    mClientAddress, CTAP2_ERR_OPERATION_DENIED, null, null, null, null);
            return false;
        }
        Log.e(TAG, "OK.");

        if (data.hasExtra(Fido.FIDO2_KEY_ERROR_EXTRA)) {
            Log.e(TAG, "error extra");
            AuthenticatorErrorResponse error = AuthenticatorErrorResponse.deserializeFromBytes(
                    data.getByteArrayExtra(Fido.FIDO2_KEY_ERROR_EXTRA));
            Log.i(TAG,
                    "error response: " + error.getErrorMessage() + " "
                            + String.valueOf(error.getErrorCodeAsInt()));

            // ErrorCode represents DOMErrors not CTAP status codes.
            // TODO: figure out translation of the remaining codes
            int ctap_status;
            switch (error.getErrorCode()) {
                case NOT_ALLOWED_ERR:
                    ctap_status = CTAP2_ERR_OPERATION_DENIED;
                    break;
                default:
                    ctap_status = CTAP2_ERR_OTHER;
                    break;
            }
            mBleHandler.onAuthenticatorAttestationResponse(
                    mClientAddress, CTAP2_ERR_OTHER, null, null);
            return false;
        }

        if (!data.hasExtra(Fido.FIDO2_KEY_RESPONSE_EXTRA)
                || !data.hasExtra(Fido.FIDO2_KEY_CREDENTIAL_EXTRA)) {
            Log.e(TAG, "Missing FIDO2_KEY_RESPONSE_EXTRA or FIDO2_KEY_CREDENTIAL_EXTRA");
            mBleHandler.onAuthenticatorAttestationResponse(
                    mClientAddress, CTAP2_ERR_OTHER, null, null);
            return false;
        }

        Log.e(TAG, "cred extra");
        PublicKeyCredential unusedPublicKeyCredential = PublicKeyCredential.deserializeFromBytes(
                data.getByteArrayExtra(Fido.FIDO2_KEY_CREDENTIAL_EXTRA));
        AuthenticatorAttestationResponse response =
                AuthenticatorAttestationResponse.deserializeFromBytes(
                        data.getByteArrayExtra(Fido.FIDO2_KEY_RESPONSE_EXTRA));
        mBleHandler.onAuthenticatorAttestationResponse(mClientAddress, CTAP2_OK,
                response.getClientDataJSON(), response.getAttestationObject());
        return true;
    }

    public boolean onSignResponse(int resultCode, Intent data) {
        if (resultCode != Activity.RESULT_OK || data == null) {
            Log.e(TAG, "Failed with result code" + resultCode);
            mBleHandler.onAuthenticatorAssertionResponse(
                    mClientAddress, CTAP2_ERR_OPERATION_DENIED, null, null, null, null);
            return false;
        }
        Log.e(TAG, "OK.");

        if (data.hasExtra(Fido.FIDO2_KEY_ERROR_EXTRA)) {
            Log.e(TAG, "error extra");
            AuthenticatorErrorResponse error = AuthenticatorErrorResponse.deserializeFromBytes(
                    data.getByteArrayExtra(Fido.FIDO2_KEY_ERROR_EXTRA));
            Log.i(TAG,
                    "error response: " + error.getErrorMessage() + " "
                            + String.valueOf(error.getErrorCodeAsInt()));

            // ErrorCode represents DOMErrors not CTAP status codes.
            // TODO: figure out translation of the remaining codes
            int ctap_status;
            switch (error.getErrorCode()) {
                case NOT_ALLOWED_ERR:
                    ctap_status = CTAP2_ERR_OPERATION_DENIED;
                    break;
                default:
                    ctap_status = CTAP2_ERR_OTHER;
                    break;
            }
            mBleHandler.onAuthenticatorAssertionResponse(
                    mClientAddress, ctap_status, null, null, null, null);
            return false;
        }

        if (!data.hasExtra(Fido.FIDO2_KEY_RESPONSE_EXTRA)
                || !data.hasExtra(Fido.FIDO2_KEY_CREDENTIAL_EXTRA)) {
            Log.e(TAG, "Missing FIDO2_KEY_RESPONSE_EXTRA or FIDO2_KEY_CREDENTIAL_EXTRA");
            mBleHandler.onAuthenticatorAssertionResponse(
                    mClientAddress, CTAP2_ERR_OTHER, null, null, null, null);
            return false;
        }

        Log.e(TAG, "cred extra");
        PublicKeyCredential unusedPublicKeyCredential = PublicKeyCredential.deserializeFromBytes(
                data.getByteArrayExtra(Fido.FIDO2_KEY_CREDENTIAL_EXTRA));
        AuthenticatorAssertionResponse response =
                AuthenticatorAssertionResponse.deserializeFromBytes(
                        data.getByteArrayExtra(Fido.FIDO2_KEY_RESPONSE_EXTRA));
        mBleHandler.onAuthenticatorAssertionResponse(mClientAddress, CTAP2_OK,
                response.getClientDataJSON(), response.getKeyHandle(),
                response.getAuthenticatorData(), response.getSignature());
        return true;
    }

    public void onQRCode(String value) {
        mBleHandler.onQRCode(value);
    }

    public void close() {
        mBleHandler.close();
    }
}
