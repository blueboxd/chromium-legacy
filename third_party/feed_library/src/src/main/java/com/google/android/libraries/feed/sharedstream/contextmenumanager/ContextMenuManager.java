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

package com.google.android.libraries.feed.sharedstream.contextmenumanager;

import android.view.View;
import java.util.List;

/** Allow for display of context menus. */
public interface ContextMenuManager {
  /**
   * Opens a context menu if there is currently no open context menu. Returns whether a menu was
   * opened.
   *
   * @param anchorView The {@link View} to position the menu by.
   * @param items The contents to display.
   * @param handler The {@link ContextMenuClickHandler} that handles the user clicking on an option.
   */
  boolean openContextMenu(View anchorView, List<String> items, ContextMenuClickHandler handler);

  /** Dismiss a popup if one is showing. */
  void dismissPopup();

  /**
   * Sets the root view of the window that the context menu is opening in. This, as well as the
   * anchor view that the click happens on, determines where the context menu opens if the context
   * menu is anchored.
   *
   * <p>Note: this is being changed to be settable after the fact for a future version of this
   * library that will use dependency injection.
   */
  void setView(View view);

  /** Notifies creator of the menu that a click has occurred. */
  interface ContextMenuClickHandler {
    void handleClick(int position);
  }
}
