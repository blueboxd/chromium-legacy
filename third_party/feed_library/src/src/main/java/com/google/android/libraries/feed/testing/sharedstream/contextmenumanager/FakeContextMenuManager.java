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

package com.google.android.libraries.feed.testing.sharedstream.contextmenumanager;

import static com.google.android.libraries.feed.common.Validators.checkNotNull;

import android.view.View;
import com.google.android.libraries.feed.sharedstream.contextmenumanager.ContextMenuManager;
import java.util.List;

/** Fake to be used to test {@link ContextMenuManager}. */
public class FakeContextMenuManager implements ContextMenuManager {

  private boolean isMenuOpened;
  /*@Nullable*/ private List<String> items;
  /*@Nullable*/ private ContextMenuClickHandler handler;

  @Override
  public boolean openContextMenu(
      View anchorView, List<String> items, ContextMenuClickHandler handler) {
    if (isMenuOpened) {
      return false;
    }

    isMenuOpened = true;
    this.items = items;
    this.handler = handler;

    return true;
  }

  @Override
  public void dismissPopup() {
    isMenuOpened = false;
    items = null;
    handler = null;
  }

  @Override
  public void setView(View view) {}

  public void performClick(int position) {
    if (!isMenuOpened) {
      throw new IllegalStateException("Cannot perform click with no menu opened.");
    }

    checkNotNull(handler).handleClick(position);
  }

  public List<String> getMenuOptions() {
    if (!isMenuOpened) {
      throw new IllegalStateException("No menu open.");
    }

    return checkNotNull(items);
  }
}
