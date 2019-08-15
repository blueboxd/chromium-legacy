// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.times;

import static org.chromium.net.TestUrlRequestCallback.ResponseStep.ON_CANCELED;

import android.content.Context;
import android.os.ConditionVariable;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.support.test.runner.AndroidJUnit4;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;

import org.chromium.net.CronetEngine;
import org.chromium.net.InlineExecutionProhibitedException;
import org.chromium.net.TestUrlRequestCallback;
import org.chromium.net.UrlRequest.Status;
import org.chromium.net.UrlRequest.StatusListener;

import java.net.URI;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.concurrent.Executor;
import java.util.concurrent.atomic.AtomicBoolean;
/**
 * Test functionality of FakeUrlRequest.
 */
@RunWith(AndroidJUnit4.class)
public class FakeUrlRequestTest {
    private CronetEngine mFakeCronetEngine;
    private FakeCronetController mFakeCronetController;

    private static Context getContext() {
        return InstrumentationRegistry.getTargetContext();
    }

    private static void checkStatus(FakeUrlRequest request, final int expectedStatus) {
        ConditionVariable foundStatus = new ConditionVariable();
        request.getStatus(new StatusListener() {
            @Override
            public void onStatus(int status) {
                assertEquals(expectedStatus, status);
                foundStatus.open();
            }
        });
        foundStatus.block();
    }

    @Before
    public void setUp() {
        mFakeCronetController = new FakeCronetController();
        mFakeCronetEngine = mFakeCronetController.newFakeCronetEngineBuilder(getContext()).build();
    }

    @After
    public void tearDown() {
        mFakeCronetEngine.shutdown();
    }

    @Test
    @SmallTest
    public void testDefaultResponse() {
        // Setup the basic response.
        String responseText = "response text";
        FakeUrlResponse response =
                new FakeUrlResponse.Builder().setResponseBody(responseText.getBytes()).build();
        String url = "www.response.com";
        mFakeCronetController.addResponseForUrl(response, url);

        // Run the request.
        TestUrlRequestCallback callback = Mockito.spy(new TestUrlRequestCallback());
        FakeUrlRequest request =
                (FakeUrlRequest) mFakeCronetEngine
                        .newUrlRequestBuilder(url, callback, callback.getExecutor())
                        .build();
        request.start();
        callback.blockForDone();

        // Verify correct callback methods called and correct response returned.
        Mockito.verify(callback, times(1)).onResponseStarted(any(), any());
        Mockito.verify(callback, times(1)).onReadCompleted(any(), any(), any());
        Mockito.verify(callback, times(1)).onSucceeded(any(), any());
        assertEquals(callback.mResponseAsString, responseText);
    }

    @Test
    @SmallTest
    public void testBuilderChecks() throws Exception {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        try {
            mFakeCronetEngine.newUrlRequestBuilder(null, callback, callback.getExecutor());
            fail("URL not null-checked");
        } catch (NullPointerException e) {
            assertEquals("URL is required.", e.getMessage());
        }
        try {
            mFakeCronetEngine.newUrlRequestBuilder("url", null, callback.getExecutor());
            fail("Callback not null-checked");
        } catch (NullPointerException e) {
            assertEquals("Callback is required.", e.getMessage());
        }
        try {
            mFakeCronetEngine.newUrlRequestBuilder("url", callback, null);
            fail("Executor not null-checked");
        } catch (NullPointerException e) {
            assertEquals("Executor is required.", e.getMessage());
        }
        // Verify successful creation doesn't throw.
        mFakeCronetEngine.newUrlRequestBuilder("url", callback, callback.getExecutor());
    }

    @Test
    @SmallTest
    public void testSetHttpMethodWhenNullFails() {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        FakeUrlRequest request =
                (FakeUrlRequest) mFakeCronetEngine
                        .newUrlRequestBuilder("url", callback, callback.getExecutor())
                        .build();
        // Check exception thrown for null method.
        try {
            request.setHttpMethod(null);
            fail("Method not null-checked");
        } catch (NullPointerException e) {
            assertEquals("Method is required.", e.getMessage());
        }
    }

    @Test
    @SmallTest
    public void testSetHttpMethodWhenInvalidFails() {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        FakeUrlRequest request =
                (FakeUrlRequest) mFakeCronetEngine
                        .newUrlRequestBuilder("url", callback, callback.getExecutor())
                        .build();

        // Check exception thrown for invalid method.
        String method = "BADMETHOD";
        try {
            request.setHttpMethod(method);
            fail("Method not checked for validity");
        } catch (IllegalArgumentException e) {
            assertEquals("Invalid http method: " + method, e.getMessage());
        }
    }

    @Test
    @SmallTest
    public void testSetHttpMethodSetsMethodToCorrectMethod() {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        FakeUrlRequest request =
                (FakeUrlRequest) mFakeCronetEngine
                        .newUrlRequestBuilder("url", callback, callback.getExecutor())
                        .build();
        String testMethod = "POST";

        AtomicBoolean foundMethod = new AtomicBoolean();

        mFakeCronetController.addResponseMatcher(new ResponseMatcher() {
            @Override
            public FakeUrlResponse getMatchingResponse(
                    String url, String httpMethod, List<Map.Entry<String, String>> headers) {
                assertEquals(testMethod, httpMethod);
                foundMethod.set(true);
                // It doesn't matter if a response is actually returned.
                return null;
            }
        });

        // Check no exception for correct method.
        request.setHttpMethod(testMethod);

        // Run the request so that the ResponseMatcher we set is checked.
        request.start();
        callback.blockForDone();

        assertTrue(foundMethod.get());
    }

    @Test
    @SmallTest
    public void testAddHeader() {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        FakeUrlRequest request =
                (FakeUrlRequest) mFakeCronetEngine
                        .newUrlRequestBuilder("TEST_URL", callback, callback.getExecutor())
                        .build();
        String headerKey = "HEADERNAME";
        String headerValue = "HEADERVALUE";
        request.addHeader(headerKey, headerValue);
        AtomicBoolean foundEntry = new AtomicBoolean();
        mFakeCronetController.addResponseMatcher(new ResponseMatcher() {
            @Override
            public FakeUrlResponse getMatchingResponse(
                    String url, String httpMethod, List<Map.Entry<String, String>> headers) {
                assertEquals(1, headers.size());
                assertEquals(headerKey, headers.get(0).getKey());
                assertEquals(headerValue, headers.get(0).getValue());
                foundEntry.set(true);
                // It doesn't matter if a response is actually returned.
                return null;
            }
        });
        // Run the request so that the ResponseMatcher we set is checked.
        request.start();
        callback.blockForDone();

        assertTrue(foundEntry.get());
    }

    @Test
    @SmallTest
    public void testRequestDoesNotStartWhenEngineShutDown() {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        FakeUrlRequest request =
                (FakeUrlRequest) mFakeCronetEngine
                        .newUrlRequestBuilder("TEST_URL", callback, callback.getExecutor())
                        .build();

        mFakeCronetEngine.shutdown();
        try {
            request.start();
            fail("Request should check that the CronetEngine is not shutdown before starting.");
        } catch (IllegalStateException e) {
            assertEquals("This request's CronetEngine is already shutdown.", e.getMessage());
        }
    }

    @Test
    @SmallTest
    public void testRequestStopsWhenCanceled() {
        TestUrlRequestCallback callback = Mockito.spy(new TestUrlRequestCallback());
        FakeUrlRequest request =
                (FakeUrlRequest) mFakeCronetEngine
                        .newUrlRequestBuilder("TEST_URL", callback, callback.getExecutor())
                        .build();
        callback.setAutoAdvance(false);
        request.start();
        callback.waitForNextStep();
        request.cancel();

        callback.blockForDone();

        Mockito.verify(callback, times(1)).onCanceled(any(), any());
        Mockito.verify(callback, times(1)).onResponseStarted(any(), any());
        Mockito.verify(callback, times(0)).onReadCompleted(any(), any(), any());
        assertEquals(callback.mResponseStep, ON_CANCELED);
    }

    @Test
    @SmallTest
    public void testRecievedByteCountInUrlResponseInfoIsEqualToResponseLength() {
        // Setup the basic response.
        String responseText = "response text";
        FakeUrlResponse response =
                new FakeUrlResponse.Builder().setResponseBody(responseText.getBytes()).build();
        String url = "TEST_URL";
        mFakeCronetController.addResponseForUrl(response, url);

        // Run the request.
        TestUrlRequestCallback callback = Mockito.spy(new TestUrlRequestCallback());
        FakeUrlRequest request =
                (FakeUrlRequest) mFakeCronetEngine
                        .newUrlRequestBuilder(url, callback, callback.getExecutor())
                        .build();
        request.start();
        callback.blockForDone();

        assertEquals(responseText.length(), callback.mResponseInfo.getReceivedByteCount());
    }

    @Test
    @SmallTest
    public void testRedirectResponse() {
        // Setup the basic response.
        String responseText = "response text";
        String redirectLocation = "/redirect_location";
        FakeUrlResponse response = new FakeUrlResponse.Builder()
                                           .setResponseBody(responseText.getBytes())
                                           .addHeader("location", redirectLocation)
                                           .setHttpStatusCode(300)
                                           .build();

        String url = "TEST_URL";
        mFakeCronetController.addResponseForUrl(response, url);

        String redirectText = "redirect text";
        FakeUrlResponse redirectToResponse =
                new FakeUrlResponse.Builder().setResponseBody(redirectText.getBytes()).build();
        String redirectUrl = URI.create(url).resolve(redirectLocation).toString();

        mFakeCronetController.addResponseForUrl(redirectToResponse, redirectUrl);

        // Run the request.
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        FakeUrlRequest request =
                (FakeUrlRequest) mFakeCronetEngine
                        .newUrlRequestBuilder(url, callback, callback.getExecutor())
                        .build();
        request.start();
        callback.blockForDone();

        // Verify response from redirected URL is returned.
        assertTrue(Objects.equals(callback.mResponseAsString, redirectText));
    }

    @Test
    @SmallTest
    public void testRedirectResponseWithNoHeaderFails() {
        // Setup the basic response.
        String responseText = "response text";
        FakeUrlResponse response = new FakeUrlResponse.Builder()
                                           .setResponseBody(responseText.getBytes())
                                           .setHttpStatusCode(300)
                                           .build();

        String url = "TEST_URL";
        mFakeCronetController.addResponseForUrl(response, url);

        // Run the request.
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        FakeUrlRequest request =
                (FakeUrlRequest) mFakeCronetEngine
                        .newUrlRequestBuilder(url, callback, callback.getExecutor())
                        .build();
        request.start();
        callback.blockForDone();

        // Verify response from redirected URL is returned.
        assertEquals(TestUrlRequestCallback.ResponseStep.ON_FAILED, callback.mResponseStep);
    }

    @Test
    @SmallTest
    public void testResponseLongerThanBuffer() {
        // Build a long response string that is 3x the buffer size.
        final int bufferStringLengthMultiplier = 3;
        TestUrlRequestCallback callback = Mockito.spy(new TestUrlRequestCallback());
        String longResponseString =
                new String(new char[callback.mReadBufferSize * bufferStringLengthMultiplier]);

        String longResponseUrl = "https://www.longResponseUrl.com";

        FakeUrlResponse reallyLongResponse = new FakeUrlResponse.Builder()
                                                     .setResponseBody(longResponseString.getBytes())
                                                     .build();
        mFakeCronetController.addResponseForUrl(reallyLongResponse, longResponseUrl);

        FakeUrlRequest request =
                (FakeUrlRequest) mFakeCronetEngine
                        .newUrlRequestBuilder(longResponseUrl, callback, callback.getExecutor())
                        .build();
        request.start();
        callback.blockForDone();

        Mockito.verify(callback, times(1)).onResponseStarted(any(), any());
        Mockito.verify(callback, times(bufferStringLengthMultiplier))
                .onReadCompleted(any(), any(), any());
        Mockito.verify(callback, times(1)).onSucceeded(any(), any());
        assertTrue(Objects.equals(callback.mResponseAsString, longResponseString));
    }

    @Test
    @SmallTest
    public void testStatusInvalidBeforeStart() {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        FakeUrlRequest request =
                (FakeUrlRequest) mFakeCronetEngine
                        .newUrlRequestBuilder("URL", callback, callback.getExecutor())
                        .build();

        checkStatus(request, Status.INVALID);
        request.start();
        callback.blockForDone();
    }

    @Test
    @SmallTest
    public void testStatusIdleWhenWaitingForRead() {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        callback.setAutoAdvance(false);
        FakeUrlRequest request =
                (FakeUrlRequest) mFakeCronetEngine
                        .newUrlRequestBuilder("URL", callback, callback.getExecutor())
                        .build();
        request.start();
        checkStatus(request, Status.IDLE);
        callback.setAutoAdvance(true);
        callback.startNextRead(request);
        callback.blockForDone();
    }

    @Test
    @SmallTest
    public void testStatusIdleWhenWaitingForRedirect() {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        callback.setAutoAdvance(false);
        String initialURL = "initialURL";
        String secondURL = "secondURL";
        mFakeCronetController.addRedirectResponse(secondURL, initialURL);
        FakeUrlRequest request =
                (FakeUrlRequest) mFakeCronetEngine
                        .newUrlRequestBuilder(initialURL, callback, callback.getExecutor())
                        .build();

        request.start();
        checkStatus(request, Status.IDLE);
        callback.setAutoAdvance(true);
        request.followRedirect();
        callback.blockForDone();
    }

    @Test
    @SmallTest
    public void testStatusInvalidWhenDone() {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        FakeUrlRequest request =
                (FakeUrlRequest) mFakeCronetEngine
                        .newUrlRequestBuilder("URL", callback, callback.getExecutor())
                        .build();
        request.start();
        callback.blockForDone();
        checkStatus(request, Status.INVALID);
    }

    @Test
    @SmallTest
    public void testIsDoneWhenComplete() {
        TestUrlRequestCallback callback = Mockito.spy(new TestUrlRequestCallback());
        FakeUrlRequest request = (FakeUrlRequest) mFakeCronetEngine
                                         .newUrlRequestBuilder("", callback, callback.getExecutor())
                                         .build();

        request.start();
        callback.blockForDone();

        Mockito.verify(callback, times(1)).onSucceeded(any(), any());
        assertTrue(request.isDone());
    }

    @Test
    @SmallTest
    public void testSetUploadDataProviderAfterStart() {
        TestUrlRequestCallback callback = Mockito.spy(new TestUrlRequestCallback());
        FakeUrlRequest request = (FakeUrlRequest) mFakeCronetEngine
                                         .newUrlRequestBuilder("", callback, callback.getExecutor())
                                         .build();
        request.setUploadDataProvider(null, null);
        request.start();
        // Must wait for the request to prevent a race in the State since it is reported in the
        // error.
        callback.blockForDone();

        try {
            request.setUploadDataProvider(null, null);
            fail("UploadDataProvider cannot be changed after request has started");
        } catch (IllegalStateException e) {
            assertEquals("Request is already started. State is: 7", e.getMessage());
        }
    }

    @Test
    @SmallTest
    public void testUrlChainIsCorrectForSuccessRequest() {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String testUrl = "TEST_URL";
        List<String> expectedUrlChain = new ArrayList<>();
        expectedUrlChain.add(testUrl);
        FakeUrlRequest request =
                (FakeUrlRequest) mFakeCronetEngine
                        .newUrlRequestBuilder(testUrl, callback, callback.getExecutor())
                        .build();
        request.start();
        callback.blockForDone();

        assertEquals(expectedUrlChain, callback.mResponseInfo.getUrlChain());
    }

    @Test
    @SmallTest
    public void testUrlChainIsCorrectForRedirectRequest() {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String testUrl1 = "TEST_URL1";
        String testUrl2 = "TEST_URL2";
        mFakeCronetController.addRedirectResponse(testUrl2, testUrl1);
        List<String> expectedUrlChain = new ArrayList<>();
        expectedUrlChain.add(testUrl1);
        expectedUrlChain.add(testUrl2);
        FakeUrlRequest request =
                (FakeUrlRequest) mFakeCronetEngine
                        .newUrlRequestBuilder(testUrl1, callback, callback.getExecutor())
                        .build();
        request.start();
        callback.blockForDone();

        assertEquals(expectedUrlChain, callback.mResponseInfo.getUrlChain());
    }

    @Test
    @SmallTest
    public void testResponseCodeCorrect() {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String testUrl = "TEST_URL";
        int expectedResponseCode = 208;
        mFakeCronetController.addResponseForUrl(
                new FakeUrlResponse.Builder().setHttpStatusCode(expectedResponseCode).build(),
                testUrl);
        FakeUrlRequest request =
                (FakeUrlRequest) mFakeCronetEngine
                        .newUrlRequestBuilder(testUrl, callback, callback.getExecutor())
                        .build();
        request.start();
        callback.blockForDone();

        assertEquals(expectedResponseCode, callback.mResponseInfo.getHttpStatusCode());
    }

    @Test
    @SmallTest
    public void testResponseTextCorrect() {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String testUrl = "TEST_URL";
        int expectedResponseCode = 208;
        String expectedResponseText = "Already Reported";
        mFakeCronetController.addResponseForUrl(
                new FakeUrlResponse.Builder().setHttpStatusCode(expectedResponseCode).build(),
                testUrl);
        FakeUrlRequest request =
                (FakeUrlRequest) mFakeCronetEngine
                        .newUrlRequestBuilder(testUrl, callback, callback.getExecutor())
                        .build();
        request.start();
        callback.blockForDone();

        assertEquals(expectedResponseText, callback.mResponseInfo.getHttpStatusText());
    }

    @Test
    @SmallTest
    public void testResponseWasCachedCorrect() {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String testUrl = "TEST_URL";
        boolean expectedWasCached = true;
        mFakeCronetController.addResponseForUrl(
                new FakeUrlResponse.Builder().setWasCached(expectedWasCached).build(), testUrl);
        FakeUrlRequest request =
                (FakeUrlRequest) mFakeCronetEngine
                        .newUrlRequestBuilder(testUrl, callback, callback.getExecutor())
                        .build();
        request.start();
        callback.blockForDone();

        assertEquals(expectedWasCached, callback.mResponseInfo.wasCached());
    }

    @Test
    @SmallTest
    public void testResponseNegotiatedProtocolCorrect() {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String testUrl = "TEST_URL";
        String expectedNegotiatedProtocol = "TEST_NEGOTIATED_PROTOCOL";
        mFakeCronetController.addResponseForUrl(
                new FakeUrlResponse.Builder()
                        .setNegotiatedProtocol(expectedNegotiatedProtocol)
                        .build(),
                testUrl);
        FakeUrlRequest request =
                (FakeUrlRequest) mFakeCronetEngine
                        .newUrlRequestBuilder(testUrl, callback, callback.getExecutor())
                        .build();
        request.start();
        callback.blockForDone();

        assertEquals(expectedNegotiatedProtocol, callback.mResponseInfo.getNegotiatedProtocol());
    }

    @Test
    @SmallTest
    public void testResponseProxyServerCorrect() {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String testUrl = "TEST_URL";
        String expectedProxyServer = "TEST_PROXY_SERVER";
        mFakeCronetController.addResponseForUrl(
                new FakeUrlResponse.Builder().setProxyServer(expectedProxyServer).build(), testUrl);
        FakeUrlRequest request =
                (FakeUrlRequest) mFakeCronetEngine
                        .newUrlRequestBuilder(testUrl, callback, callback.getExecutor())
                        .build();
        request.start();
        callback.blockForDone();

        assertEquals(expectedProxyServer, callback.mResponseInfo.getProxyServer());
    }

    @Test
    @SmallTest
    public void testDirectExecutorDisabledByDefault() {
        TestUrlRequestCallback callback = Mockito.spy(new TestUrlRequestCallback());
        callback.setAllowDirectExecutor(true);
        Executor myExecutor = new Executor() {
            @Override
            public void execute(Runnable command) {
                command.run();
            }
        };
        FakeUrlRequest request =
                (FakeUrlRequest) mFakeCronetEngine.newUrlRequestBuilder("url", callback, myExecutor)
                        .build();

        request.start();
        Mockito.verify(callback).onFailed(any(), any(), any());
        // Checks that the exception from {@link DirectPreventingExecutor} was successfully returned
        // to the callabck in the onFailed method.
        assertTrue(callback.mError.getCause() instanceof InlineExecutionProhibitedException);
    }

    @Test
    @SmallTest
    public void testLotsOfCallsToReadDoesntOverflow() {
        TestUrlRequestCallback callback = Mockito.spy(new TestUrlRequestCallback());
        // Make the buffer size small so there are lots of calls to read().
        callback.mReadBufferSize = 1;
        String testUrl = "TEST_URL";
        int responseLength = 1024;
        byte[] byteArray = new byte[responseLength];
        Arrays.fill(byteArray, (byte) 1);
        String longResponseString = new String(byteArray);
        mFakeCronetController.addResponseForUrl(
                new FakeUrlResponse.Builder()
                        .setResponseBody(longResponseString.getBytes())
                        .build(),
                testUrl);
        FakeUrlRequest request =
                (FakeUrlRequest) mFakeCronetEngine
                        .newUrlRequestBuilder(testUrl, callback, callback.getExecutor())
                        .allowDirectExecutor()
                        .build();
        request.start();
        callback.blockForDone();
        assertEquals(longResponseString, callback.mResponseAsString);
        Mockito.verify(callback, times(responseLength)).onReadCompleted(any(), any(), any());
    }

    @Test
    @SmallTest
    public void testLotsOfCallsToReadDoesntOverflowWithDirectExecutor() {
        TestUrlRequestCallback callback = Mockito.spy(new TestUrlRequestCallback());
        callback.setAllowDirectExecutor(true);
        // Make the buffer size small so there are lots of calls to read().
        callback.mReadBufferSize = 1;
        String testUrl = "TEST_URL";
        int responseLength = 1024;
        byte[] byteArray = new byte[responseLength];
        Arrays.fill(byteArray, (byte) 1);
        String longResponseString = new String(byteArray);
        Executor myExecutor = new Executor() {
            @Override
            public void execute(Runnable command) {
                command.run();
            }
        };
        mFakeCronetController.addResponseForUrl(
                new FakeUrlResponse.Builder()
                        .setResponseBody(longResponseString.getBytes())
                        .build(),
                testUrl);
        FakeUrlRequest request = (FakeUrlRequest) mFakeCronetEngine
                                         .newUrlRequestBuilder(testUrl, callback, myExecutor)
                                         .allowDirectExecutor()
                                         .build();
        request.start();
        callback.blockForDone();
        assertEquals(longResponseString, callback.mResponseAsString);
        Mockito.verify(callback, times(responseLength)).onReadCompleted(any(), any(), any());
    }

    @Test
    @SmallTest
    public void testDoubleReadFails() {
        TestUrlRequestCallback callback = Mockito.spy(new TestUrlRequestCallback());
        callback.setAutoAdvance(false);
        FakeUrlRequest request =
                (FakeUrlRequest) mFakeCronetEngine
                        .newUrlRequestBuilder("url", callback, callback.getExecutor())
                        .build();
        request.start();
        callback.startNextRead(request);
        try {
            callback.startNextRead(request);
            fail("Double read() should be disallowed.");
        } catch (IllegalStateException e) {
            assertEquals("Invalid state transition - expected 4 but was 7", e.getMessage());
        }
    }

    @Test
    @SmallTest
    public void testReadWhileRedirectingFails() {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        callback.setAutoAdvance(false);
        String url = "url";
        FakeUrlRequest request =
                (FakeUrlRequest) mFakeCronetEngine
                        .newUrlRequestBuilder(url, callback, callback.getExecutor())
                        .build();
        mFakeCronetController.addRedirectResponse("location", url);
        request.start();
        try {
            callback.startNextRead(request);
            fail("Read should be disallowed while waiting for redirect.");
        } catch (IllegalStateException e) {
            assertEquals("Invalid state transition - expected 4 but was 3", e.getMessage());
        }
        callback.setAutoAdvance(true);
        request.followRedirect();
        callback.blockForDone();
    }

    @Test
    @SmallTest
    public void testShuttingDownCronetEngineWithActiveRequestFails() {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        callback.setAutoAdvance(false);
        String url = "url";
        FakeUrlRequest request =
                (FakeUrlRequest) mFakeCronetEngine
                        .newUrlRequestBuilder(url, callback, callback.getExecutor())
                        .build();

        request.start();

        try {
            mFakeCronetEngine.shutdown();
            fail("Shutdown not checked for active requests.");
        } catch (IllegalStateException e) {
            assertEquals("Cannot shutdown with active requests.", e.getMessage());
        }
        callback.setAutoAdvance(true);
        callback.startNextRead(request);
        callback.blockForDone();
        mFakeCronetEngine.shutdown();
    }

    @Test
    @SmallTest
    public void testDefaultResponseIs404() {
        TestUrlRequestCallback callback = new TestUrlRequestCallback();
        String url = "url";
        FakeUrlRequest request =
                (FakeUrlRequest) mFakeCronetEngine
                        .newUrlRequestBuilder(url, callback, callback.getExecutor())
                        .build();

        request.start();
        callback.blockForDone();

        assertEquals(404, callback.mResponseInfo.getHttpStatusCode());
    }
}
