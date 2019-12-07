// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedrequestmanager;

import org.chromium.base.Consumer;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration;
import org.chromium.chrome.browser.feed.library.api.host.config.Configuration.ConfigKey;
import org.chromium.chrome.browser.feed.library.api.host.logging.Task;
import org.chromium.chrome.browser.feed.library.api.host.network.HttpRequest;
import org.chromium.chrome.browser.feed.library.api.host.network.HttpRequest.HttpMethod;
import org.chromium.chrome.browser.feed.library.api.host.network.NetworkClient;
import org.chromium.chrome.browser.feed.library.api.host.storage.CommitResult;
import org.chromium.chrome.browser.feed.library.api.internal.common.SemanticPropertiesWithId;
import org.chromium.chrome.browser.feed.library.api.internal.common.ThreadUtils;
import org.chromium.chrome.browser.feed.library.api.internal.protocoladapter.ProtocolAdapter;
import org.chromium.chrome.browser.feed.library.api.internal.requestmanager.ActionUploadRequestManager;
import org.chromium.chrome.browser.feed.library.api.internal.store.Store;
import org.chromium.chrome.browser.feed.library.api.internal.store.UploadableActionMutation;
import org.chromium.chrome.browser.feed.library.common.Result;
import org.chromium.chrome.browser.feed.library.common.concurrent.TaskQueue;
import org.chromium.chrome.browser.feed.library.common.concurrent.TaskQueue.TaskType;
import org.chromium.chrome.browser.feed.library.common.logging.Logger;
import org.chromium.chrome.browser.feed.library.common.protoextensions.FeedExtensionRegistry;
import org.chromium.chrome.browser.feed.library.common.time.Clock;
import org.chromium.components.feed.core.proto.libraries.api.internal.StreamDataProto.StreamUploadableAction;
import org.chromium.components.feed.core.proto.wire.ConsistencyTokenProto.ConsistencyToken;
import org.chromium.components.feed.core.proto.wire.FeedActionResponseProto.FeedActionResponse;
import org.chromium.components.feed.core.proto.wire.ResponseProto.Response;

import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.TimeUnit;

/** Default implementation of ActionUploadRequestManager. */
public final class FeedActionUploadRequestManager implements ActionUploadRequestManager {
    private static final String TAG = "ActionUploadRequest";

    private final Configuration mConfiguration;
    private final NetworkClient mNetworkClient;
    private final ProtocolAdapter mProtocolAdapter;
    private final FeedExtensionRegistry mExtensionRegistry;
    private final TaskQueue mTaskQueue;
    private final ThreadUtils mThreadUtils;
    private final Store mStore;
    private final Clock mClock;
    private final long mMaxActionUploadAttempts;
    // Total number of actions that can be uploaded in a chained batch request.
    private final long mMaxActionsUploadsPerBatchedRequest;
    // Maximum bytes that can be uploaded in a single request.
    private final long mMaxBytesPerRequest;
    private final long mMaxActionUploadTtl;

    public FeedActionUploadRequestManager(Configuration configuration, NetworkClient networkClient,
            ProtocolAdapter protocolAdapter, FeedExtensionRegistry extensionRegistry,
            TaskQueue taskQueue, ThreadUtils threadUtils, Store store, Clock clock) {
        this.mConfiguration = configuration;
        this.mNetworkClient = networkClient;
        this.mProtocolAdapter = protocolAdapter;
        this.mExtensionRegistry = extensionRegistry;
        this.mTaskQueue = taskQueue;
        this.mThreadUtils = threadUtils;
        this.mStore = store;
        this.mClock = clock;
        mMaxBytesPerRequest = configuration.getValueOrDefault(
                ConfigKey.FEED_ACTION_SERVER_MAX_SIZE_PER_REQUEST, 4000L);
        mMaxActionsUploadsPerBatchedRequest = configuration.getValueOrDefault(
                ConfigKey.FEED_ACTION_SERVER_MAX_ACTIONS_PER_REQUEST, 10L);
        mMaxActionUploadAttempts =
                configuration.getValueOrDefault(ConfigKey.FEED_ACTION_MAX_UPLOAD_ATTEMPTS, 1L);
        mMaxActionUploadTtl =
                configuration.getValueOrDefault(ConfigKey.FEED_ACTION_TTL_SECONDS, 0L);
    }

    @Override
    public void triggerUploadActions(Set<StreamUploadableAction> actions, ConsistencyToken token,
            Consumer<Result<ConsistencyToken>> consumer) {
        if (mMaxActionUploadAttempts == 0 || mMaxBytesPerRequest == 0
                || mMaxActionsUploadsPerBatchedRequest == 0) {
            consumer.accept(Result.success(token));
            return;
        }
        triggerUploadActions(actions, token, consumer, /* uploadCount= */ 0);
    }

    private void triggerUploadActions(Set<StreamUploadableAction> actions, ConsistencyToken token,
            Consumer<Result<ConsistencyToken>> consumer, int uploadCount) {
        mThreadUtils.checkNotMainThread();
        // Return the token if there are no actions to upload.
        if (actions.isEmpty() || uploadCount >= mMaxActionsUploadsPerBatchedRequest) {
            consumer.accept(Result.success(token));
            return;
        }
        UploadableActionsRequestBuilder requestBuilder =
                new UploadableActionsRequestBuilder(mProtocolAdapter);
        int actionPayloadBytes = 0;
        Set<StreamUploadableAction> actionsToUpload = new HashSet<>();
        ArrayList<String> contentIds = new ArrayList<>();
        for (StreamUploadableAction action : actions) {
            int actionBytes = action.toByteArray().length;
            if (mMaxBytesPerRequest > actionPayloadBytes + actionBytes) {
                actionsToUpload.add(action);
                contentIds.add(action.getFeatureContentId());
                actionPayloadBytes += actionBytes;
            } else {
                break;
            }
        }

        Result<List<SemanticPropertiesWithId>> semanticPropertiesResult =
                mStore.getSemanticProperties(contentIds);
        List<SemanticPropertiesWithId> semanticPropertiesList = new ArrayList<>();
        if (semanticPropertiesResult.isSuccessful()
                && !semanticPropertiesResult.getValue().isEmpty()) {
            semanticPropertiesList = semanticPropertiesResult.getValue();
        }

        Consumer<Result<ConsistencyToken>> tokenConsumer = result -> {
            mThreadUtils.checkNotMainThread();
            if (result.isSuccessful()) {
                actions.removeAll(actionsToUpload);
                if (!actions.isEmpty()) {
                    triggerUploadActions(actions, result.getValue(), consumer,
                            uploadCount + actionsToUpload.size());
                } else {
                    consumer.accept(Result.success(result.getValue()));
                }
            } else {
                consumer.accept(uploadCount == 0 ? Result.failure() : Result.success(token));
            }
        };
        requestBuilder.setConsistencyToken(token)
                .setActions(actionsToUpload)
                .setSemanticProperties(semanticPropertiesList);
        executeUploadActionRequest(actionsToUpload, requestBuilder, tokenConsumer);
    }

    private void executeUploadActionRequest(Set<StreamUploadableAction> actions,
            UploadableActionsRequestBuilder requestBuilder,
            Consumer<Result<ConsistencyToken>> consumer) {
        mThreadUtils.checkNotMainThread();

        String endpoint =
                mConfiguration.getValueOrDefault(ConfigKey.FEED_ACTION_SERVER_ENDPOINT, "");
        @HttpMethod
        String httpMethod = mConfiguration.getValueOrDefault(
                ConfigKey.FEED_ACTION_SERVER_METHOD, HttpMethod.POST);
        HttpRequest httpRequest = RequestHelper.buildHttpRequest(
                httpMethod, requestBuilder.build().toByteArray(), endpoint, /* locale= */ "");

        Logger.i(TAG, "Making Request: %s", httpRequest.getUri().getPath());
        mNetworkClient.send(httpRequest, input -> {
            Logger.i(TAG, "Request: %s completed with response code: %s",
                    httpRequest.getUri().getPath(), input.getResponseCode());
            if (input.getResponseCode() != 200) {
                String errorBody = null;
                try {
                    errorBody = new String(input.getResponseBody(), "UTF-8");
                } catch (UnsupportedEncodingException e) {
                    Logger.e(TAG, "Error handling http error logging", e);
                }
                Logger.e(TAG, "errorCode: %d", input.getResponseCode());
                Logger.e(TAG, "errorResponse: %s", errorBody);
                mTaskQueue.execute(Task.EXECUTE_UPLOAD_ACTION_REQUEST, TaskType.IMMEDIATE,
                        () -> { consumer.accept(Result.failure()); });
                return;
            }
            handleUploadableActionResponseBytes(actions, input.getResponseBody(), consumer);
        });
    }

    private void handleUploadableActionResponseBytes(Set<StreamUploadableAction> actions,
            final byte[] responseBytes, final Consumer<Result<ConsistencyToken>> consumer) {
        mTaskQueue.execute(Task.HANDLE_UPLOADABLE_ACTION_RESPONSE_BYTES, TaskType.IMMEDIATE, () -> {
            Response response;
            boolean isLengthPrefixed = mConfiguration.getValueOrDefault(
                    ConfigKey.FEED_ACTION_SERVER_RESPONSE_LENGTH_PREFIXED, true);
            try {
                response = Response.parseFrom(isLengthPrefixed
                                ? RequestHelper.getLengthPrefixedValue(responseBytes)
                                : responseBytes,
                        mExtensionRegistry.getExtensionRegistry());
            } catch (IOException e) {
                Logger.e(TAG, e, "Response parse failed");
                handleUpdatingActionsOnFailure(actions);
                consumer.accept(Result.failure());
                return;
            }
            FeedActionResponse feedActionResponse =
                    response.getExtension(FeedActionResponse.feedActionResponse);
            final Result<ConsistencyToken> contextResult;
            if (feedActionResponse.hasConsistencyToken()) {
                contextResult = Result.success(feedActionResponse.getConsistencyToken());
                UploadableActionMutation actionMutation = mStore.editUploadableActions();
                for (StreamUploadableAction action : actions) {
                    actionMutation.remove(action, action.getFeatureContentId());
                }
                CommitResult commitResult = actionMutation.commit();
                if (commitResult != CommitResult.SUCCESS) {
                    // TODO:log failure to the basicLoggingApi
                    Logger.e(TAG, "Removing actions on success failed");
                }
            } else {
                contextResult = Result.failure();
                handleUpdatingActionsOnFailure(actions);
            }
            consumer.accept(contextResult);
        });
    }

    private void handleUpdatingActionsOnFailure(Set<StreamUploadableAction> actions) {
        UploadableActionMutation actionMutation = mStore.editUploadableActions();
        for (StreamUploadableAction action : actions) {
            int uploadAttempts = action.getUploadAttempts();
            long currentTime = TimeUnit.MILLISECONDS.toSeconds(mClock.currentTimeMillis());
            long timeSinceUpload = currentTime - action.getTimestampSeconds();
            if (uploadAttempts < mMaxActionUploadAttempts
                    && timeSinceUpload < mMaxActionUploadTtl) {
                actionMutation.upsert(
                        action.toBuilder().setUploadAttempts(uploadAttempts + 1).build(),
                        action.getFeatureContentId());
            } else {
                actionMutation.remove(action, action.getFeatureContentId());
            }
            CommitResult commitResult = actionMutation.commit();
            if (commitResult != CommitResult.SUCCESS) {
                // TODO:log failure to the basicLoggingApi
                Logger.e(TAG, "Upserting actions on failure failed");
            }
        }
    }
}
