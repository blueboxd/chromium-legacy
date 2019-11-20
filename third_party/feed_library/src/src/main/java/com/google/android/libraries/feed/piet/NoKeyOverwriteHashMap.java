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

package com.google.android.libraries.feed.piet;

import com.google.search.now.ui.piet.ErrorsProto.ErrorCode;
import java.util.HashMap;

/** Map that throws if you try to insert a second key with the same value. */
public class NoKeyOverwriteHashMap<K, V> extends HashMap<K, V> {

  /** A term for the items this map contains (ex. "Style" or "Template"); used in debug logs. */
  private final String termForContentValue;

  private final ErrorCode errorCodeForDuplicate;

  NoKeyOverwriteHashMap(String termForContentValue, ErrorCode errorCodeForDuplicate) {
    this.termForContentValue = termForContentValue;
    this.errorCodeForDuplicate = errorCodeForDuplicate;
  }

  @Override
  /*@Nullable*/
  public V put(K key, V value) {
    if (containsKey(key)) {
      throw new PietFatalException(
          errorCodeForDuplicate,
          String.format("%s key '%s' already defined", termForContentValue, key));
    }
    return super.put(key, value);
  }
}
