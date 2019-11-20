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

package com.google.android.libraries.feed.testing.android;

import static com.google.android.libraries.feed.common.Validators.checkNotNull;

import android.content.Context;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.view.View;
import java.util.HashMap;
import java.util.Map;

/** A {@link LinearLayoutManager} used for testing. */
public final class LinearLayoutManagerForTest extends LinearLayoutManager {

  private final Map<Integer, View> childMap;

  public int firstVisibleItemPosition = RecyclerView.NO_POSITION;
  public int firstVisibleViewOffset;

  public int lastVisibleItemPosition = RecyclerView.NO_POSITION;

  public int scrolledPosition;
  public int scrolledOffset;

  public LinearLayoutManagerForTest(Context context) {
    super(context);
    this.childMap = new HashMap<>();
  }

  @Override
  public int findFirstVisibleItemPosition() {
    return firstVisibleItemPosition;
  }

  @Override
  public int findLastVisibleItemPosition() {
    return lastVisibleItemPosition;
  }

  @Override
  public View findViewByPosition(int position) {
    if (childMap.containsKey(position)) {
      return checkNotNull(
          childMap.get(position),
          "addChildToPosition(int position, View child) should be called prior to "
              + "findViewByPosition(int position).");
    }
    return super.findViewByPosition(position);
  }

  @Override
  public void scrollToPositionWithOffset(int scrollPosition, int scrollOffset) {
    scrolledPosition = scrollPosition;
    scrolledOffset = scrollOffset;
  }

  public void addChildToPosition(int position, View child) {
    childMap.put(position, child);
  }
}
