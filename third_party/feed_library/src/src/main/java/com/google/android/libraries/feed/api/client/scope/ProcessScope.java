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

package com.google.android.libraries.feed.api.client.scope;

import android.content.Context;
import com.google.android.libraries.feed.api.client.knowncontent.KnownContent;
import com.google.android.libraries.feed.api.client.lifecycle.AppLifecycleListener;
import com.google.android.libraries.feed.api.client.requestmanager.RequestManager;
import com.google.android.libraries.feed.api.host.action.ActionApi;
import com.google.android.libraries.feed.api.host.imageloader.ImageLoaderApi;
import com.google.android.libraries.feed.api.host.offlineindicator.OfflineIndicatorApi;
import com.google.android.libraries.feed.api.host.stream.CardConfiguration;
import com.google.android.libraries.feed.api.host.stream.SnackbarApi;
import com.google.android.libraries.feed.api.host.stream.StreamConfiguration;
import com.google.android.libraries.feed.api.host.stream.TooltipApi;
import com.google.android.libraries.feed.common.concurrent.TaskQueue;
import com.google.android.libraries.feed.common.logging.Dumpable;

/** Allows interaction with the Feed library at the process leve. */
public interface ProcessScope extends Dumpable {

  /** Returns the Feed library request manager. */
  RequestManager getRequestManager();

  /** Returns the Feed library task queue. */
  TaskQueue getTaskQueue();

  /** Returns the Feed library lifecycle listener. */
  AppLifecycleListener getAppLifecycleListener();

  /** Returns the Feed library known content. */
  KnownContent getKnownContent();

  /** Returns a {@link StreamScopeBuilder.Builder}. */
  StreamScopeBuilder createStreamScopeBuilder(
      Context context,
      ImageLoaderApi imageLoaderApi,
      ActionApi actionApi,
      StreamConfiguration streamConfiguration,
      CardConfiguration cardConfiguration,
      SnackbarApi snackbarApi,
      OfflineIndicatorApi offlineIndicatorApi,
      TooltipApi tooltipApi);

  /** Called to destroy the scope object. */
  void onDestroy();
}

