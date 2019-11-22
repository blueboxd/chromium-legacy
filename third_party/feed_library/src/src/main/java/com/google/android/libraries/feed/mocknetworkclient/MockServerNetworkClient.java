// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.mocknetworkclient;

import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.provider.Settings;
import android.provider.Settings.Global;
import android.util.Base64;

import com.google.android.libraries.feed.api.host.network.HttpRequest;
import com.google.android.libraries.feed.api.host.network.HttpRequest.HttpMethod;
import com.google.android.libraries.feed.api.host.network.HttpResponse;
import com.google.android.libraries.feed.api.host.network.NetworkClient;
import com.google.android.libraries.feed.common.functional.Consumer;
import com.google.android.libraries.feed.common.logging.Logger;
import com.google.android.libraries.feed.feedrequestmanager.RequestHelper;
import com.google.protobuf.ByteString;
import com.google.protobuf.CodedOutputStream;
import com.google.protobuf.ExtensionRegistryLite;
import com.google.search.now.wire.feed.FeedRequestProto.FeedRequest;
import com.google.search.now.wire.feed.RequestProto.Request;
import com.google.search.now.wire.feed.ResponseProto.Response;
import com.google.search.now.wire.feed.mockserver.MockServerProto.ConditionalResponse;
import com.google.search.now.wire.feed.mockserver.MockServerProto.MockServer;

import java.io.IOException;
import java.util.List;

/** A network client that returns mock responses provided via a config proto */
public final class MockServerNetworkClient implements NetworkClient {
    private static final String TAG = "MockServerNetworkClient";

    private final Context context;
    private final Response initialResponse;
    private final List<ConditionalResponse> conditionalResponses;
    private final ExtensionRegistryLite extensionRegistry;
    private final long responseDelay;

    public MockServerNetworkClient(
            Context context, MockServer mockServer, long responseDelayMillis) {
        this.context = context;
        initialResponse = mockServer.getInitialResponse();
        conditionalResponses = mockServer.getConditionalResponsesList();

        extensionRegistry = ExtensionRegistryLite.newInstance();
        extensionRegistry.add(FeedRequest.feedRequest);
        responseDelay = responseDelayMillis;
    }

    @Override
    public void send(HttpRequest httpRequest, Consumer<HttpResponse> responseConsumer) {
        try {
            if (isAirplaneModeOn()) {
                delayedAccept(new HttpResponse(400, new byte[] {}), responseConsumer);
                return;
            }
            Request request = getRequest(httpRequest);
            ByteString requestToken =
                    (request.getExtension(FeedRequest.feedRequest).getFeedQuery().hasPageToken())
                    ? request.getExtension(FeedRequest.feedRequest).getFeedQuery().getPageToken()
                    : null;
            if (requestToken != null) {
                for (ConditionalResponse response : conditionalResponses) {
                    if (!response.hasContinuationToken()) {
                        Logger.w(TAG, "Conditional response without a token");
                        continue;
                    }
                    if (requestToken.equals(response.getContinuationToken())) {
                        delayedAccept(createHttpResponse(response.getResponse()), responseConsumer);
                        return;
                    }
                }
            }
            delayedAccept(createHttpResponse(initialResponse), responseConsumer);
        } catch (IOException e) {
            // TODO : handle errors here
            Logger.e(TAG, e.getMessage());
            delayedAccept(new HttpResponse(400, new byte[] {}), responseConsumer);
        }
    }

    private boolean isAirplaneModeOn() {
        return Settings.System.getInt(context.getContentResolver(), Global.AIRPLANE_MODE_ON, 0)
                != 0;
    }

    private void delayedAccept(HttpResponse httpResponse, Consumer<HttpResponse> responseConsumer) {
        if (responseDelay <= 0) {
            responseConsumer.accept(httpResponse);
            return;
        }

        new Handler(Looper.getMainLooper())
                .postDelayed(() -> responseConsumer.accept(httpResponse), responseDelay);
    }

    // TODO Fix nullness violation: incompatible types in argument.
    @SuppressWarnings("nullness:argument.type.incompatible")
    private Request getRequest(HttpRequest httpRequest) throws IOException {
        byte[] rawRequest = new byte[0];
        if (httpRequest.getMethod().equals(HttpMethod.GET)) {
            if (httpRequest.getUri().getQueryParameter(RequestHelper.MOTHERSHIP_PARAM_PAYLOAD)
                    != null) {
                rawRequest = Base64.decode(httpRequest.getUri().getQueryParameter(
                                                   RequestHelper.MOTHERSHIP_PARAM_PAYLOAD),
                        Base64.URL_SAFE);
            }
        } else {
            rawRequest = httpRequest.getBody();
        }
        return Request.parseFrom(rawRequest, extensionRegistry);
    }

    @Override
    public void close() {}

    private HttpResponse createHttpResponse(Response response) {
        if (response == null) {
            return new HttpResponse(500, new byte[] {});
        }

        try {
            byte[] rawResponse = response.toByteArray();
            byte[] newResponse = new byte[rawResponse.length + (Integer.SIZE / 8)];
            CodedOutputStream codedOutputStream = CodedOutputStream.newInstance(newResponse);
            codedOutputStream.writeUInt32NoTag(rawResponse.length);
            codedOutputStream.writeRawBytes(rawResponse);
            codedOutputStream.flush();
            return new HttpResponse(200, newResponse);
        } catch (IOException e) {
            Logger.e(TAG, "Error creating response", e);
            return new HttpResponse(500, new byte[] {});
        }
    }
}
