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

package com.google.android.libraries.feed.api.client.knowncontent;

/** Information on the removal of a piece of content. */
public final class ContentRemoval {

  private final String url;
  private final boolean requestedByUser;

  public ContentRemoval(String url, boolean requestedByUser) {
    this.url = url;
    this.requestedByUser = requestedByUser;
  }

  /** Url for removed content. */
  public String getUrl() {
    return url;
  }

  /** Whether the removal was performed through an action of the user. */
  public boolean isRequestedByUser() {
    return requestedByUser;
  }
}
