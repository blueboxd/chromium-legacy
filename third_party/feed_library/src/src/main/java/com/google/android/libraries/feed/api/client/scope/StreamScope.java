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

import com.google.android.libraries.feed.api.client.stream.Stream;
import com.google.android.libraries.feed.api.internal.modelprovider.ModelProviderFactory;

/** Allows interacting with the Feed library on a per-stream level */
public interface StreamScope {

  /** Returns the current {@link Stream}. */
  Stream getStream();

  /** Returns the current {@link ModelProviderFactory}. */
  ModelProviderFactory getModelProviderFactory();
}
