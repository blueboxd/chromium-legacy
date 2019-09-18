// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.nfc;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.ArgumentMatchers.nullable;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.Context;
import android.content.pm.PackageManager;
import android.nfc.FormatException;
import android.nfc.NfcAdapter;
import android.nfc.NfcAdapter.ReaderCallback;
import android.nfc.NfcManager;
import android.os.Bundle;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.device.mojom.NdefMessage;
import org.chromium.device.mojom.NdefRecord;
import org.chromium.device.mojom.NdefRecordTypeFilter;
import org.chromium.device.mojom.Nfc.CancelAllWatchesResponse;
import org.chromium.device.mojom.Nfc.CancelPushResponse;
import org.chromium.device.mojom.Nfc.CancelWatchResponse;
import org.chromium.device.mojom.Nfc.PushResponse;
import org.chromium.device.mojom.Nfc.WatchResponse;
import org.chromium.device.mojom.NfcClient;
import org.chromium.device.mojom.NfcError;
import org.chromium.device.mojom.NfcErrorType;
import org.chromium.device.mojom.NfcPushOptions;
import org.chromium.device.mojom.NfcPushTarget;
import org.chromium.device.mojom.NfcScanOptions;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.io.IOException;
import java.io.UnsupportedEncodingException;

/**
 * Unit tests for NfcImpl and NdefMessageUtils classes.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NFCTest {
    private TestNfcDelegate mDelegate;
    private int mNextWatchId;
    @Mock
    private Context mContext;
    @Mock
    private NfcManager mNfcManager;
    @Mock
    private NfcAdapter mNfcAdapter;
    @Mock
    private Activity mActivity;
    @Mock
    private NfcClient mNfcClient;
    @Mock
    private NfcTagHandler mNfcTagHandler;
    @Captor
    private ArgumentCaptor<NfcError> mErrorCaptor;
    @Captor
    private ArgumentCaptor<int[]> mOnWatchCallbackCaptor;

    // Constants used for the test.
    private static final String TEST_TEXT = "test";
    private static final String TEST_URL = "https://google.com";
    private static final String TEST_JSON = "{\"key1\":\"value1\",\"key2\":2}";
    private static final String AUTHOR_RECORD_DOMAIN = "w3.org";
    private static final String AUTHOR_RECORD_TYPE = "A";
    private static final String TEXT_MIME = "text/plain";
    private static final String JSON_MIME = "application/json";
    private static final String OCTET_STREAM_MIME = "application/octet-stream";
    private static final String CHARSET_UTF8 = ";charset=UTF-8";
    private static final String CHARSET_UTF16 = ";charset=UTF-16";
    private static final String LANG_EN_US = "en-US";

    /**
     * Class that is used test NfcImpl implementation
     */
    private static class TestNfcImpl extends NfcImpl {
        public TestNfcImpl(Context context, NfcDelegate delegate) {
            super(0, delegate);
        }

        public void processPendingOperationsForTesting(NfcTagHandler handler) {
            super.processPendingOperations(handler);
        }
    }

    private static class TestNfcDelegate implements NfcDelegate {
        Activity mActivity;
        Callback<Activity> mCallback;

        public TestNfcDelegate(Activity activity) {
            mActivity = activity;
        }
        @Override
        public void trackActivityForHost(int hostId, Callback<Activity> callback) {
            mCallback = callback;
        }

        public void invokeCallback() {
            mCallback.onResult(mActivity);
        }

        @Override
        public void stopTrackingActivityForHost(int hostId) {}
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mDelegate = new TestNfcDelegate(mActivity);
        doReturn(mNfcManager).when(mContext).getSystemService(Context.NFC_SERVICE);
        doReturn(mNfcAdapter).when(mNfcManager).getDefaultAdapter();
        doReturn(true).when(mNfcAdapter).isEnabled();
        doReturn(PackageManager.PERMISSION_GRANTED)
                .when(mContext)
                .checkPermission(anyString(), anyInt(), anyInt());
        doNothing()
                .when(mNfcAdapter)
                .enableReaderMode(any(Activity.class), any(ReaderCallback.class), anyInt(),
                        (Bundle) isNull());
        doNothing().when(mNfcAdapter).disableReaderMode(any(Activity.class));
        // Tag handler overrides used to mock connected tag.
        doReturn(true).when(mNfcTagHandler).isConnected();
        doReturn(false).when(mNfcTagHandler).isTagOutOfRange();
        try {
            doNothing().when(mNfcTagHandler).connect();
            doNothing().when(mNfcTagHandler).write(any(android.nfc.NdefMessage.class));
            doReturn(createUrlWebNFCNdefMessage(TEST_URL)).when(mNfcTagHandler).read();
            doNothing().when(mNfcTagHandler).close();
        } catch (IOException | FormatException e) {
        }
        ContextUtils.initApplicationContextForTests(mContext);
    }

    /**
     * Test that error with type NOT_SUPPORTED is returned if NFC is not supported.
     */
    @Test
    @Feature({"NFCTest"})
    public void testNFCNotSupported() {
        doReturn(null).when(mNfcManager).getDefaultAdapter();
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        mDelegate.invokeCallback();
        CancelAllWatchesResponse mockCallback = mock(CancelAllWatchesResponse.class);
        nfc.cancelAllWatches(mockCallback);
        verify(mockCallback).call(mErrorCaptor.capture());
        assertEquals(NfcErrorType.NOT_SUPPORTED, mErrorCaptor.getValue().errorType);
    }

    /**
     * Test that error with type SECURITY is returned if permission to use NFC is not granted.
     */
    @Test
    @Feature({"NFCTest"})
    public void testNFCIsNotPermitted() {
        doReturn(PackageManager.PERMISSION_DENIED)
                .when(mContext)
                .checkPermission(anyString(), anyInt(), anyInt());
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        CancelAllWatchesResponse mockCallback = mock(CancelAllWatchesResponse.class);
        nfc.cancelAllWatches(mockCallback);
        verify(mockCallback).call(mErrorCaptor.capture());
        assertEquals(NfcErrorType.NOT_ALLOWED, mErrorCaptor.getValue().errorType);
    }

    /**
     * Test that method can be invoked successfully if NFC is supported and adapter is enabled.
     */
    @Test
    @Feature({"NFCTest"})
    public void testNFCIsSupported() {
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        mDelegate.invokeCallback();
        WatchResponse mockCallback = mock(WatchResponse.class);
        nfc.watch(createNfcScanOptions(), mNextWatchId, mockCallback);
        verify(mockCallback).call(mErrorCaptor.capture());
        assertNull(mErrorCaptor.getValue());
    }

    /**
     * Test conversion from NdefMessage to mojo NdefMessage.
     */
    @Test
    @Feature({"NFCTest"})
    public void testNdefToMojoConversion() throws UnsupportedEncodingException {
        // Test EMPTY record conversion.
        android.nfc.NdefMessage emptyNdefMessage = new android.nfc.NdefMessage(
                new android.nfc.NdefRecord(android.nfc.NdefRecord.TNF_EMPTY, null, null, null));
        NdefMessage emptyMojoNdefMessage = NdefMessageUtils.toNdefMessage(emptyNdefMessage);
        assertNull(emptyMojoNdefMessage.url);
        assertEquals(1, emptyMojoNdefMessage.data.length);
        assertEquals(NdefMessageUtils.RECORD_TYPE_EMPTY, emptyMojoNdefMessage.data[0].recordType);
        assertEquals(true, emptyMojoNdefMessage.data[0].mediaType.isEmpty());
        assertEquals(0, emptyMojoNdefMessage.data[0].data.length);

        // Test URL record conversion.
        android.nfc.NdefMessage urlNdefMessage =
                new android.nfc.NdefMessage(android.nfc.NdefRecord.createUri(TEST_URL));
        NdefMessage urlMojoNdefMessage = NdefMessageUtils.toNdefMessage(urlNdefMessage);
        assertNull(urlMojoNdefMessage.url);
        assertEquals(1, urlMojoNdefMessage.data.length);
        assertEquals(NdefMessageUtils.RECORD_TYPE_URL, urlMojoNdefMessage.data[0].recordType);
        assertEquals(TEXT_MIME, urlMojoNdefMessage.data[0].mediaType);
        assertEquals(TEST_URL, new String(urlMojoNdefMessage.data[0].data));

        // Test TEXT record conversion.
        android.nfc.NdefMessage textNdefMessage = new android.nfc.NdefMessage(
                android.nfc.NdefRecord.createTextRecord(LANG_EN_US, TEST_TEXT));
        NdefMessage textMojoNdefMessage = NdefMessageUtils.toNdefMessage(textNdefMessage);
        assertNull(textMojoNdefMessage.url);
        assertEquals(1, textMojoNdefMessage.data.length);
        assertEquals(NdefMessageUtils.RECORD_TYPE_TEXT, textMojoNdefMessage.data[0].recordType);
        assertEquals(TEXT_MIME, textMojoNdefMessage.data[0].mediaType);
        assertEquals(TEST_TEXT, new String(textMojoNdefMessage.data[0].data));

        // Test MIME record conversion.
        android.nfc.NdefMessage mimeNdefMessage =
                new android.nfc.NdefMessage(android.nfc.NdefRecord.createMime(
                        TEXT_MIME, ApiCompatibilityUtils.getBytesUtf8(TEST_TEXT)));
        NdefMessage mimeMojoNdefMessage = NdefMessageUtils.toNdefMessage(mimeNdefMessage);
        assertNull(mimeMojoNdefMessage.url);
        assertEquals(1, mimeMojoNdefMessage.data.length);
        assertEquals(NdefMessageUtils.RECORD_TYPE_OPAQUE, mimeMojoNdefMessage.data[0].recordType);
        assertEquals(TEXT_MIME, textMojoNdefMessage.data[0].mediaType);
        assertEquals(TEST_TEXT, new String(textMojoNdefMessage.data[0].data));

        // Test JSON record conversion.
        android.nfc.NdefMessage jsonNdefMessage =
                new android.nfc.NdefMessage(android.nfc.NdefRecord.createMime(
                        JSON_MIME, ApiCompatibilityUtils.getBytesUtf8(TEST_JSON)));
        NdefMessage jsonMojoNdefMessage = NdefMessageUtils.toNdefMessage(jsonNdefMessage);
        assertNull(jsonMojoNdefMessage.url);
        assertEquals(1, jsonMojoNdefMessage.data.length);
        assertEquals(NdefMessageUtils.RECORD_TYPE_JSON, jsonMojoNdefMessage.data[0].recordType);
        assertEquals(JSON_MIME, jsonMojoNdefMessage.data[0].mediaType);
        assertEquals(TEST_JSON, new String(jsonMojoNdefMessage.data[0].data));

        // Test Unknown record conversion.
        android.nfc.NdefMessage unknownNdefMessage = new android.nfc.NdefMessage(
                new android.nfc.NdefRecord(android.nfc.NdefRecord.TNF_UNKNOWN, null, null,
                        ApiCompatibilityUtils.getBytesUtf8(TEST_TEXT)));
        NdefMessage unknownMojoNdefMessage = NdefMessageUtils.toNdefMessage(unknownNdefMessage);
        assertNull(unknownMojoNdefMessage.url);
        assertEquals(1, unknownMojoNdefMessage.data.length);
        assertEquals(
                NdefMessageUtils.RECORD_TYPE_OPAQUE, unknownMojoNdefMessage.data[0].recordType);
        assertEquals(OCTET_STREAM_MIME, unknownMojoNdefMessage.data[0].mediaType);
        assertEquals(TEST_TEXT, new String(unknownMojoNdefMessage.data[0].data));

        // Test NdefMessage with WebNFC external type.
        android.nfc.NdefRecord jsonNdefRecord = android.nfc.NdefRecord.createMime(
                JSON_MIME, ApiCompatibilityUtils.getBytesUtf8(TEST_JSON));
        android.nfc.NdefRecord extNdefRecord =
                android.nfc.NdefRecord.createExternal(AUTHOR_RECORD_DOMAIN, AUTHOR_RECORD_TYPE,
                        ApiCompatibilityUtils.getBytesUtf8(TEST_URL));
        android.nfc.NdefMessage webNdefMessage =
                new android.nfc.NdefMessage(jsonNdefRecord, extNdefRecord);
        NdefMessage webMojoNdefMessage = NdefMessageUtils.toNdefMessage(webNdefMessage);
        assertEquals(TEST_URL, webMojoNdefMessage.url);
        assertEquals(1, webMojoNdefMessage.data.length);
        assertEquals(NdefMessageUtils.RECORD_TYPE_JSON, webMojoNdefMessage.data[0].recordType);
        assertEquals(JSON_MIME, webMojoNdefMessage.data[0].mediaType);
        assertEquals(TEST_JSON, new String(webMojoNdefMessage.data[0].data));
    }

    /**
     * Test conversion from mojo NdefMessage to android NdefMessage.
     */
    @Test
    @Feature({"NFCTest"})
    public void testMojoToNdefConversion() throws InvalidNdefMessageException {
        // Test URL record conversion.
        android.nfc.NdefMessage urlNdefMessage = createUrlWebNFCNdefMessage(TEST_URL);
        assertEquals(2, urlNdefMessage.getRecords().length);
        assertEquals(
                android.nfc.NdefRecord.TNF_WELL_KNOWN, urlNdefMessage.getRecords()[0].getTnf());
        assertEquals(TEST_URL, urlNdefMessage.getRecords()[0].toUri().toString());
        assertEquals(
                android.nfc.NdefRecord.TNF_EXTERNAL_TYPE, urlNdefMessage.getRecords()[1].getTnf());
        assertEquals(0,
                new String(urlNdefMessage.getRecords()[1].getType())
                        .compareToIgnoreCase(AUTHOR_RECORD_DOMAIN + ":" + AUTHOR_RECORD_TYPE));

        // Test TEXT record conversion.
        NdefRecord textMojoNdefRecord = new NdefRecord();
        textMojoNdefRecord.recordType = NdefMessageUtils.RECORD_TYPE_TEXT;
        textMojoNdefRecord.mediaType = TEXT_MIME;
        textMojoNdefRecord.data = ApiCompatibilityUtils.getBytesUtf8(TEST_TEXT);
        NdefMessage textMojoNdefMessage = createMojoNdefMessage(TEST_URL, textMojoNdefRecord);
        android.nfc.NdefMessage textNdefMessage =
                NdefMessageUtils.toNdefMessage(textMojoNdefMessage);
        assertEquals(2, textNdefMessage.getRecords().length);
        short tnf = textNdefMessage.getRecords()[0].getTnf();
        boolean isWellKnownOrMime = tnf == android.nfc.NdefRecord.TNF_WELL_KNOWN
                || tnf == android.nfc.NdefRecord.TNF_MIME_MEDIA;
        assertEquals(true, isWellKnownOrMime);
        assertEquals(
                android.nfc.NdefRecord.TNF_EXTERNAL_TYPE, textNdefMessage.getRecords()[1].getTnf());

        // Test MIME record conversion.
        NdefRecord mimeMojoNdefRecord = new NdefRecord();
        mimeMojoNdefRecord.recordType = NdefMessageUtils.RECORD_TYPE_OPAQUE;
        mimeMojoNdefRecord.mediaType = TEXT_MIME;
        mimeMojoNdefRecord.data = ApiCompatibilityUtils.getBytesUtf8(TEST_TEXT);
        NdefMessage mimeMojoNdefMessage = createMojoNdefMessage(TEST_URL, mimeMojoNdefRecord);
        android.nfc.NdefMessage mimeNdefMessage =
                NdefMessageUtils.toNdefMessage(mimeMojoNdefMessage);
        assertEquals(2, mimeNdefMessage.getRecords().length);
        assertEquals(
                android.nfc.NdefRecord.TNF_MIME_MEDIA, mimeNdefMessage.getRecords()[0].getTnf());
        assertEquals(TEXT_MIME, mimeNdefMessage.getRecords()[0].toMimeType());
        assertEquals(TEST_TEXT, new String(mimeNdefMessage.getRecords()[0].getPayload()));
        assertEquals(
                android.nfc.NdefRecord.TNF_EXTERNAL_TYPE, mimeNdefMessage.getRecords()[1].getTnf());

        // Test JSON record conversion.
        NdefRecord jsonMojoNdefRecord = new NdefRecord();
        jsonMojoNdefRecord.recordType = NdefMessageUtils.RECORD_TYPE_OPAQUE;
        jsonMojoNdefRecord.mediaType = JSON_MIME;
        jsonMojoNdefRecord.data = ApiCompatibilityUtils.getBytesUtf8(TEST_JSON);
        NdefMessage jsonMojoNdefMessage = createMojoNdefMessage(TEST_URL, jsonMojoNdefRecord);
        android.nfc.NdefMessage jsonNdefMessage =
                NdefMessageUtils.toNdefMessage(jsonMojoNdefMessage);
        assertEquals(2, jsonNdefMessage.getRecords().length);
        assertEquals(
                android.nfc.NdefRecord.TNF_MIME_MEDIA, jsonNdefMessage.getRecords()[0].getTnf());
        assertEquals(JSON_MIME, jsonNdefMessage.getRecords()[0].toMimeType());
        assertEquals(TEST_JSON, new String(jsonNdefMessage.getRecords()[0].getPayload()));
        assertEquals(
                android.nfc.NdefRecord.TNF_EXTERNAL_TYPE, jsonNdefMessage.getRecords()[1].getTnf());

        // Test EMPTY record conversion.
        NdefRecord emptyMojoNdefRecord = new NdefRecord();
        emptyMojoNdefRecord.recordType = NdefMessageUtils.RECORD_TYPE_EMPTY;
        NdefMessage emptyMojoNdefMessage = createMojoNdefMessage(TEST_URL, emptyMojoNdefRecord);
        android.nfc.NdefMessage emptyNdefMessage =
                NdefMessageUtils.toNdefMessage(emptyMojoNdefMessage);
        assertEquals(2, emptyNdefMessage.getRecords().length);
        assertEquals(android.nfc.NdefRecord.TNF_EMPTY, emptyNdefMessage.getRecords()[0].getTnf());
        assertEquals(android.nfc.NdefRecord.TNF_EXTERNAL_TYPE,
                emptyNdefMessage.getRecords()[1].getTnf());
    }

    /**
     * Test that invalid NdefMessage is rejected with INVALID_MESSAGE error code.
     */
    @Test
    @Feature({"NFCTest"})
    public void testInvalidNdefMessage() {
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        mDelegate.invokeCallback();
        PushResponse mockCallback = mock(PushResponse.class);
        nfc.push(new NdefMessage(), createNfcPushOptions(), mockCallback);
        nfc.processPendingOperationsForTesting(mNfcTagHandler);
        verify(mockCallback).call(mErrorCaptor.capture());
        assertEquals(NfcErrorType.INVALID_MESSAGE, mErrorCaptor.getValue().errorType);
    }

    /**
     * Test that Nfc.suspendNfcOperations() and Nfc.resumeNfcOperations() work correctly.
     */
    @Test
    @Feature({"NFCTest"})
    public void testResumeSuspend() {
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        // No activity / client or active pending operations
        nfc.suspendNfcOperations();
        nfc.resumeNfcOperations();

        mDelegate.invokeCallback();
        nfc.setClient(mNfcClient);
        WatchResponse mockCallback = mock(WatchResponse.class);
        nfc.watch(createNfcScanOptions(), mNextWatchId, mockCallback);
        nfc.suspendNfcOperations();
        verify(mNfcAdapter, times(1)).disableReaderMode(mActivity);
        nfc.resumeNfcOperations();
        // 1. Enable after watch is called, 2. after resumeNfcOperations is called.
        verify(mNfcAdapter, times(2))
                .enableReaderMode(any(Activity.class), any(ReaderCallback.class), anyInt(),
                        (Bundle) isNull());

        nfc.processPendingOperationsForTesting(mNfcTagHandler);
        // Check that watch request was completed successfully.
        verify(mockCallback).call(mErrorCaptor.capture());
        assertNull(mErrorCaptor.getValue());

        // Check that client was notified and watch with correct id was triggered.
        verify(mNfcClient, times(1))
                .onWatch(mOnWatchCallbackCaptor.capture(), nullable(String.class),
                        any(NdefMessage.class));
        assertEquals(mNextWatchId, mOnWatchCallbackCaptor.getValue()[0]);
    }

    /**
     * Test that Nfc.push() successful when NFC tag is connected.
     */
    @Test
    @Feature({"NFCTest"})
    public void testPush() {
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        mDelegate.invokeCallback();
        PushResponse mockCallback = mock(PushResponse.class);
        nfc.push(createMojoNdefMessage(), createNfcPushOptions(), mockCallback);
        nfc.processPendingOperationsForTesting(mNfcTagHandler);
        verify(mockCallback).call(mErrorCaptor.capture());
        assertNull(mErrorCaptor.getValue());
    }

    /**
     * Test that Nfc.cancelPush() cancels pending push opration and completes successfully.
     */
    @Test
    @Feature({"NFCTest"})
    public void testCancelPush() {
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        mDelegate.invokeCallback();
        PushResponse mockPushCallback = mock(PushResponse.class);
        CancelPushResponse mockCancelPushCallback = mock(CancelPushResponse.class);
        nfc.push(createMojoNdefMessage(), createNfcPushOptions(), mockPushCallback);
        nfc.cancelPush(NfcPushTarget.ANY, mockCancelPushCallback);

        // Check that push request was cancelled with OPERATION_CANCELLED.
        verify(mockPushCallback).call(mErrorCaptor.capture());
        assertEquals(NfcErrorType.OPERATION_CANCELLED, mErrorCaptor.getValue().errorType);

        // Check that cancel request was successfuly completed.
        verify(mockCancelPushCallback).call(mErrorCaptor.capture());
        assertNull(mErrorCaptor.getValue());
    }

    /**
     * Test that Nfc.watch() works correctly and client is notified.
     */
    @Test
    @Feature({"NFCTest"})
    public void testWatch() {
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        mDelegate.invokeCallback();
        nfc.setClient(mNfcClient);
        int watchId1 = mNextWatchId++;
        WatchResponse mockWatchCallback1 = mock(WatchResponse.class);
        nfc.watch(createNfcScanOptions(), watchId1, mockWatchCallback1);

        // Check that watch requests were completed successfully.
        verify(mockWatchCallback1).call(mErrorCaptor.capture());
        assertNull(mErrorCaptor.getValue());

        int watchId2 = mNextWatchId++;
        WatchResponse mockWatchCallback2 = mock(WatchResponse.class);
        nfc.watch(createNfcScanOptions(), watchId2, mockWatchCallback2);
        verify(mockWatchCallback2).call(mErrorCaptor.capture());
        assertNull(mErrorCaptor.getValue());

        // Mocks 'NFC tag found' event.
        nfc.processPendingOperationsForTesting(mNfcTagHandler);

        // Check that client was notified and correct watch ids were provided.
        verify(mNfcClient, times(1))
                .onWatch(mOnWatchCallbackCaptor.capture(), nullable(String.class),
                        any(NdefMessage.class));
        assertEquals(watchId1, mOnWatchCallbackCaptor.getValue()[0]);
        assertEquals(watchId2, mOnWatchCallbackCaptor.getValue()[1]);
    }

    /**
     * Test that Nfc.watch() matching function works correctly.
     */
    @Test
    @Feature({"NFCTest"})
    public void testWatchMatching() {
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        mDelegate.invokeCallback();
        nfc.setClient(mNfcClient);

        // Should match by WebNFC Id (exact match).
        NfcScanOptions options1 = createNfcScanOptions();
        options1.url = TEST_URL;
        int watchId1 = mNextWatchId++;
        WatchResponse mockWatchCallback1 = mock(WatchResponse.class);
        nfc.watch(options1, watchId1, mockWatchCallback1);
        verify(mockWatchCallback1).call(mErrorCaptor.capture());
        assertNull(mErrorCaptor.getValue());

        // Should match by media type.
        NfcScanOptions options2 = createNfcScanOptions();
        options2.mediaType = TEXT_MIME;
        int watchId2 = mNextWatchId++;
        WatchResponse mockWatchCallback2 = mock(WatchResponse.class);
        nfc.watch(options2, watchId2, mockWatchCallback2);
        verify(mockWatchCallback2).call(mErrorCaptor.capture());
        assertNull(mErrorCaptor.getValue());

        // Should match by record type.
        NfcScanOptions options3 = createNfcScanOptions();
        NdefRecordTypeFilter typeFilter = new NdefRecordTypeFilter();
        typeFilter.recordType = NdefMessageUtils.RECORD_TYPE_URL;
        options3.recordFilter = typeFilter;
        int watchId3 = mNextWatchId++;
        WatchResponse mockWatchCallback3 = mock(WatchResponse.class);
        nfc.watch(options3, watchId3, mockWatchCallback3);
        verify(mockWatchCallback3).call(mErrorCaptor.capture());
        assertNull(mErrorCaptor.getValue());

        // Should not match
        NfcScanOptions options4 = createNfcScanOptions();
        options4.url = AUTHOR_RECORD_DOMAIN;
        int watchId4 = mNextWatchId++;
        WatchResponse mockWatchCallback4 = mock(WatchResponse.class);
        nfc.watch(options4, watchId4, mockWatchCallback4);
        verify(mockWatchCallback4).call(mErrorCaptor.capture());
        assertNull(mErrorCaptor.getValue());

        nfc.processPendingOperationsForTesting(mNfcTagHandler);

        // Check that client was notified and watch with correct id was triggered.
        verify(mNfcClient, times(1))
                .onWatch(mOnWatchCallbackCaptor.capture(), nullable(String.class),
                        any(NdefMessage.class));
        assertEquals(3, mOnWatchCallbackCaptor.getValue().length);
        assertEquals(watchId1, mOnWatchCallbackCaptor.getValue()[0]);
        assertEquals(watchId2, mOnWatchCallbackCaptor.getValue()[1]);
        assertEquals(watchId3, mOnWatchCallbackCaptor.getValue()[2]);
    }

    /**
     * Test that Nfc.watch() can be cancelled with Nfc.cancelWatch().
     */
    @Test
    @Feature({"NFCTest"})
    public void testCancelWatch() {
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        mDelegate.invokeCallback();
        WatchResponse mockWatchCallback = mock(WatchResponse.class);
        nfc.watch(createNfcScanOptions(), mNextWatchId, mockWatchCallback);

        verify(mockWatchCallback).call(mErrorCaptor.capture());
        assertNull(mErrorCaptor.getValue());

        CancelWatchResponse mockCancelWatchCallback = mock(CancelWatchResponse.class);
        nfc.cancelWatch(mNextWatchId, mockCancelWatchCallback);

        // Check that cancel request was successfuly completed.
        verify(mockCancelWatchCallback).call(mErrorCaptor.capture());
        assertNull(mErrorCaptor.getValue());

        // Check that watch is not triggered when NFC tag is in proximity.
        nfc.processPendingOperationsForTesting(mNfcTagHandler);
        verify(mNfcClient, times(0))
                .onWatch(any(int[].class), nullable(String.class), any(NdefMessage.class));
    }

    /**
     * Test that Nfc.cancelAllWatches() cancels all pending watch operations.
     */
    @Test
    @Feature({"NFCTest"})
    public void testCancelAllWatches() {
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        mDelegate.invokeCallback();
        WatchResponse mockWatchCallback1 = mock(WatchResponse.class);
        WatchResponse mockWatchCallback2 = mock(WatchResponse.class);
        nfc.watch(createNfcScanOptions(), mNextWatchId++, mockWatchCallback1);
        verify(mockWatchCallback1).call(mErrorCaptor.capture());
        assertNull(mErrorCaptor.getValue());

        nfc.watch(createNfcScanOptions(), mNextWatchId++, mockWatchCallback2);
        verify(mockWatchCallback2).call(mErrorCaptor.capture());
        assertNull(mErrorCaptor.getValue());

        CancelAllWatchesResponse mockCallback = mock(CancelAllWatchesResponse.class);
        nfc.cancelAllWatches(mockCallback);

        // Check that cancel request was successfuly completed.
        verify(mockCallback).call(mErrorCaptor.capture());
        assertNull(mErrorCaptor.getValue());
    }

    /**
     * Test that Nfc.cancelWatch() with invalid id is failing with NOT_FOUND error.
     */
    @Test
    @Feature({"NFCTest"})
    public void testCancelWatchInvalidId() {
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        mDelegate.invokeCallback();
        WatchResponse mockWatchCallback = mock(WatchResponse.class);
        nfc.watch(createNfcScanOptions(), mNextWatchId, mockWatchCallback);

        verify(mockWatchCallback).call(mErrorCaptor.capture());
        assertNull(mErrorCaptor.getValue());

        CancelWatchResponse mockCancelWatchCallback = mock(CancelWatchResponse.class);
        nfc.cancelWatch(mNextWatchId + 1, mockCancelWatchCallback);

        verify(mockCancelWatchCallback).call(mErrorCaptor.capture());
        assertEquals(NfcErrorType.NOT_FOUND, mErrorCaptor.getValue().errorType);
    }

    /**
     * Test that Nfc.cancelAllWatches() is failing with NOT_FOUND error if there are no active
     * watch opeartions.
     */
    @Test
    @Feature({"NFCTest"})
    public void testCancelAllWatchesWithNoWathcers() {
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        mDelegate.invokeCallback();
        CancelAllWatchesResponse mockCallback = mock(CancelAllWatchesResponse.class);
        nfc.cancelAllWatches(mockCallback);

        verify(mockCallback).call(mErrorCaptor.capture());
        assertEquals(NfcErrorType.NOT_FOUND, mErrorCaptor.getValue().errorType);
    }

    /**
     * Test that when tag is disconnected during read operation, IllegalStateException is handled.
     */
    @Test
    @Feature({"NFCTest"})
    public void testTagDisconnectedDuringRead() throws IOException, FormatException {
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        mDelegate.invokeCallback();
        nfc.setClient(mNfcClient);
        WatchResponse mockWatchCallback = mock(WatchResponse.class);
        nfc.watch(createNfcScanOptions(), mNextWatchId, mockWatchCallback);

        // Force read operation to fail
        doThrow(IllegalStateException.class).when(mNfcTagHandler).read();

        // Mocks 'NFC tag found' event.
        nfc.processPendingOperationsForTesting(mNfcTagHandler);

        // Check that client was not notified.
        verify(mNfcClient, times(0))
                .onWatch(mOnWatchCallbackCaptor.capture(), nullable(String.class),
                        any(NdefMessage.class));
    }

    /**
     * Test that when tag is disconnected during write operation, IllegalStateException is handled.
     */
    @Test
    @Feature({"NFCTest"})
    public void testTagDisconnectedDuringWrite() throws IOException, FormatException {
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        mDelegate.invokeCallback();
        PushResponse mockCallback = mock(PushResponse.class);

        // Force write operation to fail
        doThrow(IllegalStateException.class)
                .when(mNfcTagHandler)
                .write(any(android.nfc.NdefMessage.class));
        nfc.push(createMojoNdefMessage(), createNfcPushOptions(), mockCallback);
        nfc.processPendingOperationsForTesting(mNfcTagHandler);
        verify(mockCallback).call(mErrorCaptor.capture());

        // Test that correct error is returned.
        assertNotNull(mErrorCaptor.getValue());
        assertEquals(NfcErrorType.IO_ERROR, mErrorCaptor.getValue().errorType);
    }

    /**
     * Test that push operation completes with TIMER_EXPIRED error when operation times-out.
     */
    @Test
    @Feature({"NFCTest"})
    public void testPushTimeout() {
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        mDelegate.invokeCallback();
        PushResponse mockCallback = mock(PushResponse.class);

        // Set 1 millisecond timeout.
        nfc.push(createMojoNdefMessage(), createNfcPushOptions(1), mockCallback);

        // Wait for timeout.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        // Test that correct error is returned.
        verify(mockCallback).call(mErrorCaptor.capture());
        assertNotNull(mErrorCaptor.getValue());
        assertEquals(NfcErrorType.TIMER_EXPIRED, mErrorCaptor.getValue().errorType);
    }

    /**
     * Test that multiple Nfc.push() invocations do not disable reader mode.
     */
    @Test
    @Feature({"NFCTest"})
    public void testPushMultipleInvocations() {
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        mDelegate.invokeCallback();

        PushResponse mockCallback1 = mock(PushResponse.class);
        PushResponse mockCallback2 = mock(PushResponse.class);
        nfc.push(createMojoNdefMessage(), createNfcPushOptions(), mockCallback1);
        nfc.push(createMojoNdefMessage(), createNfcPushOptions(), mockCallback2);

        verify(mNfcAdapter, times(1))
                .enableReaderMode(any(Activity.class), any(ReaderCallback.class), anyInt(),
                        (Bundle) isNull());
        verify(mNfcAdapter, times(0)).disableReaderMode(mActivity);

        verify(mockCallback1).call(mErrorCaptor.capture());
        assertNotNull(mErrorCaptor.getValue());
        assertEquals(NfcErrorType.OPERATION_CANCELLED, mErrorCaptor.getValue().errorType);
    }

    /**
     * Test that reader mode is disabled after push operation timeout is expired.
     */
    @Test
    @Feature({"NFCTest"})
    public void testReaderModeIsDisabledAfterTimeout() {
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        mDelegate.invokeCallback();
        PushResponse mockCallback = mock(PushResponse.class);

        // Set 1 millisecond timeout.
        nfc.push(createMojoNdefMessage(), createNfcPushOptions(1), mockCallback);

        verify(mNfcAdapter, times(1))
                .enableReaderMode(any(Activity.class), any(ReaderCallback.class), anyInt(),
                        (Bundle) isNull());

        // Wait for timeout.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        // Reader mode is disabled
        verify(mNfcAdapter, times(1)).disableReaderMode(mActivity);

        // Test that correct error is returned.
        verify(mockCallback).call(mErrorCaptor.capture());
        assertNotNull(mErrorCaptor.getValue());
        assertEquals(NfcErrorType.TIMER_EXPIRED, mErrorCaptor.getValue().errorType);
    }

    /**
     * Test that reader mode is disabled and two push operations are cancelled with correct
     * error code.
     */
    @Test
    @Feature({"NFCTest"})
    public void testTwoPushInvocationsWithCancel() {
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        mDelegate.invokeCallback();

        PushResponse mockCallback1 = mock(PushResponse.class);

        // First push without timeout, must be completed with OPERATION_CANCELLED.
        nfc.push(createMojoNdefMessage(), createNfcPushOptions(), mockCallback1);

        PushResponse mockCallback2 = mock(PushResponse.class);

        // Second push with 1 millisecond timeout, should be cancelled before timer expires,
        // thus, operation must be completed with OPERATION_CANCELLED.
        nfc.push(createMojoNdefMessage(), createNfcPushOptions(1), mockCallback2);

        verify(mNfcAdapter, times(1))
                .enableReaderMode(any(Activity.class), any(ReaderCallback.class), anyInt(),
                        (Bundle) isNull());
        verify(mockCallback1).call(mErrorCaptor.capture());
        assertNotNull(mErrorCaptor.getValue());
        assertEquals(NfcErrorType.OPERATION_CANCELLED, mErrorCaptor.getValue().errorType);

        CancelPushResponse mockCancelPushCallback = mock(CancelPushResponse.class);
        nfc.cancelPush(NfcPushTarget.ANY, mockCancelPushCallback);

        // Reader mode is disabled after cancelPush is invoked.
        verify(mNfcAdapter, times(1)).disableReaderMode(mActivity);

        // Wait for delayed tasks to complete.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        // The disableReaderMode is not called after delayed tasks are processed.
        verify(mNfcAdapter, times(1)).disableReaderMode(mActivity);

        // Test that correct error is returned.
        verify(mockCallback2).call(mErrorCaptor.capture());
        assertNotNull(mErrorCaptor.getValue());
        assertEquals(NfcErrorType.OPERATION_CANCELLED, mErrorCaptor.getValue().errorType);
    }

    /**
     * Test that reader mode is disabled and two push operations with timeout are cancelled
     * with correct error codes.
     */
    @Test
    @Feature({"NFCTest"})
    public void testTwoPushInvocationsWithTimeout() {
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        mDelegate.invokeCallback();

        PushResponse mockCallback1 = mock(PushResponse.class);

        // First push without timeout, must be completed with OPERATION_CANCELLED.
        nfc.push(createMojoNdefMessage(), createNfcPushOptions(1), mockCallback1);

        PushResponse mockCallback2 = mock(PushResponse.class);

        // Second push with 1 millisecond timeout, should be cancelled with TIMER_EXPIRED.
        nfc.push(createMojoNdefMessage(), createNfcPushOptions(1), mockCallback2);

        verify(mNfcAdapter, times(1))
                .enableReaderMode(any(Activity.class), any(ReaderCallback.class), anyInt(),
                        (Bundle) isNull());

        // Reader mode enabled for the duration of timeout.
        verify(mNfcAdapter, times(0)).disableReaderMode(mActivity);

        verify(mockCallback1).call(mErrorCaptor.capture());
        assertNotNull(mErrorCaptor.getValue());
        assertEquals(NfcErrorType.OPERATION_CANCELLED, mErrorCaptor.getValue().errorType);

        // Wait for delayed tasks to complete.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        // Reader mode is disabled
        verify(mNfcAdapter, times(1)).disableReaderMode(mActivity);

        // Test that correct error is returned for second push.
        verify(mockCallback2).call(mErrorCaptor.capture());
        assertNotNull(mErrorCaptor.getValue());
        assertEquals(NfcErrorType.TIMER_EXPIRED, mErrorCaptor.getValue().errorType);
    }

    /**
     * Test that reader mode is not disabled when there is an active watch operation and push
     * operation timer is expired.
     */
    @Test
    @Feature({"NFCTest"})
    public void testTimeoutDontDisableReaderMode() {
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        mDelegate.invokeCallback();
        WatchResponse mockWatchCallback = mock(WatchResponse.class);
        nfc.watch(createNfcScanOptions(), mNextWatchId, mockWatchCallback);

        PushResponse mockPushCallback = mock(PushResponse.class);
        // Should be cancelled with TIMER_EXPIRED.
        nfc.push(createMojoNdefMessage(), createNfcPushOptions(1), mockPushCallback);

        verify(mNfcAdapter, times(1))
                .enableReaderMode(any(Activity.class), any(ReaderCallback.class), anyInt(),
                        (Bundle) isNull());

        // Wait for delayed tasks to complete.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        // Push was cancelled with TIMER_EXPIRED.
        verify(mockPushCallback).call(mErrorCaptor.capture());
        assertNotNull(mErrorCaptor.getValue());
        assertEquals(NfcErrorType.TIMER_EXPIRED, mErrorCaptor.getValue().errorType);

        verify(mNfcAdapter, times(0)).disableReaderMode(mActivity);

        CancelAllWatchesResponse mockCancelCallback = mock(CancelAllWatchesResponse.class);
        nfc.cancelAllWatches(mockCancelCallback);

        // Check that cancel request was successfuly completed.
        verify(mockCancelCallback).call(mErrorCaptor.capture());
        assertNull(mErrorCaptor.getValue());

        // Reader mode is disabled when there are no pending push / watch operations.
        verify(mNfcAdapter, times(1)).disableReaderMode(mActivity);
    }

    /**
     * Test timeout overflow and negative timeout
     */
    @Test
    @Feature({"NFCTest"})
    public void testInvalidPushOptions() {
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        mDelegate.invokeCallback();
        PushResponse mockCallback = mock(PushResponse.class);

        // Long overflow
        nfc.push(createMojoNdefMessage(), createNfcPushOptions(Long.MAX_VALUE + 1), mockCallback);

        verify(mockCallback).call(mErrorCaptor.capture());
        assertNotNull(mErrorCaptor.getValue());
        assertEquals(NfcErrorType.NOT_SUPPORTED, mErrorCaptor.getValue().errorType);

        // Test negative timeout
        PushResponse mockCallback2 = mock(PushResponse.class);
        nfc.push(createMojoNdefMessage(), createNfcPushOptions(-1), mockCallback2);

        verify(mockCallback2).call(mErrorCaptor.capture());
        assertNotNull(mErrorCaptor.getValue());
        assertEquals(NfcErrorType.NOT_SUPPORTED, mErrorCaptor.getValue().errorType);
    }

    /**
     * Test that Nfc.watch() WebNFC Id pattern matching works correctly.
     */
    @Test
    @Feature({"NFCTest"})
    public void testWatchPatternMatching() throws IOException, FormatException {
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        mDelegate.invokeCallback();
        nfc.setClient(mNfcClient);

        // Should match.
        int watchId1 = mNextWatchId++;
        {
            NfcScanOptions options = createNfcScanOptions();
            options.url = "https://test.com/*";
            WatchResponse mockWatchCallback = mock(WatchResponse.class);
            nfc.watch(options, watchId1, mockWatchCallback);
            verify(mockWatchCallback).call(mErrorCaptor.capture());
            assertNull(mErrorCaptor.getValue());
        }

        // Should match.
        int watchId2 = mNextWatchId++;
        {
            NfcScanOptions options = createNfcScanOptions();
            options.url = "https://test.com/contact/42";
            WatchResponse mockWatchCallback = mock(WatchResponse.class);
            nfc.watch(options, watchId2, mockWatchCallback);
            verify(mockWatchCallback).call(mErrorCaptor.capture());
            assertNull(mErrorCaptor.getValue());
        }

        // Should match.
        int watchId3 = mNextWatchId++;
        {
            NfcScanOptions options = createNfcScanOptions();
            options.url = "https://subdomain.test.com/*";
            WatchResponse mockWatchCallback = mock(WatchResponse.class);
            nfc.watch(options, watchId3, mockWatchCallback);
            verify(mockWatchCallback).call(mErrorCaptor.capture());
            assertNull(mErrorCaptor.getValue());
        }

        // Should match.
        int watchId4 = mNextWatchId++;
        {
            NfcScanOptions options = createNfcScanOptions();
            options.url = "https://subdomain.test.com/contact";
            WatchResponse mockWatchCallback = mock(WatchResponse.class);
            nfc.watch(options, watchId4, mockWatchCallback);
            verify(mockWatchCallback).call(mErrorCaptor.capture());
            assertNull(mErrorCaptor.getValue());
        }

        // Should not match.
        {
            NfcScanOptions options = createNfcScanOptions();
            options.url = "https://www.test.com/*";
            WatchResponse mockWatchCallback = mock(WatchResponse.class);
            nfc.watch(options, mNextWatchId++, mockWatchCallback);
            verify(mockWatchCallback).call(mErrorCaptor.capture());
            assertNull(mErrorCaptor.getValue());
        }

        // Should not match.
        {
            NfcScanOptions options = createNfcScanOptions();
            options.url = "http://test.com/*";
            WatchResponse mockWatchCallback = mock(WatchResponse.class);
            nfc.watch(options, mNextWatchId++, mockWatchCallback);
            verify(mockWatchCallback).call(mErrorCaptor.capture());
            assertNull(mErrorCaptor.getValue());
        }

        // Should not match.
        {
            NfcScanOptions options = createNfcScanOptions();
            options.url = "invalid pattern url";
            WatchResponse mockWatchCallback = mock(WatchResponse.class);
            nfc.watch(options, mNextWatchId++, mockWatchCallback);
            verify(mockWatchCallback).call(mErrorCaptor.capture());
            assertNull(mErrorCaptor.getValue());
        }

        doReturn(createUrlWebNFCNdefMessage("https://subdomain.test.com/contact/42"))
                .when(mNfcTagHandler)
                .read();
        nfc.processPendingOperationsForTesting(mNfcTagHandler);

        // None of the watches should match NdefMessage with this WebNFC Id.
        doReturn(createUrlWebNFCNdefMessage("https://notest.com/foo")).when(mNfcTagHandler).read();
        nfc.processPendingOperationsForTesting(mNfcTagHandler);

        // Check that client was notified and watch with correct id was triggered.
        verify(mNfcClient, times(1))
                .onWatch(mOnWatchCallbackCaptor.capture(), nullable(String.class),
                        any(NdefMessage.class));
        assertEquals(4, mOnWatchCallbackCaptor.getValue().length);
        assertEquals(watchId1, mOnWatchCallbackCaptor.getValue()[0]);
        assertEquals(watchId2, mOnWatchCallbackCaptor.getValue()[1]);
        assertEquals(watchId3, mOnWatchCallbackCaptor.getValue()[2]);
        assertEquals(watchId4, mOnWatchCallbackCaptor.getValue()[3]);
    }

    /**
     * Test that Nfc.watch() WebNFC Id pattern matching works correctly for invalid WebNFC Ids.
     */
    @Test
    @Feature({"NFCTest"})
    public void testWatchPatternMatchingInvalidId() throws IOException, FormatException {
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        mDelegate.invokeCallback();
        nfc.setClient(mNfcClient);

        // Should not match when invalid WebNFC Id is received.
        NfcScanOptions options = createNfcScanOptions();
        options.url = "https://test.com/*";
        WatchResponse mockWatchCallback = mock(WatchResponse.class);
        nfc.watch(options, mNextWatchId, mockWatchCallback);
        verify(mockWatchCallback).call(mErrorCaptor.capture());
        assertNull(mErrorCaptor.getValue());

        doReturn(createUrlWebNFCNdefMessage("http://subdomain.test.com/contact/42"))
                .when(mNfcTagHandler)
                .read();
        nfc.processPendingOperationsForTesting(mNfcTagHandler);

        doReturn(createUrlWebNFCNdefMessage("ftp://subdomain.test.com/contact/42"))
                .when(mNfcTagHandler)
                .read();
        nfc.processPendingOperationsForTesting(mNfcTagHandler);

        doReturn(createUrlWebNFCNdefMessage("invalid url")).when(mNfcTagHandler).read();
        nfc.processPendingOperationsForTesting(mNfcTagHandler);

        verify(mNfcClient, times(0))
                .onWatch(mOnWatchCallbackCaptor.capture(), nullable(String.class),
                        any(NdefMessage.class));
    }

    /**
     * Test that Nfc.push() succeeds for NFC messages with EMPTY records.
     */
    @Test
    @Feature({"NFCTest"})
    public void testPushWithEmptyRecord() {
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        mDelegate.invokeCallback();
        PushResponse mockCallback = mock(PushResponse.class);

        // Create message with empty record.
        NdefRecord emptyNdefRecord = new NdefRecord();
        emptyNdefRecord.recordType = NdefMessageUtils.RECORD_TYPE_EMPTY;
        NdefMessage ndefMessage = createMojoNdefMessage(TEST_URL, emptyNdefRecord);

        nfc.push(ndefMessage, createNfcPushOptions(), mockCallback);
        nfc.processPendingOperationsForTesting(mNfcTagHandler);
        verify(mockCallback).call(mErrorCaptor.capture());
        assertNull(mErrorCaptor.getValue());
    }

    /**
     * Creates NfcPushOptions with default values.
     */
    private NfcPushOptions createNfcPushOptions() {
        return createNfcPushOptions(/*timeout=*/Double.POSITIVE_INFINITY);
    }

    /**
     * Creates NfcPushOptions with specified timeout.
     */
    private NfcPushOptions createNfcPushOptions(double timeout) {
        NfcPushOptions pushOptions = new NfcPushOptions();
        pushOptions.target = NfcPushTarget.ANY;
        pushOptions.timeout = timeout;
        pushOptions.ignoreRead = false;
        return pushOptions;
    }

    private NfcScanOptions createNfcScanOptions() {
        NfcScanOptions options = new NfcScanOptions();
        options.url = "";
        options.mediaType = "";
        options.recordFilter = null;
        return options;
    }

    private NdefMessage createMojoNdefMessage() {
        NdefMessage message = new NdefMessage();
        message.url = "";
        message.data = new NdefRecord[1];

        NdefRecord nfcRecord = new NdefRecord();
        nfcRecord.recordType = NdefMessageUtils.RECORD_TYPE_TEXT;
        nfcRecord.mediaType = TEXT_MIME;
        nfcRecord.data = ApiCompatibilityUtils.getBytesUtf8(TEST_TEXT);
        message.data[0] = nfcRecord;
        return message;
    }

    private NdefMessage createMojoNdefMessage(String url, NdefRecord record) {
        NdefMessage message = new NdefMessage();
        message.url = url;
        message.data = new NdefRecord[1];
        message.data[0] = record;
        return message;
    }

    private android.nfc.NdefMessage createUrlWebNFCNdefMessage(String webNfcId) {
        NdefRecord urlMojoNdefRecord = new NdefRecord();
        urlMojoNdefRecord.recordType = NdefMessageUtils.RECORD_TYPE_URL;
        urlMojoNdefRecord.mediaType = TEXT_MIME;
        urlMojoNdefRecord.data = ApiCompatibilityUtils.getBytesUtf8(TEST_URL);
        NdefMessage urlNdefMessage = createMojoNdefMessage(webNfcId, urlMojoNdefRecord);
        try {
            return NdefMessageUtils.toNdefMessage(urlNdefMessage);
        } catch (InvalidNdefMessageException e) {
            return null;
        }
    }
}
