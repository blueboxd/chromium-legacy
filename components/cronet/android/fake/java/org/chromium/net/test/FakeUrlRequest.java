// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.net.test;

import androidx.annotation.GuardedBy;

import org.chromium.net.CronetException;
import org.chromium.net.InlineExecutionProhibitedException;
import org.chromium.net.UploadDataProvider;
import org.chromium.net.UrlResponseInfo;
import org.chromium.net.impl.CallbackExceptionImpl;
import org.chromium.net.impl.CronetExceptionImpl;
import org.chromium.net.impl.JavaUrlRequestUtils.CheckedRunnable;
import org.chromium.net.impl.JavaUrlRequestUtils.DirectPreventingExecutor;
import org.chromium.net.impl.JavaUrlRequestUtils.State;
import org.chromium.net.impl.Preconditions;
import org.chromium.net.impl.UrlRequestBase;
import org.chromium.net.impl.UrlResponseInfoImpl;

import java.net.URI;
import java.nio.ByteBuffer;
import java.util.AbstractMap;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.Executor;

/**
 * Fake UrlRequest that retrieves responses from the associated FakeCronetController. Used for
 * testing Cronet usage on Android.
 */
final class FakeUrlRequest extends UrlRequestBase {
    // Callback used to report responses to the client.
    private final Callback mCallback;
    // The {@link Executor} provided by the user to be used for callbacks.
    private final Executor mUserExecutor;
    // The {@link Executor} provided by the engine used to break up callback loops.
    private final Executor mExecutor;
    // The {@link FakeCronetController} that will provide responses for this request.
    private final FakeCronetController mFakeCronetController;
    // The fake {@link CronetEngine} that should be notified when this request starts and stops.
    private final FakeCronetEngine mFakeCronetEngine;
    // Source of thread safety for this class.
    private final Object mLock = new Object();
    // The chain of URL's this request has received.
    @GuardedBy("mLock")
    private final List<String> mUrlChain = new ArrayList<>();
    // The list of HTTP headers used by this request to establish a connection.
    @GuardedBy("mLock")
    private final ArrayList<Map.Entry<String, String>> mAllHeadersList = new ArrayList<>();
    // The current URL this request is connecting to.
    @GuardedBy("mLock")
    private String mCurrentUrl;
    // The {@link FakeUrlResponse} for the current URL.
    @GuardedBy("mLock")
    private FakeUrlResponse mCurrentFakeResponse;
    // The {@link UrlResponseInfo} for the current request.
    @GuardedBy("mLock")
    private UrlResponseInfo mUrlResponseInfo;
    // The response from the current request that needs to be sent.
    @GuardedBy("mLock")
    private ByteBuffer mResponse;
    // The HTTP method used by this request to establish a connection.
    @GuardedBy("mLock")
    private String mHttpMethod = "GET";

    @GuardedBy("mLock")
    @State
    private int mState = State.NOT_STARTED;

    /**
     * Holds a subset of StatusValues - {@link State#STARTED} can represent
     * {@link Status#SENDING_REQUEST} or {@link Status#WAITING_FOR_RESPONSE}. While the distinction
     * isn't needed to implement the logic in this class, it is needed to implement
     * {@link #getStatus(StatusListener)}.
     */
    @StatusValues
    private volatile int mAdditionalStatusDetails = Status.INVALID;

    /**
     * Used to map from HTTP status codes to the corresponding human-readable text.
     */
    private final static Map<Integer, String> HTTP_STATUS_CODE_TO_TEXT;
    static {
        Map<Integer, String> httpCodeMap = new HashMap<>();
        httpCodeMap.put(100, "Continue");
        httpCodeMap.put(101, "Switching Protocols");
        httpCodeMap.put(102, "Processing");
        httpCodeMap.put(103, "Early Hints");
        httpCodeMap.put(200, "OK");
        httpCodeMap.put(201, "Created");
        httpCodeMap.put(202, "Accepted");
        httpCodeMap.put(203, "Non-Authoritative Information");
        httpCodeMap.put(204, "No Content");
        httpCodeMap.put(205, "Reset Content");
        httpCodeMap.put(206, "Partial Content");
        httpCodeMap.put(207, "Multi-Status");
        httpCodeMap.put(208, "Already Reported");
        httpCodeMap.put(226, "IM Used");
        httpCodeMap.put(300, "Multiple Choices");
        httpCodeMap.put(301, "Moved Permanently");
        httpCodeMap.put(302, "Found");
        httpCodeMap.put(303, "See Other");
        httpCodeMap.put(304, "Not Modified");
        httpCodeMap.put(305, "Use Proxy");
        httpCodeMap.put(306, "Unused");
        httpCodeMap.put(307, "Temporary Redirect");
        httpCodeMap.put(308, "Permanent Redirect");
        httpCodeMap.put(400, "Bad Request");
        httpCodeMap.put(401, "Unauthorized");
        httpCodeMap.put(402, "Payment Required");
        httpCodeMap.put(403, "Forbidden");
        httpCodeMap.put(404, "Not Found");
        httpCodeMap.put(405, "Method Not Allowed");
        httpCodeMap.put(406, "Not Acceptable");
        httpCodeMap.put(407, "Proxy Authentication Required");
        httpCodeMap.put(408, "Request Timeout");
        httpCodeMap.put(409, "Conflict");
        httpCodeMap.put(410, "Gone");
        httpCodeMap.put(411, "Length Required");
        httpCodeMap.put(412, "Precondition Failed");
        httpCodeMap.put(413, "Payload Too Large");
        httpCodeMap.put(414, "URI Too Long");
        httpCodeMap.put(415, "Unsupported Media Type");
        httpCodeMap.put(416, "Range Not Satisfiable");
        httpCodeMap.put(417, "Expectation Failed");
        httpCodeMap.put(421, "Misdirected Request");
        httpCodeMap.put(422, "Unprocessable Entity");
        httpCodeMap.put(423, "Locked");
        httpCodeMap.put(424, "Failed Dependency");
        httpCodeMap.put(425, "Too Early");
        httpCodeMap.put(426, "Upgrade Required");
        httpCodeMap.put(428, "Precondition Required");
        httpCodeMap.put(429, "Too Many Requests");
        httpCodeMap.put(431, "Request Header Fields Too Large");
        httpCodeMap.put(451, "Unavailable For Legal Reasons");
        httpCodeMap.put(500, "Internal Server Error");
        httpCodeMap.put(501, "Not Implemented");
        httpCodeMap.put(502, "Bad Gateway");
        httpCodeMap.put(503, "Service Unavailable");
        httpCodeMap.put(504, "Gateway Timeout");
        httpCodeMap.put(505, "HTTP Version Not Supported");
        httpCodeMap.put(506, "Variant Also Negotiates");
        httpCodeMap.put(507, "Insufficient Storage");
        httpCodeMap.put(508, "Loop Denied");
        httpCodeMap.put(510, "Not Extended");
        httpCodeMap.put(511, "Network Authentication Required");
        HTTP_STATUS_CODE_TO_TEXT = Collections.unmodifiableMap(httpCodeMap);
    }

    FakeUrlRequest(Callback callback, Executor userExecutor, Executor executor, String url,
            boolean allowDirectExecutor, boolean trafficStatsTagSet, int trafficStatsTag,
            final boolean trafficStatsUidSet, final int trafficStatsUid,
            FakeCronetController fakeCronetController, FakeCronetEngine fakeCronetEngine) {
        if (url == null) {
            throw new NullPointerException("URL is required");
        }
        if (callback == null) {
            throw new NullPointerException("Listener is required");
        }
        if (executor == null) {
            throw new NullPointerException("Executor is required");
        }
        mCallback = callback;
        mUserExecutor =
                allowDirectExecutor ? userExecutor : new DirectPreventingExecutor(userExecutor);
        mExecutor = executor;
        mCurrentUrl = url;
        mFakeCronetController = fakeCronetController;
        mFakeCronetEngine = fakeCronetEngine;
    }

    @Override
    public void setUploadDataProvider(UploadDataProvider uploadDataProvider, Executor executor) {
        synchronized (mLock) {
            checkNotStarted();
            // TODO(kirchman) Implement UploadDataProvider.
        }
    }

    @Override
    public void setHttpMethod(String method) {
        synchronized (mLock) {
            checkNotStarted();
            if (method == null) {
                throw new NullPointerException("Method is required.");
            }
            if ("OPTIONS".equalsIgnoreCase(method) || "GET".equalsIgnoreCase(method)
                    || "HEAD".equalsIgnoreCase(method) || "POST".equalsIgnoreCase(method)
                    || "PUT".equalsIgnoreCase(method) || "DELETE".equalsIgnoreCase(method)
                    || "TRACE".equalsIgnoreCase(method) || "PATCH".equalsIgnoreCase(method)) {
                mHttpMethod = method;
            } else {
                throw new IllegalArgumentException("Invalid http method: " + method);
            }
        }
    }

    @Override
    public void addHeader(String header, String value) {
        synchronized (mLock) {
            checkNotStarted();
            mAllHeadersList.add(new AbstractMap.SimpleEntry<String, String>(header, value));
        }
    }

    /**
     * Verifies that the request is not already started and throws an exception if it is.
     */
    @GuardedBy("mLock")
    private void checkNotStarted() {
        if (mState != State.NOT_STARTED) {
            throw new IllegalStateException("Request is already started. State is: " + mState);
        }
    }

    @Override
    public void start() {
        synchronized (mLock) {
            if (mFakeCronetEngine.startRequest()) {
                boolean transitionedState = false;
                try {
                    transitionStates(State.NOT_STARTED, State.STARTED);
                    mAdditionalStatusDetails = Status.CONNECTING;
                    transitionedState = true;
                } finally {
                    if (!transitionedState) {
                        mFakeCronetEngine.onRequestDestroyed();
                    }
                }
                mUrlChain.add(mCurrentUrl);
                fakeConnect();
            } else {
                throw new IllegalStateException("This request's CronetEngine is already shutdown.");
            }
        }
    }

    /**
     * Fakes a request to a server by retrieving a response to this {@link UrlRequest} from the
     * {@link FakeCronetController}.
     */
    @GuardedBy("mLock")
    private void fakeConnect() {
        mAdditionalStatusDetails = Status.WAITING_FOR_RESPONSE;
        mCurrentFakeResponse =
                mFakeCronetController.getResponse(mCurrentUrl, mHttpMethod, mAllHeadersList);
        int responseCode = mCurrentFakeResponse.getHttpStatusCode();
        mUrlResponseInfo = new UrlResponseInfoImpl(
                Collections.unmodifiableList(new ArrayList<>(mUrlChain)), responseCode,
                getDescriptionByCode(responseCode), mCurrentFakeResponse.getAllHeadersList(),
                mCurrentFakeResponse.getWasCached(), mCurrentFakeResponse.getNegotiatedProtocol(),
                mCurrentFakeResponse.getProxyServer(),
                mCurrentFakeResponse.getResponseBody().length);
        mResponse = ByteBuffer.wrap(mCurrentFakeResponse.getResponseBody());
        // Check for a redirect.
        if (responseCode >= 300 && responseCode < 400) {
            processRedirectResponse();
        } else {
            final UrlResponseInfo info = mUrlResponseInfo;
            transitionStates(State.STARTED, State.AWAITING_READ);
            executeCheckedRunnable(new CheckedRunnable() {
                @Override
                public void run() throws Exception {
                    mCallback.onResponseStarted(FakeUrlRequest.this, info);
                }
            });
        }
    }

    /**
     * Retrieves the redirect location from the response headers and responds to the
     * {@link UrlRequest.Callback#onRedirectReceived} method. Adds the redirect URL to the chain.
     *
     * @param url the URL that the {@link FakeUrlResponse} redirected this request to
     */
    @GuardedBy("mLock")
    private void processRedirectResponse() {
        transitionStates(State.STARTED, State.REDIRECT_RECEIVED);
        if (mUrlResponseInfo.getAllHeaders().get("location") == null) {
            // Response did not have a location header, so this request must fail.
            mUserExecutor.execute(new Runnable() {
                @Override
                public void run() {
                    tryToFailWithException(new CronetExceptionImpl(
                            "Request failed due to bad redirect HTTP headers",
                            new IllegalStateException("Response recieved from URL: " + mCurrentUrl
                                    + " was a "
                                    + "redirect, but lacked a location header.")));
                }
            });
            return;
        }
        String pendingRedirectUrl =
                URI.create(mCurrentUrl)
                        .resolve(mUrlResponseInfo.getAllHeaders().get("location").get(0))
                        .toString();
        mCurrentUrl = pendingRedirectUrl;
        mUrlChain.add(mCurrentUrl);
        transitionStates(State.REDIRECT_RECEIVED, State.AWAITING_FOLLOW_REDIRECT);
        final UrlResponseInfo info = mUrlResponseInfo;
        mExecutor.execute(new Runnable() {
            @Override
            public void run() {
                executeCheckedRunnable(new CheckedRunnable() {
                    @Override
                    public void run() throws Exception {
                        mCallback.onRedirectReceived(FakeUrlRequest.this, info, pendingRedirectUrl);
                    }
                });
            }
        });
    }

    @Override
    public void read(ByteBuffer buffer) {
        // Entering {@link #State.READING} is somewhat redundant because the entire response is
        // already acquired. We should still transition so that the fake {@link UrlRequest} follows
        // the same state flow as a real request.
        Preconditions.checkHasRemaining(buffer);
        Preconditions.checkDirect(buffer);
        synchronized (mLock) {
            transitionStates(State.AWAITING_READ, State.READING);
            final UrlResponseInfo info = mUrlResponseInfo;
            if (mResponse.hasRemaining()) {
                transitionStates(State.READING, State.AWAITING_READ);
                fillBufferWithResponse(buffer);
                mExecutor.execute(new Runnable() {
                    @Override
                    public void run() {
                        executeCheckedRunnable(new CheckedRunnable() {
                            @Override
                            public void run() throws Exception {
                                mCallback.onReadCompleted(FakeUrlRequest.this, info, buffer);
                            }
                        });
                    }
                });
            } else {
                if (setTerminalState(State.COMPLETE)) {
                    mUserExecutor.execute(new Runnable() {
                        @Override
                        public void run() {
                            mCallback.onSucceeded(FakeUrlRequest.this, info);
                        }
                    });
                }
            }
        }
    }

    /**
     * Puts as much of the remaining response as will fit into the {@link ByteBuffer} and removes
     * that part of the string from the response left to send.
     *
     * @param buffer the {@link ByteBuffer} to put the response into
     * @return the buffer with the response that we want to send back in it
     */
    @GuardedBy("mLock")
    private void fillBufferWithResponse(ByteBuffer buffer) {
        final int maxTransfer = Math.min(buffer.remaining(), mResponse.remaining());
        ByteBuffer temp = mResponse.duplicate();
        temp.limit(temp.position() + maxTransfer);
        buffer.put(temp);
        mResponse.position(mResponse.position() + maxTransfer);
    }

    @Override
    public void followRedirect() {
        synchronized (mLock) {
            transitionStates(State.AWAITING_FOLLOW_REDIRECT, State.STARTED);
            fakeConnect();
        }
    }

    @Override
    public void cancel() {
        synchronized (mLock) {
            final UrlResponseInfo info = mUrlResponseInfo;
            if (setTerminalState(State.CANCELLED)) {
                mUserExecutor.execute(new Runnable() {
                    @Override
                    public void run() {
                        mCallback.onCanceled(FakeUrlRequest.this, info);
                    }
                });
            }
        }
    }

    @Override
    public void getStatus(final StatusListener listener) {
        synchronized (mLock) {
            int extraStatus = this.mAdditionalStatusDetails;

            @StatusValues
            final int status;
            switch (mState) {
                case State.ERROR:
                case State.COMPLETE:
                case State.CANCELLED:
                case State.NOT_STARTED:
                    status = Status.INVALID;
                    break;
                case State.STARTED:
                    status = extraStatus;
                    break;
                case State.REDIRECT_RECEIVED:
                case State.AWAITING_FOLLOW_REDIRECT:
                case State.AWAITING_READ:
                    status = Status.IDLE;
                    break;
                case State.READING:
                    status = Status.READING_RESPONSE;
                    break;
                default:
                    throw new IllegalStateException("Switch is exhaustive: " + mState);
            }
            mUserExecutor.execute(new Runnable() {
                @Override
                public void run() {
                    listener.onStatus(status);
                }
            });
        }
    }

    @Override
    public boolean isDone() {
        synchronized (mLock) {
            return mState == State.COMPLETE || mState == State.ERROR || mState == State.CANCELLED;
        }
    }

    /**
     * Swaps from the expected state to a new state. If the swap fails, and it's not
     * due to an earlier error or cancellation, throws an exception.
     */
    @GuardedBy("mLock")
    private void transitionStates(@State int expected, @State int newState) {
        if (mState == expected) {
            mState = newState;
        } else {
            if (!(mState == State.CANCELLED || mState == State.ERROR)) {
                throw new IllegalStateException(
                        "Invalid state transition - expected " + expected + " but was " + mState);
            }
        }
    }

    /**
     * Calls the callback's onFailed method if this request is not complete. Should be executed on
     * the {@code mUserExecutor}, unless the error is a {@link InlineExecutionProhibitedException}
     * produced by the {@code mUserExecutor}.
     *
     * @param e the {@link CronetException} that the request should pass to the callback.
     *
     */
    private void tryToFailWithException(CronetException e) {
        synchronized (mLock) {
            if (setTerminalState(State.ERROR)) {
                mCallback.onFailed(FakeUrlRequest.this, mUrlResponseInfo, e);
            }
        }
    }

    /**
     * Execute a {@link CheckedRunnable} and call the {@link UrlRequest.Callback#onFailed} method
     * if there is an exception and we can change to {@link State.ERROR}. Used to communicate with
     * the {@link UrlRequest.Callback} methods using the executor provided by the constructor. This
     * should be the last call in the critical section. If this is not the last call in a critical
     * section, we risk modifying shared resources in a recursive call to another method
     * guarded by the {@code mLock}. This is because in Java synchronized blocks are reentrant.
     *
     * @param checkedRunnable the runnable to execute
     */
    private void executeCheckedRunnable(CheckedRunnable checkedRunnable) {
        try {
            mUserExecutor.execute(new Runnable() {
                @Override
                public void run() {
                    try {
                        checkedRunnable.run();
                    } catch (Exception e) {
                        tryToFailWithException(new CallbackExceptionImpl(
                                "Exception received from UrlRequest.Callback", e));
                    }
                }
            });
        } catch (InlineExecutionProhibitedException e) {
            // Don't try to fail using the {@code mUserExecutor} because it produced this error.
            tryToFailWithException(new CronetExceptionImpl("Direct executor not allowed", e));
        }
    }

    /**
     * Check the current state and if the request is started, but not complete, failed, or
     * cancelled, change to the terminal state and call {@link FakeCronetEngine#onDestroyed}. This
     * method ensures {@link FakeCronetEngine#onDestroyed} is only called once.
     *
     * @param terminalState the terminal state to set; one of {@link State.ERROR},
     * {@link State.COMPLETE}, or {@link State.CANCELLED}
     * @return true if the terminal state has been set.
     */
    @GuardedBy("mLock")
    private boolean setTerminalState(@State int terminalState) {
        switch (mState) {
            case State.NOT_STARTED:
                throw new IllegalStateException("Can't enter terminal state before start");
            case State.ERROR: // fallthrough
            case State.COMPLETE: // fallthrough
            case State.CANCELLED:
                return false; // Already in a terminal state
            default: {
                mState = terminalState;
                mFakeCronetEngine.onRequestDestroyed();
                return true;
            }
        }
    }

    /**
     * Gets a human readable description for a HTTP status code.
     *
     * @param code the code to retrieve the status for
     * @return the HTTP status text as a string
     */
    private static String getDescriptionByCode(Integer code) {
        return HTTP_STATUS_CODE_TO_TEXT.containsKey(code) ? HTTP_STATUS_CODE_TO_TEXT.get(code)
                                                          : "Unassigned";
    }
}
