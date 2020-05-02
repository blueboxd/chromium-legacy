// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauth.authenticator;

import android.annotation.TargetApi;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothGatt;
import android.bluetooth.BluetoothGattCharacteristic;
import android.bluetooth.BluetoothGattDescriptor;
import android.bluetooth.BluetoothGattServer;
import android.bluetooth.BluetoothGattServerCallback;
import android.bluetooth.BluetoothGattService;
import android.bluetooth.BluetoothManager;
import android.bluetooth.BluetoothProfile;
import android.bluetooth.le.AdvertiseCallback;
import android.bluetooth.le.AdvertiseData;
import android.bluetooth.le.AdvertiseSettings;
import android.bluetooth.le.BluetoothLeAdvertiser;
import android.content.Context;
import android.content.SharedPreferences;
import android.os.ParcelUuid;
import android.util.Base64;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.SingleThreadTaskRunner;
import org.chromium.base.task.TaskTraits;

import java.io.Closeable;
import java.nio.ByteBuffer;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.UUID;

/**
 * Interfaces the Android BLE APIs with C++ code in cablev2_authenticator_android.cc that implements
 * the caBLE v2 protocol. There is, at most, a single instance of this class in an address space.
 */
@TargetApi(21)
class BLEHandler extends BluetoothGattServerCallback implements Closeable {
    // TODO: remove @TargetApi once 21 is the minimum, Clank-wide.

    private static final String TAG = "CableBLEHandler";
    // These UUIDs are taken from the FIDO spec[1], save for the caBLE UUID
    // which is allocated to Google. See
    // https://fidoalliance.org/specs/fido-v2.0-ps-20190130/fido-client-to-authenticator-protocol-v2.0-ps-20190130.html#ble
    private static final String CABLE_UUID = "0000fde2-0000-1000-8000-00805f9b34fb";
    private static final String CONTROL_POINT_UUID = "f1d0fff1-deaa-ecee-b42f-c9ba7ed623bb";
    private static final String CONTROL_POINT_LENGTH_UUID = "f1d0fff3-deaa-ecee-b42f-c9ba7ed623bb";
    private static final String SERVICE_REVISION_UUID = "00002a28-0000-1000-8000-00805f9b34fb";
    private static final String STATUS_UUID = "f1d0fff2-deaa-ecee-b42f-c9ba7ed623bb";
    private static final String CLIENT_CHARACTERISTIC_DESCRIPTOR_UUID =
            "00002902-0000-1000-8000-00805f9b34fb";

    // See
    // https://www.bluetooth.com/wp-content/uploads/Sitecore-Media-Library/Gatt/Xml/Descriptors/org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml
    private static final int GATT_CCCD_NOTIFICATIONS_DISABLED = 0;
    private static final int GATT_CCCD_NOTIFICATIONS_ENABLED = 1;

    // TODO: delete, along with the helper function that uses it, once we no longer
    // need to dump debugging information from this code.
    private static final char[] HEX_CHARS = "0123456789ABCDEF".toCharArray();

    // The filename and key name of the SharedPreferences value that contains
    // the base64-encoded state from the native code.
    private static final String STATE_FILE_NAME = "cablev2_authenticator";
    private static final String STATE_VALUE_NAME = "keys";

    // The (G)ATT op-code and attribute handle take three bytes.
    private static final int GATT_MTU_OVERHEAD = 3;
    // If no MTU is negotiated, the GATT default is just 23 bytes. Subtract three bytes of GATT
    // overhead.
    private static final int DEFAULT_MTU = 23 - GATT_MTU_OVERHEAD;

    /**
     * The pending fragments to send to each client. If present, the value is
     * never an empty array.
     */
    private final HashMap<Long, byte[][]> mPendingFragments;
    private final HashMap<Long, Integer> mKnownMtus;
    private final Context mContext;
    private final BluetoothManager mManager;
    private final CableAuthenticator mAuthenticator;

    /**
     * Android's BLE callbacks may happen on arbitrary threads. In order to
     * avoid dealing with concurrency here, and in the C++ code, BLE callbacks
     * are bounced to a specified thread which is accessed via this task runner.
     */
    private SingleThreadTaskRunner mTaskRunner;

    private AdvertiseCallback mCallback;
    private BluetoothGattServer mServer;
    private BluetoothGattDescriptor mCccd;
    private BluetoothGattCharacteristic mStatusChar;
    private BluetoothDevice mConnectedDevice;

    BLEHandler(CableAuthenticator authenticator) {
        mPendingFragments = new HashMap<Long, byte[][]>();
        mKnownMtus = new HashMap<Long, Integer>();
        mContext = ContextUtils.getApplicationContext();
        mManager = (BluetoothManager) mContext.getSystemService(Context.BLUETOOTH_SERVICE);
        mAuthenticator = authenticator;
    }

    /**
     * Exports the GATT service. Does not start advertising immediately, but calls the native
     * start method, which may do so.
     *
     * @return true if successful and false on error.
     */
    public boolean start() {
        SharedPreferences prefs =
                mContext.getSharedPreferences(STATE_FILE_NAME, Context.MODE_PRIVATE);
        byte[] stateBytes = null;
        try {
            stateBytes = Base64.decode(prefs.getString(STATE_VALUE_NAME, ""), Base64.DEFAULT);
            if (stateBytes.length == 0) {
                stateBytes = null;
            }
        } catch (IllegalArgumentException e) {
            Log.w(TAG, "Ignoring corrupt state");
        }

        BluetoothGattService cableService = new BluetoothGattService(
                UUID.fromString(CABLE_UUID), BluetoothGattService.SERVICE_TYPE_PRIMARY);
        cableService.addCharacteristic(new BluetoothGattCharacteristic(
                UUID.fromString(CONTROL_POINT_UUID), BluetoothGattCharacteristic.PROPERTY_WRITE,
                BluetoothGattCharacteristic.PERMISSION_WRITE));
        cableService.addCharacteristic(
                new BluetoothGattCharacteristic(UUID.fromString(CONTROL_POINT_LENGTH_UUID),
                        BluetoothGattCharacteristic.PROPERTY_READ,
                        BluetoothGattCharacteristic.PERMISSION_READ));
        mStatusChar = new BluetoothGattCharacteristic(UUID.fromString(STATUS_UUID),
                BluetoothGattCharacteristic.PROPERTY_READ
                        | BluetoothGattCharacteristic.PROPERTY_NOTIFY,
                BluetoothGattCharacteristic.PERMISSION_READ);
        mCccd = new BluetoothGattDescriptor(UUID.fromString(CLIENT_CHARACTERISTIC_DESCRIPTOR_UUID),
                BluetoothGattDescriptor.PERMISSION_WRITE | BluetoothGattDescriptor.PERMISSION_READ);
        mStatusChar.addDescriptor(mCccd);
        cableService.addCharacteristic(mStatusChar);
        cableService.addCharacteristic(
                new BluetoothGattCharacteristic(UUID.fromString(SERVICE_REVISION_UUID),
                        BluetoothGattCharacteristic.PROPERTY_READ
                                | BluetoothGattCharacteristic.PROPERTY_WRITE,
                        BluetoothGattCharacteristic.PERMISSION_READ
                                | BluetoothGattCharacteristic.PERMISSION_WRITE));

        mServer = mManager.openGattServer(mContext, this);
        if (!mServer.addService(cableService)) {
            Log.i(TAG, "addService failed");
            return false;
        }

        // Android does not document on which thread BLE callbacks will occur
        // but, in practice they seem to happen on the Binder thread. In order
        // to serialise callbacks, all processing (including native processing)
        // happens on a dedicated thread. This is a SingleThreadTaskRunner so
        // that all native callback happen in the same thread. That avoids
        // worrying about JNIEnv objects and references being incorrectly used
        // across threads.
        assert mTaskRunner == null;
        // TODO: in practice, this sadly appears to return the UI thread,
        // despite requesting |BEST_EFFORT|.
        mTaskRunner = PostTask.createSingleThreadTaskRunner(TaskTraits.BEST_EFFORT);
        // Local variables passed into a lambda must be final.
        final byte[] state = stateBytes;
        mTaskRunner.postTask(() -> BLEHandlerJni.get().start(this, state));

        return true;
    }

    @Override
    public void onMtuChanged(BluetoothDevice device, int mtu) {
        Log.i(TAG, "onMtuChanged(): device=%s, mtu=%s", device.getAddress(), mtu);
        Long client = addressToLong(device.getAddress());
        // At least six bytes is required: three bytes of GATT overhead and
        // three bytes of CTAP2 framing. If the requested MTU is less than
        // this, try six bytes anyway. The operations will probably fail but
        // there's nothing we can do about it.
        if (mtu < 6) {
            mtu = 6;
        }

        int clientMTU = mtu - GATT_MTU_OVERHEAD;
        mKnownMtus.put(client, clientMTU);
    }

    @Override
    public void onCharacteristicReadRequest(BluetoothDevice device, int requestId, int offset,
            BluetoothGattCharacteristic characteristic) {
        Log.i(TAG,
                "onCharacteristicReadRequest(): device=%s, requestId=%s, "
                        + "offset=%s, characteristic=%s",
                device.getAddress(), requestId, offset, characteristic.getUuid().toString());
        if (offset != 0) {
            Log.i(TAG, "onCharacteristicReadRequest: non-zero offset request");
            mServer.sendResponse(device, requestId, BluetoothGatt.GATT_FAILURE, 0, null);
            return;
        }

        Long client = addressToLong(device.getAddress());
        switch (characteristic.getUuid().toString()) {
            case CONTROL_POINT_LENGTH_UUID:
                mTaskRunner.postTask(() -> {
                    Integer mtu = mKnownMtus.get(client);
                    if (mtu == null) {
                        mtu = DEFAULT_MTU;
                    }
                    byte[] mtuBytes = {(byte) (mtu >> 8), (byte) (mtu & 0xff)};
                    mServer.sendResponse(
                            device, requestId, BluetoothGatt.GATT_SUCCESS, 0, mtuBytes);
                });
                break;

            default:
                // The control-point length is the only characteristic that is
                // read because the status sends data purely via notifications.
                mServer.sendResponse(device, requestId, BluetoothGatt.GATT_FAILURE, 0, null);
        }
    }

    @Override
    public void onCharacteristicWriteRequest(BluetoothDevice device, int requestId,
            BluetoothGattCharacteristic characteristic, boolean preparedWrite,
            boolean responseNeeded, int offset, byte[] value) {
        Log.i(TAG,
                "onCharacteristicWriteRequest(): device=%s, requestId=%s, "
                        + "characteristic=%s, preparedWrite=%s, "
                        + "responseNeeded=%s, offset=%d, value=%s",
                device.getAddress(), requestId, characteristic.getUuid().toString(), preparedWrite,
                responseNeeded, offset, hex(value));

        // fidoControlPoint is the only characteristic that is written to as caBLE clients don't set
        // the service revision to save a round-trip. The first client to write to this
        // characteristic "connects" to the authenticator. Writes from all other clients are
        // rejected.
        // TODO: signal an error if a connected client disconnects before any other result is
        // signaled to UI?

        if (value == null || offset != 0
                || !characteristic.getUuid().toString().equals(CONTROL_POINT_UUID)
                || (mConnectedDevice != null && !mConnectedDevice.equals(device))) {
            if (responseNeeded) {
                mServer.sendResponse(device, requestId, BluetoothGatt.GATT_FAILURE, 0, null);
            }
            return;
        }

        if (mConnectedDevice == null) {
            mConnectedDevice = device;
            mAuthenticator.notifyAuthenticatorConnected();
        }

        Long client = addressToLong(device.getAddress());
        // The buffer containing the data is owned by the BLE stack and
        // might be reused once this callback returns. Thus a copy is
        // made for the handler thread.
        byte[] valueCopy = Arrays.copyOf(value, value.length);
        mTaskRunner.postTask(() -> {
            Integer mtu = mKnownMtus.get(client);
            if (mtu == null) {
                mtu = DEFAULT_MTU;
            }
            byte[][] responseFragments =
                    BLEHandlerJni.get().write(client.longValue(), mtu, valueCopy);
            if (responseNeeded) {
                int status = responseFragments == null ? BluetoothGatt.GATT_FAILURE
                                                       : BluetoothGatt.GATT_SUCCESS;
                mServer.sendResponse(device, requestId, status, 0, null);
            }
            if (responseFragments != null && responseFragments.length > 0) {
                sendNotification(device, responseFragments);
            }
        });
    }

    /**
     * Triggers a notification on the fidoStatus characteristic to the given device.
     */
    public void sendNotification(BluetoothDevice device, byte[][] fragments) {
        Log.i(TAG, "onCharacteristicWriteRequest sending " + hex(fragments[0]));
        Long client = addressToLong(device.getAddress());
        assert !mPendingFragments.containsKey(client);

        mStatusChar.setValue(fragments[0]);
        mServer.notifyCharacteristicChanged(device, mStatusChar, /*confirm=*/false);
        if (fragments.length > 1) {
            mPendingFragments.put(client, Arrays.copyOfRange(fragments, 1, fragments.length));
        }
    }

    /**
     * Like sendNotification(BluetoothDevice, byte[][]), but for a client ID passed from native
     * code.
     */
    @CalledByNative
    public void sendNotification(long client, byte[][] fragments) {
        String addr = longToAddress(client);
        Log.i(TAG, "sendNotification to " + addr);
        List<BluetoothDevice> devices = mManager.getConnectedDevices(BluetoothProfile.GATT);
        BluetoothDevice device = null;
        for (int i = 0; i < devices.size(); i++) {
            if (addr.equals(devices.get(i).getAddress())) {
                device = devices.get(i);
                break;
            }
        }
        if (device == null) {
            Log.i(TAG, "can't find connected device " + addr);
            return;
        }
        sendNotification(device, fragments);
    }

    /**
     * Called by the BLE stack when transmission of a notification has completed.
     * If that transmission was successful, the next buffered fragment is sent the client.
     */
    @Override
    public void onNotificationSent(BluetoothDevice device, int status) {
        Long client = addressToLong(device.getAddress());

        mTaskRunner.postTask(() -> {
            if (status != BluetoothGatt.GATT_SUCCESS) {
                mPendingFragments.remove(client);
                return;
            }

            byte[][] remainingFragments = mPendingFragments.get(client);
            if (remainingFragments == null) {
                return;
            }

            Log.i(TAG, "onNotificationSent sending " + hex(remainingFragments[0]));
            mStatusChar.setValue(remainingFragments[0]);
            mServer.notifyCharacteristicChanged(device, mStatusChar, /*confirm=*/false);

            if (remainingFragments.length > 1) {
                mPendingFragments.put(client,
                        Arrays.copyOfRange(remainingFragments, 1, remainingFragments.length));
            } else {
                mPendingFragments.remove(client);
            }
        });
    }

    private static String hex(byte[] bytes) {
        if (bytes == null) {
            return "(null)";
        }

        char[] ret = new char[bytes.length * 2];
        for (int j = 0; j < bytes.length; j++) {
            int v = bytes[j] & 0xFF;
            ret[j * 2] = HEX_CHARS[v >>> 4];
            ret[j * 2 + 1] = HEX_CHARS[v & 0x0F];
        }
        return new String(ret);
    }

    // addressToLong converts a BLE address into a Long, which is smaller and
    // easier to deal with.
    private static Long addressToLong(String address) {
        // See
        // https://developer.android.com/reference/android/bluetooth/BluetoothDevice.html#getAddress()
        // for the format of the address string.
        return Long.valueOf(address.replace(":", ""), 16);
    }

    // longToAddress is the inverse operation to addressToLong.
    private static String longToAddress(long addr) {
        char[] result = new char[17];
        for (int i = 0; i < 6; i++) {
            int v = (int) ((addr >>> ((5 - i) * 8)) & 0xFF);
            result[i * 3] = HEX_CHARS[v >>> 4];
            result[i * 3 + 1] = HEX_CHARS[v & 0x0F];
            if (i < 5) {
                result[i * 3 + 2] = ':';
            }
        }
        return new String(result);
    }

    @Override
    public void onDescriptorWriteRequest(BluetoothDevice device, int requestId,
            BluetoothGattDescriptor descriptor, boolean preparedWrite, boolean responseNeeded,
            int offset, byte[] value) {
        // There is only a single descriptor: the CCCD used for notifications.
        // It is how GATT clients subscribe to notifications and the protocol
        // is part of the core GATT specification.
        if (!descriptor.getUuid().equals(mCccd.getUuid())) {
            return;
        }
        Log.i(TAG, "onDescriptorWriteRequest: " + hex(value) + " " + responseNeeded);

        if (offset != 0 || value.length != 2) {
            if (responseNeeded) {
                mServer.sendResponse(device, requestId, BluetoothGatt.GATT_FAILURE, 0, null);
            }
            return;
        }

        int request = (int) value[0] + (int) value[1] * 256;
        Long client = addressToLong(device.getAddress());
        mTaskRunner.postTask(() -> {
            int status = BluetoothGatt.GATT_FAILURE;
            switch (request) {
                case GATT_CCCD_NOTIFICATIONS_DISABLED:
                    mPendingFragments.remove(client);
                    status = BluetoothGatt.GATT_SUCCESS;
                    break;

                case GATT_CCCD_NOTIFICATIONS_ENABLED:
                    status = BluetoothGatt.GATT_SUCCESS;
                    break;
            }

            if (responseNeeded) {
                mServer.sendResponse(device, requestId, status, 0, null);
            }
        });
    }

    /**
     * Called to indicate that a QR code was scanned by the user.
     *
     * @param value contents of the QR code, which will be a valid caBLE
     *              URL, i.e. "fido://c1/"...
     */
    public void onQRCode(String value) {
        mTaskRunner.postTask(() -> { BLEHandlerJni.get().onQRScanned(value); });
    }

    private void maybeStopAdvertising() {
        if (mCallback == null) {
            return;
        }

        BluetoothLeAdvertiser advertiser =
                BluetoothAdapter.getDefaultAdapter().getBluetoothLeAdvertiser();
        Log.i(TAG, "stopping advertising");
        advertiser.stopAdvertising(mCallback);
        mCallback = null;
    }

    @Override
    public void close() {
        mTaskRunner.postTask(() -> {
            maybeStopAdvertising();
            BLEHandlerJni.get().stop();
        });
    }

    /**
     * Called by C++ code to start advertising a given UUID, which is passed
     * as 16 bytes.
     */
    @CalledByNative
    public void sendBLEAdvert(byte[] dataUuidBytes) {
        assert mTaskRunner.belongsToCurrentThread();
        Log.i(TAG, "sendBLEAdvert " + hex(dataUuidBytes));

        maybeStopAdvertising();

        BluetoothLeAdvertiser advertiser =
                BluetoothAdapter.getDefaultAdapter().getBluetoothLeAdvertiser();
        mCallback = new AdvertiseCallback() {
            @Override
            public void onStartFailure(int errorCode) {
                Log.i(TAG, "advertising failure" + errorCode);
            }

            @Override
            public void onStartSuccess(AdvertiseSettings settingsInEffect) {
                Log.i(TAG, "advertising success");
            }
        };

        AdvertiseSettings settings =
                (new AdvertiseSettings.Builder())
                        .setAdvertiseMode(AdvertiseSettings.ADVERTISE_MODE_LOW_LATENCY)
                        .setConnectable(true)
                        .setTxPowerLevel(AdvertiseSettings.ADVERTISE_TX_POWER_MEDIUM)
                        .build();
        ParcelUuid fidoUuid = new ParcelUuid(UUID.fromString(CABLE_UUID));

        ByteBuffer bb = ByteBuffer.wrap(dataUuidBytes);
        long high = bb.getLong();
        long low = bb.getLong();
        UUID dataUuid = new UUID(high, low);

        AdvertiseData data = (new AdvertiseData.Builder())
                                     .addServiceUuid(fidoUuid)
                                     .addServiceUuid(new ParcelUuid(dataUuid))
                                     .setIncludeDeviceName(false)
                                     .setIncludeTxPowerLevel(false)
                                     .build();

        advertiser.startAdvertising(settings, data, mCallback);
    }

    /**
     * Called by native code to store a new state blob.
     */
    @CalledByNative
    public void setState(byte[] newState) {
        assert mTaskRunner.belongsToCurrentThread();

        SharedPreferences prefs =
                mContext.getSharedPreferences(STATE_FILE_NAME, Context.MODE_PRIVATE);
        Log.i(TAG, "Writing updated state");
        prefs.edit()
                .putString(STATE_VALUE_NAME,
                        Base64.encodeToString(newState, Base64.NO_WRAP | Base64.NO_PADDING))
                .apply();
    }

    /**
     * Called by native code to process a makeCredential request.
     */
    @CalledByNative
    void makeCredential(long client, String origin, String rpId, byte[] challenge, byte[] userId,
            int[] algorithms, byte[][] excludedCredentialIds, boolean residentKeyRequired) {
        mAuthenticator.makeCredential(client, origin, rpId, challenge, userId, algorithms,
                excludedCredentialIds, residentKeyRequired);
    }

    /**
     * Called by native code to process a getAssertion request.
     */
    @CalledByNative
    void getAssertion(long client, String origin, String rpId, byte[] challenge,
            byte[][] allowedCredentialIds) {
        mAuthenticator.getAssertion(client, origin, rpId, challenge, allowedCredentialIds);
    }

    /**
     * Called by CableAuthenticator to notify native code of a response to a makeCredential request.
     */
    public void onAuthenticatorAttestationResponse(
            long client, int ctapStatus, byte[] clientDataJSON, byte[] attestationObject) {
        mTaskRunner.postTask(
                ()
                        -> BLEHandlerJni.get().onAuthenticatorAttestationResponse(
                                client, ctapStatus, clientDataJSON, attestationObject));
    }

    /**
     * Called by CableAuthenticator to notify native code of a response to a getAssertion request.
     */
    public void onAuthenticatorAssertionResponse(long client, int ctapStatus, byte[] clientDataJSON,
            byte[] credentialID, byte[] authenticatorData, byte[] signature) {
        mTaskRunner.postTask(
                ()
                        -> BLEHandlerJni.get().onAuthenticatorAssertionResponse(client, ctapStatus,
                                clientDataJSON, credentialID, authenticatorData, signature));
    }

    @NativeMethods
    interface Natives {
        /**
         * Called to alert the C++ code to a new instance. The C++ code calls back into this object
         * to send data.
         */
        void start(BLEHandler bleHandler, byte[] stateBytes);
        void stop();
        /**
         * Called when a QR code has been scanned.
         *
         * @param value contents of the QR code, which will be a valid caBLE
         *              URL, i.e. "fido://c1/"...
         */
        void onQRScanned(String value);
        /**
         * Called to alert the C++ code that a GATT client wrote data.
         */
        byte[][] write(long client, int mtu, byte[] data);
        /**
         * Called to alert native code of a response to a makeCredential request.
         */
        void onAuthenticatorAttestationResponse(
                long client, int ctapStatus, byte[] clientDataJSON, byte[] attestationObject);
        /**
         * Called to alert native code of a response to a getAssertion request.
         */
        void onAuthenticatorAssertionResponse(long client, int ctapStatus, byte[] clientDataJSON,
                byte[] credentialID, byte[] authenticatorData, byte[] signature);
    }
}
