// Copyright 2018 The Feed Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package com.google.android.libraries.feed.feedrequestmanager;

import com.google.android.libraries.feed.api.host.config.Configuration;
import com.google.android.libraries.feed.api.host.config.Configuration.ConfigKey;
import com.google.android.libraries.feed.api.host.logging.Task;
import com.google.android.libraries.feed.api.host.network.HttpRequest;
import com.google.android.libraries.feed.api.host.network.HttpRequest.HttpMethod;
import com.google.android.libraries.feed.api.host.network.NetworkClient;
import com.google.android.libraries.feed.api.host.storage.CommitResult;
import com.google.android.libraries.feed.api.internal.common.SemanticPropertiesWithId;
import com.google.android.libraries.feed.api.internal.common.ThreadUtils;
import com.google.android.libraries.feed.api.internal.protocoladapter.ProtocolAdapter;
import com.google.android.libraries.feed.api.internal.requestmanager.ActionUploadRequestManager;
import com.google.android.libraries.feed.api.internal.store.Store;
import com.google.android.libraries.feed.api.internal.store.UploadableActionMutation;
import com.google.android.libraries.feed.common.Result;
import com.google.android.libraries.feed.common.concurrent.TaskQueue;
import com.google.android.libraries.feed.common.concurrent.TaskQueue.TaskType;
import com.google.android.libraries.feed.common.functional.Consumer;
import com.google.android.libraries.feed.common.logging.Logger;
import com.google.android.libraries.feed.common.protoextensions.FeedExtensionRegistry;
import com.google.android.libraries.feed.common.time.Clock;
import com.google.search.now.feed.client.StreamDataProto.StreamUploadableAction;
import com.google.search.now.wire.feed.ConsistencyTokenProto.ConsistencyToken;
import com.google.search.now.wire.feed.FeedActionResponseProto.FeedActionResponse;
import com.google.search.now.wire.feed.ResponseProto.Response;
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

  private final Configuration configuration;
  private final NetworkClient networkClient;
  private final ProtocolAdapter protocolAdapter;
  private final FeedExtensionRegistry extensionRegistry;
  private final TaskQueue taskQueue;
  private final ThreadUtils threadUtils;
  private final Store store;
  private final Clock clock;
  private final long maxActionUploadAttempts;
  // Total number of actions that can be uploaded in a chained batch request.
  private final long maxActionsUploadsPerBatchedRequest;
  // Maximum bytes that can be uploaded in a single request.
  private final long maxBytesPerRequest;
  private final long maxActionUploadTtl;

  public FeedActionUploadRequestManager(
      Configuration configuration,
      NetworkClient networkClient,
      ProtocolAdapter protocolAdapter,
      FeedExtensionRegistry extensionRegistry,
      TaskQueue taskQueue,
      ThreadUtils threadUtils,
      Store store,
      Clock clock) {
    this.configuration = configuration;
    this.networkClient = networkClient;
    this.protocolAdapter = protocolAdapter;
    this.extensionRegistry = extensionRegistry;
    this.taskQueue = taskQueue;
    this.threadUtils = threadUtils;
    this.store = store;
    this.clock = clock;
    maxBytesPerRequest =
        configuration.getValueOrDefault(ConfigKey.FEED_ACTION_SERVER_MAX_SIZE_PER_REQUEST, 4000L);
    maxActionsUploadsPerBatchedRequest =
        configuration.getValueOrDefault(ConfigKey.FEED_ACTION_SERVER_MAX_ACTIONS_PER_REQUEST, 10L);
    maxActionUploadAttempts =
        configuration.getValueOrDefault(ConfigKey.FEED_ACTION_MAX_UPLOAD_ATTEMPTS, 1L);
    maxActionUploadTtl = configuration.getValueOrDefault(ConfigKey.FEED_ACTION_TTL_SECONDS, 0L);
  }

  @Override
  public void triggerUploadActions(
      Set<StreamUploadableAction> actions,
      ConsistencyToken token,
      Consumer<Result<ConsistencyToken>> consumer) {
    if (maxActionUploadAttempts == 0
        || maxBytesPerRequest == 0
        || maxActionsUploadsPerBatchedRequest == 0) {
      consumer.accept(Result.success(token));
      return;
    }
    triggerUploadActions(actions, token, consumer, /* uploadCount= */ 0);
  }

  private void triggerUploadActions(
      Set<StreamUploadableAction> actions,
      ConsistencyToken token,
      Consumer<Result<ConsistencyToken>> consumer,
      int uploadCount) {
    threadUtils.checkNotMainThread();
    // Return the token if there are no actions to upload.
    if (actions.isEmpty() || uploadCount >= maxActionsUploadsPerBatchedRequest) {
      consumer.accept(Result.success(token));
      return;
    }
    UploadableActionsRequestBuilder requestBuilder =
        new UploadableActionsRequestBuilder(protocolAdapter);
    int actionPayloadBytes = 0;
    Set<StreamUploadableAction> actionsToUpload = new HashSet<>();
    ArrayList<String> contentIds = new ArrayList<>();
    for (StreamUploadableAction action : actions) {
      int actionBytes = action.toByteArray().length;
      if (maxBytesPerRequest > actionPayloadBytes + actionBytes) {
        actionsToUpload.add(action);
        contentIds.add(action.getFeatureContentId());
        actionPayloadBytes += actionBytes;
      } else {
        break;
      }
    }

    Result<List<SemanticPropertiesWithId>> semanticPropertiesResult =
        store.getSemanticProperties(contentIds);
    List<SemanticPropertiesWithId> semanticPropertiesList = new ArrayList<>();
    if (semanticPropertiesResult.isSuccessful() && !semanticPropertiesResult.getValue().isEmpty()) {
      semanticPropertiesList = semanticPropertiesResult.getValue();
    }

    Consumer<Result<ConsistencyToken>> tokenConsumer =
        result -> {
          threadUtils.checkNotMainThread();
          if (result.isSuccessful()) {
            actions.removeAll(actionsToUpload);
            if (!actions.isEmpty()) {
              triggerUploadActions(
                  actions, result.getValue(), consumer, uploadCount + actionsToUpload.size());
            } else {
              consumer.accept(Result.success(result.getValue()));
            }
          } else {
            consumer.accept(uploadCount == 0 ? Result.failure() : Result.success(token));
          }
        };
    requestBuilder
        .setConsistencyToken(token)
        .setActions(actionsToUpload)
        .setSemanticProperties(semanticPropertiesList);
    executeUploadActionRequest(actionsToUpload, requestBuilder, tokenConsumer);
  }

  private void executeUploadActionRequest(
      Set<StreamUploadableAction> actions,
      UploadableActionsRequestBuilder requestBuilder,
      Consumer<Result<ConsistencyToken>> consumer) {
    threadUtils.checkNotMainThread();

    String endpoint = configuration.getValueOrDefault(ConfigKey.FEED_ACTION_SERVER_ENDPOINT, "");
    @HttpMethod
    String httpMethod =
        configuration.getValueOrDefault(ConfigKey.FEED_ACTION_SERVER_METHOD, HttpMethod.POST);
    HttpRequest httpRequest =
        RequestHelper.buildHttpRequest(
            httpMethod, requestBuilder.build().toByteArray(), endpoint, /* locale= */ "");

    Logger.i(TAG, "Making Request: %s", httpRequest.getUri().getPath());
    networkClient.send(
        httpRequest,
        input -> {
          Logger.i(
              TAG,
              "Request: %s completed with response code: %s",
              httpRequest.getUri().getPath(),
              input.getResponseCode());
          if (input.getResponseCode() != 200) {
            String errorBody = null;
            try {
              errorBody = new String(input.getResponseBody(), "UTF-8");
            } catch (UnsupportedEncodingException e) {
              Logger.e(TAG, "Error handling http error logging", e);
            }
            Logger.e(TAG, "errorCode: %d", input.getResponseCode());
            Logger.e(TAG, "errorResponse: %s", errorBody);
            taskQueue.execute(
                Task.EXECUTE_UPLOAD_ACTION_REQUEST,
                TaskType.IMMEDIATE,
                () -> {
                  consumer.accept(Result.failure());
                });
            return;
          }
          handleUploadableActionResponseBytes(actions, input.getResponseBody(), consumer);
        });
  }

  private void handleUploadableActionResponseBytes(
      Set<StreamUploadableAction> actions,
      final byte[] responseBytes,
      final Consumer<Result<ConsistencyToken>> consumer) {
    taskQueue.execute(
        Task.HANDLE_UPLOADABLE_ACTION_RESPONSE_BYTES,
        TaskType.IMMEDIATE,
        () -> {
          Response response;
          boolean isLengthPrefixed =
              configuration.getValueOrDefault(
                  ConfigKey.FEED_ACTION_SERVER_RESPONSE_LENGTH_PREFIXED, true);
          try {
            response =
                Response.parseFrom(
                    isLengthPrefixed
                        ? RequestHelper.getLengthPrefixedValue(responseBytes)
                        : responseBytes,
                    extensionRegistry.getExtensionRegistry());
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
            UploadableActionMutation actionMutation = store.editUploadableActions();
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
    UploadableActionMutation actionMutation = store.editUploadableActions();
    for (StreamUploadableAction action : actions) {
      int uploadAttempts = action.getUploadAttempts();
      long currentTime = TimeUnit.MILLISECONDS.toSeconds(clock.currentTimeMillis());
      long timeSinceUpload = currentTime - action.getTimestampSeconds();
      if (uploadAttempts < maxActionUploadAttempts && timeSinceUpload < maxActionUploadTtl) {
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
