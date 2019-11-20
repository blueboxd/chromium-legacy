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

package com.google.android.libraries.feed.api.internal.common;

import com.google.search.now.wire.feed.ContentIdProto.ContentId;
import java.util.Arrays;

/**
 * Represents the content needed for a dismiss action internally. Holds the content ID of the
 * dismissed content, and any semantic properties associated with it.
 */
public final class DismissActionWithSemanticProperties {
  private final ContentId contentId;
  private final byte /*@Nullable*/ [] semanticProperties;

  public DismissActionWithSemanticProperties(
      ContentId contentId, byte /*@Nullable*/ [] semanticProperties) {
    this.contentId = contentId;
    this.semanticProperties = semanticProperties;
  }

  public ContentId getContentId() {
    return contentId;
  }

  public byte /*@Nullable*/ [] getSemanticProperties() {
    return semanticProperties;
  }

  @Override
  public boolean equals(/*@Nullable*/ Object o) {
    if (this == o) {
      return true;
    }
    if (o == null || getClass() != o.getClass()) {
      return false;
    }

    DismissActionWithSemanticProperties that = (DismissActionWithSemanticProperties) o;

    if (!contentId.equals(that.contentId)) {
      return false;
    }
    return Arrays.equals(semanticProperties, that.semanticProperties);
  }

  @Override
  public int hashCode() {
    int result = contentId.hashCode();
    result = 31 * result + Arrays.hashCode(semanticProperties);
    return result;
  }
}
