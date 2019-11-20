// Copyright 2019 The Feed Authors.
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

package com.google.android.libraries.feed.testing.requestmanager;

import com.google.android.libraries.feed.api.host.logging.RequestReason;
import com.google.android.libraries.feed.api.host.logging.Task;
import com.google.android.libraries.feed.api.internal.common.Model;
import com.google.android.libraries.feed.api.internal.protocoladapter.ProtocolAdapter;
import com.google.android.libraries.feed.api.internal.requestmanager.FeedRequestManager;
import com.google.android.libraries.feed.common.Result;
import com.google.android.libraries.feed.common.concurrent.MainThreadRunner;
import com.google.android.libraries.feed.common.concurrent.TaskQueue;
import com.google.android.libraries.feed.common.concurrent.TaskQueue.TaskType;
import com.google.android.libraries.feed.common.concurrent.testing.FakeThreadUtils;
import com.google.android.libraries.feed.common.functional.Consumer;
import com.google.search.now.feed.client.StreamDataProto.StreamToken;
import com.google.search.now.wire.feed.ConsistencyTokenProto.ConsistencyToken;
import com.google.search.now.wire.feed.ResponseProto.Response;
import java.util.ArrayDeque;
import java.util.Queue;

/**
 * Fake implementation of a {@link FeedRequestManager}. This acts a Queue of responses which will be
 * sent through the {@link ProtocolAdapter} to create the {@code List<StreamDataOperation>} which is
 * then returned as a {@link Result} to the {@link Consumer}.
 */
public class FakeFeedRequestManager implements FeedRequestManager {
  private final FakeThreadUtils fakeThreadUtils;
  private final MainThreadRunner mainThreadRunner;
  private final ProtocolAdapter protocolAdapter;
  private final Queue<ResponseWithDelay> responses = new ArrayDeque<>();
  private final TaskQueue taskQueue;
  /*@Nullable*/ private StreamToken latestStreamToken = null;
  @RequestReason private int latestRequestReason = RequestReason.UNKNOWN;

  public FakeFeedRequestManager(
      FakeThreadUtils fakeThreadUtils,
      MainThreadRunner mainThreadRunner,
      ProtocolAdapter protocolAdapter,
      TaskQueue taskQueue) {
    this.fakeThreadUtils = fakeThreadUtils;
    this.mainThreadRunner = mainThreadRunner;
    this.protocolAdapter = protocolAdapter;
    this.taskQueue = taskQueue;
  }

  // TODO: queue responses for action uploads
  /** Adds a Response to the queue. */
  public FakeFeedRequestManager queueResponse(Response response) {
    return queueResponse(response, /* delayMs= */ 0);
  }

  /** Adds a Response to the queue with a delay. */
  public FakeFeedRequestManager queueResponse(Response response, long delayMs) {
    responses.add(new ResponseWithDelay(response, delayMs));
    return this;
  }

  /** Adds an error to the queue. */
  public FakeFeedRequestManager queueError() {
    return queueError(/* delayMs= */ 0);
  }

  /** Adds an error to the queue with a delay. */
  public FakeFeedRequestManager queueError(long delayMs) {
    responses.add(new ResponseWithDelay(delayMs));
    return this;
  }

  @Override
  public void loadMore(
      StreamToken streamToken, ConsistencyToken token, Consumer<Result<Model>> consumer) {
    fakeThreadUtils.checkNotMainThread();
    latestStreamToken = streamToken;
    handleResponseWithDelay(responses.remove(), consumer);
  }

  @Override
  public void triggerRefresh(@RequestReason int reason, Consumer<Result<Model>> consumer) {
    triggerRefresh(reason, ConsistencyToken.getDefaultInstance(), consumer);
  }

  @Override
  public void triggerRefresh(
      @RequestReason int reason, ConsistencyToken token, Consumer<Result<Model>> consumer) {
    latestRequestReason = reason;
    ResponseWithDelay responseWithDelay = responses.remove();
    taskQueue.execute(
        Task.UNKNOWN,
        TaskType.HEAD_INVALIDATE,
        () -> {
          handleResponseWithDelay(responseWithDelay, consumer);
        });
  }

  private void handleResponseWithDelay(
      ResponseWithDelay responseWithDelay, Consumer<Result<Model>> consumer) {
    if (responseWithDelay.delayMs > 0) {
      mainThreadRunner.executeWithDelay(
          "FakeFeedRequestManager#consumer",
          () -> {
            invokeConsumer(responseWithDelay, consumer);
          },
          responseWithDelay.delayMs);
    } else {
      invokeConsumer(responseWithDelay, consumer);
    }
  }

  private void invokeConsumer(
      ResponseWithDelay responseWithDelay, Consumer<Result<Model>> consumer) {
    boolean policy = fakeThreadUtils.enforceMainThread(true);
    if (responseWithDelay.isError) {
      consumer.accept(Result.failure());
    } else {
      consumer.accept(protocolAdapter.createModel(responseWithDelay.response));
    }
    fakeThreadUtils.enforceMainThread(policy);
  }

  /** Returns the latest {@link StreamToken} passed in to the {@link FeedRequestManager}. */
  /*@Nullable*/
  public StreamToken getLatestStreamToken() {
    return latestStreamToken;
  }

  /**
   * Returns the latest {@link RequestReason} passed in to the {@link FeedRequestManager}. Returns
   * {@literal RequestReason.UNKNOWN} if the request manager has not been invoked.
   */
  @RequestReason
  public int getLatestRequestReason() {
    return latestRequestReason;
  }

  private static final class ResponseWithDelay {
    private final Response response;
    private final boolean isError;
    private final long delayMs;

    private ResponseWithDelay(Response response, long delayMs) {
      this.response = response;
      this.delayMs = delayMs;
      isError = false;
    }

    private ResponseWithDelay(long delayMs) {
      this.response = Response.getDefaultInstance();
      this.delayMs = delayMs;
      isError = true;
    }
  }
}
