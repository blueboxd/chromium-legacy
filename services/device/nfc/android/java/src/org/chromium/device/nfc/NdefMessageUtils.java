// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.nfc;

import android.net.Uri;
import android.os.Build;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.device.mojom.NdefMessage;
import org.chromium.device.mojom.NdefRecord;

import java.io.UnsupportedEncodingException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Utility class that provides conversion between Android NdefMessage and Mojo NdefMessage data
 * structures.
 */
public final class NdefMessageUtils {
    private static final String TAG = "NdefMessageUtils";
    private static final String AUTHOR_RECORD_DOMAIN = "w3.org";
    private static final String AUTHOR_RECORD_TYPE = "A";
    private static final String TEXT_MIME = "text/plain";
    private static final String JSON_MIME = "application/json";
    private static final String OCTET_STREAM_MIME = "application/octet-stream";

    public static final String RECORD_TYPE_EMPTY = "empty";
    public static final String RECORD_TYPE_TEXT = "text";
    public static final String RECORD_TYPE_URL = "url";
    public static final String RECORD_TYPE_JSON = "json";
    public static final String RECORD_TYPE_OPAQUE = "opaque";
    public static final String RECORD_TYPE_SMART_POSTER = "smart-poster";

    private static class PairOfDomainAndType {
        private String mDomain;
        private String mType;

        private PairOfDomainAndType(String domain, String type) {
            mDomain = domain;
            mType = type;
        }
    }

    /**
     * Converts mojo NdefMessage to android.nfc.NdefMessage
     */
    public static android.nfc.NdefMessage toNdefMessage(NdefMessage message)
            throws InvalidNdefMessageException {
        try {
            List<android.nfc.NdefRecord> records = new ArrayList<android.nfc.NdefRecord>();
            for (int i = 0; i < message.data.length; ++i) {
                records.add(toNdefRecord(message.data[i]));
            }
            // NdefRecord.createExternal() will internally convert both the domain and type to
            // lower-case. Details: https://github.com/w3c/web-nfc/issues/308
            records.add(android.nfc.NdefRecord.createExternal(AUTHOR_RECORD_DOMAIN,
                    AUTHOR_RECORD_TYPE, ApiCompatibilityUtils.getBytesUtf8(message.url)));
            android.nfc.NdefRecord[] ndefRecords = new android.nfc.NdefRecord[records.size()];
            records.toArray(ndefRecords);
            return new android.nfc.NdefMessage(ndefRecords);
        } catch (UnsupportedEncodingException | InvalidNdefMessageException
                | IllegalArgumentException e) {
            throw new InvalidNdefMessageException();
        }
    }

    /**
     * Converts android.nfc.NdefMessage to mojo NdefMessage
     */
    public static NdefMessage toNdefMessage(android.nfc.NdefMessage ndefMessage)
            throws UnsupportedEncodingException {
        android.nfc.NdefRecord[] ndefRecords = ndefMessage.getRecords();
        NdefMessage webNdefMessage = new NdefMessage();
        List<NdefRecord> nfcRecords = new ArrayList<NdefRecord>();

        for (int i = 0; i < ndefRecords.length; i++) {
            // NFC Forum requires that the domain and type used in an external record are treated as
            // case insensitive, so we compare while ignoring the case.
            if ((ndefRecords[i].getTnf() == android.nfc.NdefRecord.TNF_EXTERNAL_TYPE)
                    && new String(ndefRecords[i].getType(), "UTF-8")
                                    .compareToIgnoreCase(
                                            AUTHOR_RECORD_DOMAIN + ":" + AUTHOR_RECORD_TYPE)
                            == 0) {
                webNdefMessage.url = new String(ndefRecords[i].getPayload(), "UTF-8");
                continue;
            }

            NdefRecord nfcRecord = toNdefRecord(ndefRecords[i]);
            if (nfcRecord != null) nfcRecords.add(nfcRecord);
        }

        webNdefMessage.data = new NdefRecord[nfcRecords.size()];
        nfcRecords.toArray(webNdefMessage.data);
        return webNdefMessage;
    }

    /**
     * Converts mojo NdefRecord to android.nfc.NdefRecord
     * |record.data| should always be treated as "UTF-8" encoding bytes, this is guaranteed by the
     * sender (Blink).
     */
    private static android.nfc.NdefRecord toNdefRecord(NdefRecord record)
            throws InvalidNdefMessageException, IllegalArgumentException,
                   UnsupportedEncodingException {
        switch (record.recordType) {
            case RECORD_TYPE_URL:
                return android.nfc.NdefRecord.createUri(new String(record.data, "UTF-8"));
            case RECORD_TYPE_TEXT:
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                    return android.nfc.NdefRecord.createTextRecord(
                            "en-US", new String(record.data, "UTF-8"));
                } else {
                    return android.nfc.NdefRecord.createMime(TEXT_MIME, record.data);
                }
            case RECORD_TYPE_JSON:
            case RECORD_TYPE_OPAQUE:
                return android.nfc.NdefRecord.createMime(record.mediaType, record.data);
            case RECORD_TYPE_EMPTY:
                return new android.nfc.NdefRecord(
                        android.nfc.NdefRecord.TNF_EMPTY, null, null, null);
            case RECORD_TYPE_SMART_POSTER:
                // TODO(https://crbug.com/520391): Support 'smart-poster' type records.
                throw new InvalidNdefMessageException();
        }
        PairOfDomainAndType pair = parseDomainAndType(record.recordType);
        if (pair != null) {
            return android.nfc.NdefRecord.createExternal(pair.mDomain, pair.mType, record.data);
        }

        throw new InvalidNdefMessageException();
    }

    /**
     * Converts android.nfc.NdefRecord to mojo NdefRecord
     */
    private static NdefRecord toNdefRecord(android.nfc.NdefRecord ndefRecord)
            throws UnsupportedEncodingException {
        NdefRecord record = null;
        switch (ndefRecord.getTnf()) {
            case android.nfc.NdefRecord.TNF_EMPTY:
                record = createEmptyRecord();
                break;
            case android.nfc.NdefRecord.TNF_MIME_MEDIA:
                record = createMIMERecord(
                        new String(ndefRecord.getType(), "UTF-8"), ndefRecord.getPayload());
                break;
            case android.nfc.NdefRecord.TNF_ABSOLUTE_URI:
                record = createURLRecord(ndefRecord.toUri());
                break;
            case android.nfc.NdefRecord.TNF_WELL_KNOWN:
                record = createWellKnownRecord(ndefRecord);
                break;
            case android.nfc.NdefRecord.TNF_UNKNOWN:
                record = createUnKnownRecord(ndefRecord.getPayload());
                break;
            case android.nfc.NdefRecord.TNF_EXTERNAL_TYPE:
                record = createExternalTypeRecord(
                        new String(ndefRecord.getType(), "UTF-8"), ndefRecord.getPayload());
                break;
        }
        record.id = new String(ndefRecord.getId(), "UTF-8");
        return record;
    }

    /**
     * Constructs empty NdefMessage
     */
    public static android.nfc.NdefMessage emptyNdefMessage() {
        return new android.nfc.NdefMessage(
                new android.nfc.NdefRecord(android.nfc.NdefRecord.TNF_EMPTY, null, null, null));
    }

    /**
     * Constructs empty NdefRecord
     */
    private static NdefRecord createEmptyRecord() {
        NdefRecord nfcRecord = new NdefRecord();
        nfcRecord.recordType = RECORD_TYPE_EMPTY;
        nfcRecord.mediaType = "";
        nfcRecord.data = new byte[0];
        return nfcRecord;
    }

    /**
     * Constructs URL NdefRecord
     */
    private static NdefRecord createURLRecord(Uri uri) {
        if (uri == null) return null;
        NdefRecord nfcRecord = new NdefRecord();
        nfcRecord.recordType = RECORD_TYPE_URL;
        nfcRecord.mediaType = TEXT_MIME;
        nfcRecord.data = ApiCompatibilityUtils.getBytesUtf8(uri.toString());
        return nfcRecord;
    }

    /**
     * Constructs MIME or JSON NdefRecord
     */
    private static NdefRecord createMIMERecord(String mediaType, byte[] payload) {
        NdefRecord nfcRecord = new NdefRecord();
        if (mediaType.equals(JSON_MIME)) {
            nfcRecord.recordType = RECORD_TYPE_JSON;
        } else {
            nfcRecord.recordType = RECORD_TYPE_OPAQUE;
        }
        nfcRecord.mediaType = mediaType;
        nfcRecord.data = payload;
        return nfcRecord;
    }

    /**
     * Constructs TEXT NdefRecord
     */
    private static NdefRecord createTextRecord(byte[] text) {
        // Check that text byte array is not empty.
        if (text.length == 0) {
            return null;
        }

        NdefRecord nfcRecord = new NdefRecord();
        nfcRecord.recordType = RECORD_TYPE_TEXT;
        nfcRecord.mediaType = TEXT_MIME;
        // According to NFCForum-TS-RTD_Text_1.0 specification, section 3.2.1 Syntax.
        // First byte of the payload is status byte, defined in Table 3: Status Byte Encodings.
        // 0-5: lang code length
        // 6  : must be zero
        // 8  : 0 - text is in UTF-8 encoding, 1 - text is in UTF-16 encoding.
        int langCodeLength = (text[0] & (byte) 0x3F);
        int textBodyStartPos = langCodeLength + 1;
        if (textBodyStartPos > text.length) {
            return null;
        }
        nfcRecord.data = Arrays.copyOfRange(text, textBodyStartPos, text.length);
        return nfcRecord;
    }

    /**
     * Constructs well known type (TEXT or URI) NdefRecord
     */
    private static NdefRecord createWellKnownRecord(android.nfc.NdefRecord record) {
        if (Arrays.equals(record.getType(), android.nfc.NdefRecord.RTD_URI)) {
            return createURLRecord(record.toUri());
        }

        if (Arrays.equals(record.getType(), android.nfc.NdefRecord.RTD_TEXT)) {
            return createTextRecord(record.getPayload());
        }

        // TODO(https://crbug.com/520391): Support RTD_SMART_POSTER type records.

        return null;
    }

    /**
     * Constructs unknown known type NdefRecord
     */
    private static NdefRecord createUnKnownRecord(byte[] payload) {
        NdefRecord nfcRecord = new NdefRecord();
        nfcRecord.recordType = RECORD_TYPE_OPAQUE;
        nfcRecord.mediaType = OCTET_STREAM_MIME;
        nfcRecord.data = payload;
        return nfcRecord;
    }

    /**
     * Constructs External type NdefRecord
     */
    private static NdefRecord createExternalTypeRecord(String customType, byte[] payload) {
        NdefRecord nfcRecord = new NdefRecord();
        nfcRecord.recordType = customType;
        nfcRecord.mediaType = OCTET_STREAM_MIME;
        nfcRecord.data = payload;
        return nfcRecord;
    }

    /**
     * Parses the input custom type to get its domain and type.
     * e.g. returns a pair ('w3.org', 'xyz') for the input 'w3.org:xyz'.
     * Returns null for invalid input.
     * https://w3c.github.io/web-nfc/#the-ndefrecordtype-string
     *
     * TODO(https://crbug.com/520391): Refine the validation algorithm here accordingly once there
     * is a conclusion on some case-sensitive things at https://github.com/w3c/web-nfc/issues/331.
     */
    private static PairOfDomainAndType parseDomainAndType(String customType) {
        int colonIndex = customType.indexOf(':');
        if (colonIndex == -1) return null;

        // TODO(ThisCL): verify |domain| is a valid FQDN, asking help at
        // https://groups.google.com/a/chromium.org/forum/#!topic/chromium-dev/QN2mHt_WgHo.
        String domain = customType.substring(0, colonIndex);
        if (domain.isEmpty()) return null;

        String type = customType.substring(colonIndex + 1);
        if (type.isEmpty()) return null;
        if (!type.matches("[a-zA-Z0-9()+,\\-:=@;$_!*'.]+")) return null;

        return new PairOfDomainAndType(domain, type);
    }
}
