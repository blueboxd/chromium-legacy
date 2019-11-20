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

import android.app.AlertDialog;
import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.os.Build.VERSION;
import android.view.View;
import android.view.ViewParent;
import android.widget.ArrayAdapter;
import android.widget.ListView;
import java.util.List;

/**
 * {@link ContextMenuManager} implementation to show a floating context menu that is positioned
 * without regard to the anchor view.
 */
public class FloatingContextMenuManager implements ContextMenuManager {

  private final Context context;
  /*@Nullable*/ private AlertDialog alertDialog;

  public FloatingContextMenuManager(Context context) {
    this.context = context;
  }

  @Override
  public boolean openContextMenu(
      View anchorView, List<String> items, ContextMenuClickHandler handler) {
    if (menuShowing()) {
      return false;
    }

    ArrayAdapter<String> adapter =
        new ArrayAdapter<>(context, R.layout.feed_simple_list_item, items);

    ListView listView = createListView(adapter, context);
    AlertDialog.Builder dialogBuilder = new AlertDialog.Builder(context);

    dialogBuilder.setView(listView);
    AlertDialog alertDialog = dialogBuilder.create();
    listView.setOnItemClickListener(
        (parent, view, position, id) -> {
          handler.handleClick(position);
          dismissPopup();
        });

    ViewParent parent = anchorView.getParent();
    if (parent != null) {
      parent.requestDisallowInterceptTouchEvent(true);
    }
    alertDialog.show();

    this.alertDialog = alertDialog;
    return true;
  }

  private ListView createListView(ArrayAdapter<String> adapter, Context context) {

    ListView listView = new ListView(context);
    listView.setAdapter(adapter);

    // In API 21, 22, and 23 (Lollipop and Marshmallow), there aren't any dividers in the system
    // default context menu. However, in KitKat there are dividers, which we replicate here.
    if (VERSION.SDK_INT < 21) {
      listView.setDivider(new ColorDrawable(Color.GRAY));
      listView.setDividerHeight(1);
    } else {
      listView.setDivider(null);
      listView.setDividerHeight(0);
    }

    return listView;
  }

  @Override
  public void dismissPopup() {
    if (alertDialog != null) {
      alertDialog.dismiss();
      alertDialog = null;
    }
  }

  @Override
  public void setView(View view) {
    // No-op.
  }

  private boolean menuShowing() {
    return alertDialog != null && alertDialog.isShowing();
  }
}
