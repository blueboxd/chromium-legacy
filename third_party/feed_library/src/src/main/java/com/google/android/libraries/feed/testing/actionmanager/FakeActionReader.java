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

package com.google.android.libraries.feed.testing.actionmanager;

import com.google.android.libraries.feed.api.internal.actionmanager.ActionReader;
import com.google.android.libraries.feed.api.internal.common.DismissActionWithSemanticProperties;
import com.google.android.libraries.feed.common.Result;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** Fake implementation of {@link ActionReader}. */
public final class FakeActionReader implements ActionReader {
  private final ArrayList<DismissActionWithSemanticProperties> dismissActions = new ArrayList<>();

  @Override
  public Result<List<DismissActionWithSemanticProperties>>
      getDismissActionsWithSemanticProperties() {
    return Result.success(dismissActions);
  }

  /** Adds a dismiss action with semantic properties. */
  public FakeActionReader addDismissActionsWithSemanticProperties(
      DismissActionWithSemanticProperties... dismissActionsToAdd) {
    Collections.addAll(dismissActions, dismissActionsToAdd);
    return this;
  }
}
