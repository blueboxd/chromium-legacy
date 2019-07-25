// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.device.nfc;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
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
import static org.mockito.Mockito.verifyZeroInteractions;

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
import org.chromium.device.mojom.NdefCompatibility;
import org.chromium.device.mojom.NdefMessage;
import org.chromium.device.mojom.NdefRecord;
import org.chromium.device.mojom.NdefRecordType;
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
import org.chromium.device.mojom.NfcReaderOptions;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.io.IOException;
import java.io.UnsupportedEncodingException;

/**
 * Unit tests for NfcImpl and NfcTypeConverter classes.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NFCTest {
    private TestNfcDelegate mDelegate;
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
    private ArgumentCaptor<Integer> mWatchCaptor;
    @Captor
    private ArgumentCaptor<int[]> mOnWatchCallbackCaptor;

    // Constants used for the test.
    private static final String TEST_TEXT = "test";
    private static final String TEST_URL = "https://google.com";
    private static final String TEST_JSON = "{\"key1\":\"value1\",\"key2\":2}";
    private static final String DOMAIN = "w3.org";
    private static final String TYPE = "webnfc";
    private static final String TEXT_MIME = "text/plain";
    private static final String JSON_MIME = "application/json";
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
     * Test that error with type NOT_READABLE is returned if NFC is not supported.
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
        assertEquals(NfcErrorType.NOT_READABLE, mErrorCaptor.getValue().errorType);
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
        nfc.watch(createNfcReaderOptions(), mockCallback);
        verify(mockCallback).call(anyInt(), mErrorCaptor.capture());
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
        NdefMessage emptyMojoNdefMessage = NfcTypeConverter.toNdefMessage(emptyNdefMessage);
        assertNull(emptyMojoNdefMessage.url);
        assertEquals(1, emptyMojoNdefMessage.data.length);
        assertEquals(NdefRecordType.EMPTY, emptyMojoNdefMessage.data[0].recordType);
        assertEquals(true, emptyMojoNdefMessage.data[0].mediaType.isEmpty());
        assertEquals(0, emptyMojoNdefMessage.data[0].data.length);

        // Test URL record conversion.
        android.nfc.NdefMessage urlNdefMessage =
                new android.nfc.NdefMessage(android.nfc.NdefRecord.createUri(TEST_URL));
        NdefMessage urlMojoNdefMessage = NfcTypeConverter.toNdefMessage(urlNdefMessage);
        assertNull(urlMojoNdefMessage.url);
        assertEquals(1, urlMojoNdefMessage.data.length);
        assertEquals(NdefRecordType.URL, urlMojoNdefMessage.data[0].recordType);
        assertEquals(TEXT_MIME, urlMojoNdefMessage.data[0].mediaType);
        assertEquals(TEST_URL, new String(urlMojoNdefMessage.data[0].data));

        // Test TEXT record conversion.
        android.nfc.NdefMessage textNdefMessage = new android.nfc.NdefMessage(
                android.nfc.NdefRecord.createTextRecord(LANG_EN_US, TEST_TEXT));
        NdefMessage textMojoNdefMessage = NfcTypeConverter.toNdefMessage(textNdefMessage);
        assertNull(textMojoNdefMessage.url);
        assertEquals(1, textMojoNdefMessage.data.length);
        assertEquals(NdefRecordType.TEXT, textMojoNdefMessage.data[0].recordType);
        assertEquals(TEXT_MIME, textMojoNdefMessage.data[0].mediaType);
        assertEquals(TEST_TEXT, new String(textMojoNdefMessage.data[0].data));

        // Test MIME record conversion.
        android.nfc.NdefMessage mimeNdefMessage =
                new android.nfc.NdefMessage(android.nfc.NdefRecord.createMime(
                        TEXT_MIME, ApiCompatibilityUtils.getBytesUtf8(TEST_TEXT)));
        NdefMessage mimeMojoNdefMessage = NfcTypeConverter.toNdefMessage(mimeNdefMessage);
        assertNull(mimeMojoNdefMessage.url);
        assertEquals(1, mimeMojoNdefMessage.data.length);
        assertEquals(NdefRecordType.OPAQUE_RECORD, mimeMojoNdefMessage.data[0].recordType);
        assertEquals(TEXT_MIME, textMojoNdefMessage.data[0].mediaType);
        assertEquals(TEST_TEXT, new String(textMojoNdefMessage.data[0].data));

        // Test JSON record conversion.
        android.nfc.NdefMessage jsonNdefMessage =
                new android.nfc.NdefMessage(android.nfc.NdefRecord.createMime(
                        JSON_MIME, ApiCompatibilityUtils.getBytesUtf8(TEST_JSON)));
        NdefMessage jsonMojoNdefMessage = NfcTypeConverter.toNdefMessage(jsonNdefMessage);
        assertNull(jsonMojoNdefMessage.url);
        assertEquals(1, jsonMojoNdefMessage.data.length);
        assertEquals(NdefRecordType.JSON, jsonMojoNdefMessage.data[0].recordType);
        assertEquals(JSON_MIME, jsonMojoNdefMessage.data[0].mediaType);
        assertEquals(TEST_JSON, new String(jsonMojoNdefMessage.data[0].data));

        // Test NdefMessage with WebNFC external type.
        android.nfc.NdefRecord jsonNdefRecord = android.nfc.NdefRecord.createMime(
                JSON_MIME, ApiCompatibilityUtils.getBytesUtf8(TEST_JSON));
        android.nfc.NdefRecord extNdefRecord = android.nfc.NdefRecord.createExternal(
                DOMAIN, TYPE, ApiCompatibilityUtils.getBytesUtf8(TEST_URL));
        android.nfc.NdefMessage webNdefMessage =
                new android.nfc.NdefMessage(jsonNdefRecord, extNdefRecord);
        NdefMessage webMojoNdefMessage = NfcTypeConverter.toNdefMessage(webNdefMessage);
        assertEquals(TEST_URL, webMojoNdefMessage.url);
        assertEquals(1, webMojoNdefMessage.data.length);
        assertEquals(NdefRecordType.JSON, webMojoNdefMessage.data[0].recordType);
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
        assertEquals(DOMAIN + ":" + TYPE, new String(urlNdefMessage.getRecords()[1].getType()));

        // Test TEXT record conversion.
        NdefRecord textMojoNdefRecord = new NdefRecord();
        textMojoNdefRecord.recordType = NdefRecordType.TEXT;
        textMojoNdefRecord.mediaType = TEXT_MIME;
        textMojoNdefRecord.data = ApiCompatibilityUtils.getBytesUtf8(TEST_TEXT);
        NdefMessage textMojoNdefMessage = createMojoNdefMessage(TEST_URL, textMojoNdefRecord);
        android.nfc.NdefMessage textNdefMessage =
                NfcTypeConverter.toNdefMessage(textMojoNdefMessage);
        assertEquals(2, textNdefMessage.getRecords().length);
        short tnf = textNdefMessage.getRecords()[0].getTnf();
        boolean isWellKnownOrMime = tnf == android.nfc.NdefRecord.TNF_WELL_KNOWN
                || tnf == android.nfc.NdefRecord.TNF_MIME_MEDIA;
        assertEquals(true, isWellKnownOrMime);
        assertEquals(
                android.nfc.NdefRecord.TNF_EXTERNAL_TYPE, textNdefMessage.getRecords()[1].getTnf());

        // Test MIME record conversion.
        NdefRecord mimeMojoNdefRecord = new NdefRecord();
        mimeMojoNdefRecord.recordType = NdefRecordType.OPAQUE_RECORD;
        mimeMojoNdefRecord.mediaType = TEXT_MIME;
        mimeMojoNdefRecord.data = ApiCompatibilityUtils.getBytesUtf8(TEST_TEXT);
        NdefMessage mimeMojoNdefMessage = createMojoNdefMessage(TEST_URL, mimeMojoNdefRecord);
        android.nfc.NdefMessage mimeNdefMessage =
                NfcTypeConverter.toNdefMessage(mimeMojoNdefMessage);
        assertEquals(2, mimeNdefMessage.getRecords().length);
        assertEquals(
                android.nfc.NdefRecord.TNF_MIME_MEDIA, mimeNdefMessage.getRecords()[0].getTnf());
        assertEquals(TEXT_MIME, mimeNdefMessage.getRecords()[0].toMimeType());
        assertEquals(TEST_TEXT, new String(mimeNdefMessage.getRecords()[0].getPayload()));
        assertEquals(
                android.nfc.NdefRecord.TNF_EXTERNAL_TYPE, mimeNdefMessage.getRecords()[1].getTnf());

        // Test JSON record conversion.
        NdefRecord jsonMojoNdefRecord = new NdefRecord();
        jsonMojoNdefRecord.recordType = NdefRecordType.OPAQUE_RECORD;
        jsonMojoNdefRecord.mediaType = JSON_MIME;
        jsonMojoNdefRecord.data = ApiCompatibilityUtils.getBytesUtf8(TEST_JSON);
        NdefMessage jsonMojoNdefMessage = createMojoNdefMessage(TEST_URL, jsonMojoNdefRecord);
        android.nfc.NdefMessage jsonNdefMessage =
                NfcTypeConverter.toNdefMessage(jsonMojoNdefMessage);
        assertEquals(2, jsonNdefMessage.getRecords().length);
        assertEquals(
                android.nfc.NdefRecord.TNF_MIME_MEDIA, jsonNdefMessage.getRecords()[0].getTnf());
        assertEquals(JSON_MIME, jsonNdefMessage.getRecords()[0].toMimeType());
        assertEquals(TEST_JSON, new String(jsonNdefMessage.getRecords()[0].getPayload()));
        assertEquals(
                android.nfc.NdefRecord.TNF_EXTERNAL_TYPE, jsonNdefMessage.getRecords()[1].getTnf());

        // Test EMPTY record conversion.
        NdefRecord emptyMojoNdefRecord = new NdefRecord();
        emptyMojoNdefRecord.recordType = NdefRecordType.EMPTY;
        NdefMessage emptyMojoNdefMessage = createMojoNdefMessage(TEST_URL, emptyMojoNdefRecord);
        android.nfc.NdefMessage emptyNdefMessage =
                NfcTypeConverter.toNdefMessage(emptyMojoNdefMessage);
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
        nfc.watch(createNfcReaderOptions(), mockCallback);
        nfc.suspendNfcOperations();
        verify(mNfcAdapter, times(1)).disableReaderMode(mActivity);
        nfc.resumeNfcOperations();
        // 1. Enable after watch is called, 2. after resumeNfcOperations is called.
        verify(mNfcAdapter, times(2))
                .enableReaderMode(any(Activity.class), any(ReaderCallback.class), anyInt(),
                        (Bundle) isNull());

        nfc.processPendingOperationsForTesting(mNfcTagHandler);
        // Check that watch request was completed successfully.
        verify(mockCallback).call(mWatchCaptor.capture(), mErrorCaptor.capture());
        assertNull(mErrorCaptor.getValue());

        // Check that client was notified and watch with correct id was triggered.
        verify(mNfcClient, times(1))
                .onWatch(mOnWatchCallbackCaptor.capture(), nullable(String.class),
                        any(NdefMessage.class));
        assertEquals(mWatchCaptor.getValue().intValue(), mOnWatchCallbackCaptor.getValue()[0]);
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
     * Test that compatibility is checked for Nfc.push() with NFC_FORUM device.
     */
    @Test
    @Feature({"NFCTest"})
    public void testPushCompatibilityMatchingNfcForum() {
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        mDelegate.invokeCallback();
        doReturn(NdefCompatibility.NFC_FORUM).when(mNfcTagHandler).compatibility();

        // Should match, because |compatibility| is set to NdefCompatibility.ANY
        // in createNfcPushOptions().
        PushResponse mockCallback_1 = mock(PushResponse.class);
        nfc.push(createMojoNdefMessage(), createNfcPushOptions(), mockCallback_1);
        nfc.processPendingOperationsForTesting(mNfcTagHandler);
        verify(mockCallback_1).call(mErrorCaptor.capture());
        assertNull(mErrorCaptor.getValue());

        // Should match.
        PushResponse mockCallback_2 = mock(PushResponse.class);
        NfcPushOptions options_2 = createNfcPushOptions();
        options_2.compatibility = NdefCompatibility.NFC_FORUM;
        nfc.push(createMojoNdefMessage(), options_2, mockCallback_2);
        nfc.processPendingOperationsForTesting(mNfcTagHandler);
        verify(mockCallback_2).call(mErrorCaptor.capture());
        assertNull(mErrorCaptor.getValue());

        // Should not match.
        PushResponse mockCallback_3 = mock(PushResponse.class);
        NfcPushOptions options_3 = createNfcPushOptions();
        options_3.compatibility = NdefCompatibility.VENDOR;
        nfc.push(createMojoNdefMessage(), options_3, mockCallback_3);
        nfc.processPendingOperationsForTesting(mNfcTagHandler);
        verifyZeroInteractions(mockCallback_3);
    }

    /**
     * Test that compatibility is checked for Nfc.push() with VENDOR device.
     */
    @Test
    @Feature({"NFCTest"})
    public void testPushCompatibilityMatchingVendor() {
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        mDelegate.invokeCallback();
        doReturn(NdefCompatibility.VENDOR).when(mNfcTagHandler).compatibility();

        // Should match, because |compatibility| is set to NdefCompatibility.ANY
        // in createNfcPushOptions().
        PushResponse mockCallback_1 = mock(PushResponse.class);
        nfc.push(createMojoNdefMessage(), createNfcPushOptions(), mockCallback_1);
        nfc.processPendingOperationsForTesting(mNfcTagHandler);
        verify(mockCallback_1).call(mErrorCaptor.capture());
        assertNull(mErrorCaptor.getValue());

        // Should match for NdefCompatibility.VENDOR.
        PushResponse mockCallback_2 = mock(PushResponse.class);
        NfcPushOptions options_2 = createNfcPushOptions();
        options_2.compatibility = NdefCompatibility.VENDOR;
        nfc.push(createMojoNdefMessage(), options_2, mockCallback_2);
        nfc.processPendingOperationsForTesting(mNfcTagHandler);
        verify(mockCallback_2).call(mErrorCaptor.capture());
        assertNull(mErrorCaptor.getValue());

        // Should not match.
        PushResponse mockCallback_3 = mock(PushResponse.class);
        NfcPushOptions options_3 = createNfcPushOptions();
        options_3.compatibility = NdefCompatibility.NFC_FORUM;
        nfc.push(createMojoNdefMessage(), options_3, mockCallback_3);
        nfc.processPendingOperationsForTesting(mNfcTagHandler);
        verifyZeroInteractions(mockCallback_3);
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
        WatchResponse mockWatchCallback1 = mock(WatchResponse.class);
        nfc.watch(createNfcReaderOptions(), mockWatchCallback1);

        // Check that watch requests were completed successfully.
        verify(mockWatchCallback1).call(mWatchCaptor.capture(), mErrorCaptor.capture());
        assertNull(mErrorCaptor.getValue());
        int watchId1 = mWatchCaptor.getValue().intValue();

        WatchResponse mockWatchCallback2 = mock(WatchResponse.class);
        nfc.watch(createNfcReaderOptions(), mockWatchCallback2);
        verify(mockWatchCallback2).call(mWatchCaptor.capture(), mErrorCaptor.capture());
        assertNull(mErrorCaptor.getValue());
        int watchId2 = mWatchCaptor.getValue().intValue();
        // Check that each watch operation is associated with unique id.
        assertNotEquals(watchId1, watchId2);

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
        NfcReaderOptions options1 = createNfcReaderOptions();
        options1.compatibility = NdefCompatibility.NFC_FORUM;
        options1.url = TEST_URL;
        WatchResponse mockWatchCallback1 = mock(WatchResponse.class);
        nfc.watch(options1, mockWatchCallback1);
        verify(mockWatchCallback1).call(mWatchCaptor.capture(), mErrorCaptor.capture());
        assertNull(mErrorCaptor.getValue());
        int watchId1 = mWatchCaptor.getValue().intValue();

        // Should match by media type.
        NfcReaderOptions options2 = createNfcReaderOptions();
        options2.compatibility = NdefCompatibility.ANY;
        options2.mediaType = TEXT_MIME;
        WatchResponse mockWatchCallback2 = mock(WatchResponse.class);
        nfc.watch(options2, mockWatchCallback2);
        verify(mockWatchCallback2).call(mWatchCaptor.capture(), mErrorCaptor.capture());
        assertNull(mErrorCaptor.getValue());
        int watchId2 = mWatchCaptor.getValue().intValue();

        // Should match by record type.
        NfcReaderOptions options3 = createNfcReaderOptions();
        options3.compatibility = NdefCompatibility.ANY;
        NdefRecordTypeFilter typeFilter = new NdefRecordTypeFilter();
        typeFilter.recordType = NdefRecordType.URL;
        options3.recordFilter = typeFilter;
        WatchResponse mockWatchCallback3 = mock(WatchResponse.class);
        nfc.watch(options3, mockWatchCallback3);
        verify(mockWatchCallback3).call(mWatchCaptor.capture(), mErrorCaptor.capture());
        assertNull(mErrorCaptor.getValue());
        int watchId3 = mWatchCaptor.getValue().intValue();

        // Should not match
        NfcReaderOptions options4 = createNfcReaderOptions();
        options4.compatibility = NdefCompatibility.NFC_FORUM;
        options4.url = DOMAIN;
        WatchResponse mockWatchCallback4 = mock(WatchResponse.class);
        nfc.watch(options4, mockWatchCallback4);
        verify(mockWatchCallback4).call(mWatchCaptor.capture(), mErrorCaptor.capture());
        assertNull(mErrorCaptor.getValue());
        int watchId4 = mWatchCaptor.getValue().intValue();

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
        nfc.watch(createNfcReaderOptions(), mockWatchCallback);

        verify(mockWatchCallback).call(mWatchCaptor.capture(), mErrorCaptor.capture());
        assertNull(mErrorCaptor.getValue());

        CancelWatchResponse mockCancelWatchCallback = mock(CancelWatchResponse.class);
        nfc.cancelWatch(mWatchCaptor.getValue().intValue(), mockCancelWatchCallback);

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
        nfc.watch(createNfcReaderOptions(), mockWatchCallback1);
        verify(mockWatchCallback1).call(mWatchCaptor.capture(), mErrorCaptor.capture());
        assertNull(mErrorCaptor.getValue());

        nfc.watch(createNfcReaderOptions(), mockWatchCallback2);
        verify(mockWatchCallback2).call(mWatchCaptor.capture(), mErrorCaptor.capture());
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
        nfc.watch(createNfcReaderOptions(), mockWatchCallback);

        verify(mockWatchCallback).call(mWatchCaptor.capture(), mErrorCaptor.capture());
        assertNull(mErrorCaptor.getValue());

        CancelWatchResponse mockCancelWatchCallback = mock(CancelWatchResponse.class);
        nfc.cancelWatch(mWatchCaptor.getValue().intValue() + 1, mockCancelWatchCallback);

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
        nfc.watch(createNfcReaderOptions(), mockWatchCallback);

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
        nfc.watch(createNfcReaderOptions(), mockWatchCallback);

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
     * Test that 'nfc-forum' tag messages can only be read by Nfc.watch() with 'nfc-forum' or 'any'
     * option.
     */
    @Test
    @Feature({"NFCTest"})
    public void testWatchCompatibilityMatchingNfcForumTag() {
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        mDelegate.invokeCallback();
        nfc.setClient(mNfcClient);

        // Should match.
        {
            NfcReaderOptions options = createNfcReaderOptions();
            options.compatibility = NdefCompatibility.NFC_FORUM;
            options.url = TEST_URL;
            WatchResponse mockWatchCallback = mock(WatchResponse.class);
            nfc.watch(options, mockWatchCallback);
            verify(mockWatchCallback).call(mWatchCaptor.capture(), mErrorCaptor.capture());
            assertNull(mErrorCaptor.getValue());
        }
        int watchId1 = mWatchCaptor.getValue().intValue();

        // Should not match.
        {
            NfcReaderOptions options = createNfcReaderOptions();
            options.compatibility = NdefCompatibility.VENDOR;
            options.url = TEST_URL;
            WatchResponse mockWatchCallback = mock(WatchResponse.class);
            nfc.watch(options, mockWatchCallback);
            verify(mockWatchCallback).call(mWatchCaptor.capture(), mErrorCaptor.capture());
            assertNull(mErrorCaptor.getValue());
        }
        int watchId2 = mWatchCaptor.getValue().intValue();

        // Should match.
        {
            NfcReaderOptions options = createNfcReaderOptions();
            options.compatibility = NdefCompatibility.ANY;
            options.url = TEST_URL;
            WatchResponse mockWatchCallback = mock(WatchResponse.class);
            nfc.watch(options, mockWatchCallback);
            verify(mockWatchCallback).call(mWatchCaptor.capture(), mErrorCaptor.capture());
            assertNull(mErrorCaptor.getValue());
        }
        int watchId3 = mWatchCaptor.getValue().intValue();

        doReturn(NdefCompatibility.NFC_FORUM).when(mNfcTagHandler).compatibility();
        nfc.processPendingOperationsForTesting(mNfcTagHandler);

        // Check that client was notified and watch with 'nfc-forum' or 'any' compatibility was
        // triggered.
        verify(mNfcClient, times(1))
                .onWatch(mOnWatchCallbackCaptor.capture(), nullable(String.class),
                        any(NdefMessage.class));
        assertEquals(2, mOnWatchCallbackCaptor.getValue().length);
        assertEquals(watchId1, mOnWatchCallbackCaptor.getValue()[0]);
        assertEquals(watchId3, mOnWatchCallbackCaptor.getValue()[1]);
    }

    /**
     * Test that 'vendor' tag messages can only be read by Nfc.watch() with 'vendor' or 'any'
     * option.
     */
    @Test
    @Feature({"NFCTest"})
    public void testWatchCompatibilityMatchingVendorTag() {
        TestNfcImpl nfc = new TestNfcImpl(mContext, mDelegate);
        mDelegate.invokeCallback();
        nfc.setClient(mNfcClient);

        // Should not match.
        {
            NfcReaderOptions options = createNfcReaderOptions();
            options.compatibility = NdefCompatibility.NFC_FORUM;
            options.url = TEST_URL;
            WatchResponse mockWatchCallback = mock(WatchResponse.class);
            nfc.watch(options, mockWatchCallback);
            verify(mockWatchCallback).call(mWatchCaptor.capture(), mErrorCaptor.capture());
            assertNull(mErrorCaptor.getValue());
        }
        int watchId1 = mWatchCaptor.getValue().intValue();

        // Should match.
        {
            NfcReaderOptions options = createNfcReaderOptions();
            options.compatibility = NdefCompatibility.VENDOR;
            options.url = TEST_URL;
            WatchResponse mockWatchCallback = mock(WatchResponse.class);
            nfc.watch(options, mockWatchCallback);
            verify(mockWatchCallback).call(mWatchCaptor.capture(), mErrorCaptor.capture());
            assertNull(mErrorCaptor.getValue());
        }
        int watchId2 = mWatchCaptor.getValue().intValue();

        // Should match.
        {
            NfcReaderOptions options = createNfcReaderOptions();
            options.compatibility = NdefCompatibility.ANY;
            options.url = TEST_URL;
            WatchResponse mockWatchCallback = mock(WatchResponse.class);
            nfc.watch(options, mockWatchCallback);
            verify(mockWatchCallback).call(mWatchCaptor.capture(), mErrorCaptor.capture());
            assertNull(mErrorCaptor.getValue());
        }
        int watchId3 = mWatchCaptor.getValue().intValue();

        doReturn(NdefCompatibility.VENDOR).when(mNfcTagHandler).compatibility();
        nfc.processPendingOperationsForTesting(mNfcTagHandler);

        // Check that client was notified and watch with 'vendor' or 'any' compatibility was
        // triggered.
        verify(mNfcClient, times(1))
                .onWatch(mOnWatchCallbackCaptor.capture(), nullable(String.class),
                        any(NdefMessage.class));
        assertEquals(2, mOnWatchCallbackCaptor.getValue().length);
        assertEquals(watchId2, mOnWatchCallbackCaptor.getValue()[0]);
        assertEquals(watchId3, mOnWatchCallbackCaptor.getValue()[1]);
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
        {
            NfcReaderOptions options = createNfcReaderOptions();
            options.compatibility = NdefCompatibility.NFC_FORUM;
            options.url = "https://test.com/*";
            WatchResponse mockWatchCallback = mock(WatchResponse.class);
            nfc.watch(options, mockWatchCallback);
            verify(mockWatchCallback).call(mWatchCaptor.capture(), mErrorCaptor.capture());
            assertNull(mErrorCaptor.getValue());
        }
        int watchId1 = mWatchCaptor.getValue().intValue();

        // Should match.
        {
            NfcReaderOptions options = createNfcReaderOptions();
            options.compatibility = NdefCompatibility.NFC_FORUM;
            options.url = "https://test.com/contact/42";
            WatchResponse mockWatchCallback = mock(WatchResponse.class);
            nfc.watch(options, mockWatchCallback);
            verify(mockWatchCallback).call(mWatchCaptor.capture(), mErrorCaptor.capture());
            assertNull(mErrorCaptor.getValue());
        }
        int watchId2 = mWatchCaptor.getValue().intValue();

        // Should match.
        {
            NfcReaderOptions options = createNfcReaderOptions();
            options.compatibility = NdefCompatibility.NFC_FORUM;
            options.url = "https://subdomain.test.com/*";
            WatchResponse mockWatchCallback = mock(WatchResponse.class);
            nfc.watch(options, mockWatchCallback);
            verify(mockWatchCallback).call(mWatchCaptor.capture(), mErrorCaptor.capture());
            assertNull(mErrorCaptor.getValue());
        }
        int watchId3 = mWatchCaptor.getValue().intValue();

        // Should match.
        {
            NfcReaderOptions options = createNfcReaderOptions();
            options.compatibility = NdefCompatibility.NFC_FORUM;
            options.url = "https://subdomain.test.com/contact";
            WatchResponse mockWatchCallback = mock(WatchResponse.class);
            nfc.watch(options, mockWatchCallback);
            verify(mockWatchCallback).call(mWatchCaptor.capture(), mErrorCaptor.capture());
            assertNull(mErrorCaptor.getValue());
        }
        int watchId4 = mWatchCaptor.getValue().intValue();

        // Should not match.
        {
            NfcReaderOptions options = createNfcReaderOptions();
            options.compatibility = NdefCompatibility.NFC_FORUM;
            options.url = "https://www.test.com/*";
            WatchResponse mockWatchCallback = mock(WatchResponse.class);
            nfc.watch(options, mockWatchCallback);
            verify(mockWatchCallback).call(mWatchCaptor.capture(), mErrorCaptor.capture());
            assertNull(mErrorCaptor.getValue());
        }

        // Should not match.
        {
            NfcReaderOptions options = createNfcReaderOptions();
            options.compatibility = NdefCompatibility.NFC_FORUM;
            options.url = "http://test.com/*";
            WatchResponse mockWatchCallback = mock(WatchResponse.class);
            nfc.watch(options, mockWatchCallback);
            verify(mockWatchCallback).call(mWatchCaptor.capture(), mErrorCaptor.capture());
            assertNull(mErrorCaptor.getValue());
        }

        // Should not match.
        {
            NfcReaderOptions options = createNfcReaderOptions();
            options.compatibility = NdefCompatibility.NFC_FORUM;
            options.url = "invalid pattern url";
            WatchResponse mockWatchCallback = mock(WatchResponse.class);
            nfc.watch(options, mockWatchCallback);
            verify(mockWatchCallback).call(mWatchCaptor.capture(), mErrorCaptor.capture());
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
        NfcReaderOptions options = createNfcReaderOptions();
        options.compatibility = NdefCompatibility.NFC_FORUM;
        options.url = "https://test.com/*";
        WatchResponse mockWatchCallback = mock(WatchResponse.class);
        nfc.watch(options, mockWatchCallback);
        verify(mockWatchCallback).call(mWatchCaptor.capture(), mErrorCaptor.capture());
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
        emptyNdefRecord.recordType = NdefRecordType.EMPTY;
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
        pushOptions.compatibility = NdefCompatibility.ANY;
        return pushOptions;
    }

    private NfcReaderOptions createNfcReaderOptions() {
        NfcReaderOptions options = new NfcReaderOptions();
        options.url = "";
        options.mediaType = "";
        options.compatibility = NdefCompatibility.ANY;
        options.recordFilter = null;
        return options;
    }

    private NdefMessage createMojoNdefMessage() {
        NdefMessage message = new NdefMessage();
        message.url = "";
        message.data = new NdefRecord[1];

        NdefRecord nfcRecord = new NdefRecord();
        nfcRecord.recordType = NdefRecordType.TEXT;
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
        urlMojoNdefRecord.recordType = NdefRecordType.URL;
        urlMojoNdefRecord.mediaType = TEXT_MIME;
        urlMojoNdefRecord.data = ApiCompatibilityUtils.getBytesUtf8(TEST_URL);
        NdefMessage urlNdefMessage = createMojoNdefMessage(webNfcId, urlMojoNdefRecord);
        try {
            return NfcTypeConverter.toNdefMessage(urlNdefMessage);
        } catch (InvalidNdefMessageException e) {
            return null;
        }
    }
}
