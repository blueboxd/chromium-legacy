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

package com.google.android.libraries.feed.feedsessionmanager.internal;

import com.google.android.libraries.feed.common.logging.Dumpable;
import com.google.android.libraries.feed.common.logging.Dumper;
import com.google.android.libraries.feed.common.logging.Logger;
import com.google.search.now.feed.client.StreamDataProto.StreamStructure;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Class tracking the content IDs associated to a session. */
public class SessionContentTracker implements Dumpable {
  private static final String TAG = "SessionContentTracker";

  private final boolean supportsClearAll;
  private final Set<String> contentIds = new HashSet<>();

  SessionContentTracker(boolean supportsClearAll) {
    this.supportsClearAll = supportsClearAll;
  }

  public boolean isEmpty() {
    return contentIds.isEmpty();
  }

  public void clear() {
    contentIds.clear();
  }

  public boolean contains(String contentId) {
    return contentIds.contains(contentId);
  }

  public Set<String> getContentIds() {
    return new HashSet<>(contentIds);
  }

  public void update(StreamStructure streamStructure) {
    String contentId = streamStructure.getContentId();
    switch (streamStructure.getOperation()) {
      case UPDATE_OR_APPEND: // Fall-through
      case REQUIRED_CONTENT:
        contentIds.add(contentId);
        break;
      case REMOVE:
        contentIds.remove(contentId);
        break;
      case CLEAR_ALL:
        if (supportsClearAll) {
          contentIds.clear();
        } else {
          Logger.i(TAG, "CLEAR_ALL not supported.");
        }
        break;
      default:
        Logger.e(TAG, "unsupported operation: %s", streamStructure.getOperation());
    }
  }

  public void update(List<StreamStructure> streamStructures) {
    for (StreamStructure streamStructure : streamStructures) {
      update(streamStructure);
    }
  }

  @Override
  public void dump(Dumper dumper) {
    dumper.title(TAG);
    dumper.forKey("contentIds").value(contentIds.size());
  }
}
