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

package com.google.android.libraries.feed.basicstream.internal.pendingdismiss;

import com.google.android.libraries.feed.sharedstream.pendingdismiss.PendingDismissCallback;
import com.google.search.now.ui.action.FeedActionProto.UndoAction;

/**
 * Interface that is passed through the stream hierarchy so during a dismissLocal we can find the
 * cluster FeatureDriver and pass that to the StreamDriver to handle a pending dismissLocal.
 */
public interface ClusterPendingDismissHelper {

  /**
   * Triggers the temporary removal of the content with snackbar. Content will either come back or
   * be fully removed based on the interactions with the snackbar.
   *
   * @param undoAction - Information for the rendering of the snackbar.
   * @param pendingDismissCallback - Callbacks to call once content has been committed or reversed.
   */
  void triggerPendingDismissForCluster(
      UndoAction undoAction, PendingDismissCallback pendingDismissCallback);
}
