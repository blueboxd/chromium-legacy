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

package com.google.android.libraries.feed.api.host.action;

import com.google.android.libraries.feed.api.client.knowncontent.ContentMetadata;

/**
 * Allows the Feed instruct the host to perform actions. In the case that an action is triggered
 * that is not enabled the host has one of two options:
 *
 * <ol>
 *   <li>The host can ignore the action.
 *   <li>The host can throw an exception which the Stream will not catch, crashing the app.
 * </ol>
 */
public interface ActionPeformerApi {

  /** Opens the given URL. */
  void openUrl(String url);

  /** Opens the given URL in incognito mode. */
  void openUrlInIncognitoMode(String url);

  /** Opens the given URL in a new tab. */
  void openUrlInNewTab(String url);

  /** Opens the given URL in a new window. */
  void openUrlInNewWindow(String url);

  /**
   * Downloads the given url.
   *
   * @param contentMetadata The {@link ContentMetadata} defining the content to be downloaded.
   */
  void downloadUrl(ContentMetadata contentMetadata);

  /** Opens the Google Product Help page for the Feed. */
  void learnMore();
}
