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

/** Allows the Feed to query the host as to what actions are enabled. */
public interface ActionEnabledApi {

  /** Whether the host can open a URL. */
  boolean canOpenUrl();

  /** Whether the host can open a URL in incognito mode. */
  boolean canOpenUrlInIncognitoMode();

  /** Whether the host can open a URL in a new tab. */
  boolean canOpenUrlInNewTab();

  /** Whether the host can open a URL in a new window. */
  boolean canOpenUrlInNewWindow();

  /** Whether the host can download a URL. */
  boolean canDownloadUrl();

  /** Whether the host can open the Google Product Help page. */
  boolean canLearnMore();
}
