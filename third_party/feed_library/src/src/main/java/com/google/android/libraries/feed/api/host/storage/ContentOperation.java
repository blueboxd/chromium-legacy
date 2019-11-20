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

package com.google.android.libraries.feed.api.host.storage;

import android.support.annotation.IntDef;
import java.util.Arrays;

/** A mutation to the underlying {@link ContentStorage}. */
public abstract class ContentOperation {
  /** The types of operations. */
  @IntDef({Type.DELETE, Type.DELETE_BY_PREFIX, Type.UPSERT, Type.DELETE_ALL})
  public @interface Type {
    /** Delete the content for the provided key */
    int DELETE = 0;
    /** Delete all content with keys beginning with the specified prefix */
    int DELETE_BY_PREFIX = 1;
    /** Insert or update the content for the provided key */
    int UPSERT = 3;
    /** Delete all content for all keys. */
    int DELETE_ALL = 4;
  }

  public @Type int getType() {
    return type;
  }

  private final @Type int type;

  // Only the following classes may extend ContentOperation
  private ContentOperation(@Type int type) {
    this.type = type;
  }

  /**
   * A {@link ContentOperation} created by calling {@link ContentMutation.Builder#upsert(String,
   * byte[])}.
   */
  public static final class Upsert extends ContentOperation {
    private final String key;
    private final byte[] value;

    Upsert(String key, byte[] value) {
      super(Type.UPSERT);
      this.key = key;
      this.value = value;
    }

    public String getKey() {
      return key;
    }

    public byte[] getValue() {
      return value;
    }

    @Override
    public boolean equals(/*@Nullable*/ Object o) {
      if (!super.equals(o)) {
        return false;
      }

      if (o instanceof Upsert) {
        Upsert upsert = (Upsert) o;
        return key.equals(upsert.key) && Arrays.equals(value, upsert.value);
      } else {
        return false;
      }
    }

    @Override
    public int hashCode() {
      int result = key.hashCode();
      result = 31 * result + Arrays.hashCode(value);
      return result;
    }
  }

  /**
   * A {@link ContentOperation} created by calling {@link ContentMutation.Builder#delete(String)}.
   */
  public static final class Delete extends ContentOperation {
    private final String key;

    Delete(String key) {
      super(Type.DELETE);
      this.key = key;
    }

    public String getKey() {
      return key;
    }

    @Override
    public boolean equals(/*@Nullable*/ Object o) {
      if (!super.equals(o)) {
        return false;
      }

      if (o instanceof Delete) {
        Delete delete = (Delete) o;
        return key.equals(delete.key);
      } else {
        return false;
      }
    }

    @Override
    public int hashCode() {
      return key.hashCode();
    }
  }

  /**
   * A {@link ContentOperation} created by calling {@link
   * ContentMutation.Builder#deleteByPrefix(String)}.
   */
  public static final class DeleteByPrefix extends ContentOperation {
    private final String prefix;

    DeleteByPrefix(String prefix) {
      super(Type.DELETE_BY_PREFIX);
      this.prefix = prefix;
    }

    public String getPrefix() {
      return prefix;
    }

    @Override
    public boolean equals(/*@Nullable*/ Object o) {
      if (!super.equals(o)) {
        return false;
      }

      if (o instanceof DeleteByPrefix) {
        DeleteByPrefix that = (DeleteByPrefix) o;
        return prefix.equals(that.prefix);
      } else {
        return false;
      }
    }

    @Override
    public int hashCode() {
      return prefix.hashCode();
    }
  }

  // TODO: replace these comments with better details
  /** A {@link ContentOperation} created by calling {@link ContentMutation.Builder#deleteAll()} */
  static final class DeleteAll extends ContentOperation {
    DeleteAll() {
      super(Type.DELETE_ALL);
    }
  }

  @Override
  public boolean equals(/*@Nullable*/ Object o) {
    if (this == o) {
      return true;
    }
    if (!(o instanceof ContentOperation)) {
      return false;
    }

    ContentOperation operation = (ContentOperation) o;

    return type == operation.type;
  }

  @Override
  public int hashCode() {
    return type;
  }
}
