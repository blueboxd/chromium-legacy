// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.nfc;

import android.nfc.Tag;
import android.nfc.tech.IsoDep;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;

import java.util.Arrays;

/**
 * Utility class that provides functions to tell whether tag should be accessible using Web NFC.
 */
public class NfcBlocklist {
    private static final String TAG = "NfcBlocklist";

    private static Boolean sIsTagBlockedForTesting;

    // https://developers.yubico.com/PIV/Introduction/Yubico_extensions.html
    private static final byte[] YUBIKEY_5_SERIES_HISTORICAL_BYTES = new byte[] {(byte) 0x80, 0x73,
            (byte) 0xc0, 0x21, (byte) 0xc0, 0x57, 0x59, 0x75, 0x62, 0x69, 0x4b, 0x65, 0x79};
    private static final byte[] YUBIKEY_NEO_HISTORICAL_BYTES = new byte[] {
            (byte) 0x59, 0x75, 0x62, 0x69, 0x6b, 0x65, 0x79, 0x4e, 0x45, 0x4f, 0x72, 0x33};

    /**
     * Returns true if tag is blocked, otherwise false. A tag is blocked if it is a YubiKey.
     *
     * @param tag @see android.nfc.Tag
     * @return true if tag is blocked, otherwise false.
     */
    public static boolean isTagBlocked(Tag tag) {
        if (sIsTagBlockedForTesting != null) {
            return sIsTagBlockedForTesting;
        }

        if (tag == null) return false;

        IsoDep iso = IsoDep.get(tag);
        if (iso != null) {
            byte[] historicalBytes = iso.getHistoricalBytes();
            if (Arrays.equals(historicalBytes, YUBIKEY_5_SERIES_HISTORICAL_BYTES)
                    || Arrays.equals(historicalBytes, YUBIKEY_NEO_HISTORICAL_BYTES)) {
                Log.w(TAG, "YubiKey detected. Access is blocked.");
                return true;
            }
        }

        return false;
    }

    /** Block/unblock NFC tag access for testing use only. */
    @VisibleForTesting
    public static void setIsTagBlockedForTesting(Boolean blocked) {
        sIsTagBlockedForTesting = blocked;
    }
}
