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

package com.google.android.libraries.feed.common.intern;

/**
 * Similar to {@link String#intern} but for other immutable types. An interner will de-duplicate
 * objects that are identical so only one copy is stored in memory.
 */
public interface Interner<T> {

  /**
   * Returns a canonical representation for the give input object. If there is already an internal
   * object equal to the input object, then the internal object is returned. Otherwise, the input
   * object is added internally and a reference to it is returned.
   */
  T intern(T input);

  /** Clears the internally store objects. */
  void clear();

  /** Returns the number of the internally stored objects. */
  int size();
}
