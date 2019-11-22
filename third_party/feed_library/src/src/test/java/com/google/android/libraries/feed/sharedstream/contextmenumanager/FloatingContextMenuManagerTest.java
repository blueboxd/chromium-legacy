// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.sharedstream.contextmenumanager;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface.OnClickListener;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ListAdapter;
import android.widget.ListView;

import com.google.android.libraries.feed.sharedstream.contextmenumanager.ContextMenuManager.ContextMenuClickHandler;
import com.google.common.collect.ImmutableList;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.shadows.ShadowAlertDialog;

/** Tests for {@link ContextMenuManagerImpl}. */
@RunWith(RobolectricTestRunner.class)
public class FloatingContextMenuManagerTest {
    private static final ImmutableList<String> ITEMS = ImmutableList.of("1", "2", "3");

    private Context context;
    private FloatingContextMenuManager contextMenuManager;
    @Mock
    private ContextMenuClickHandler handler;
    private View anchorView;

    @Before
    public void createContextMenuManager() {
        initMocks(this);
        context = Robolectric.buildActivity(Activity.class).get();
        context.setTheme(R.style.Light);
        contextMenuManager = new FloatingContextMenuManager(context);
        anchorView = new View(context);
    }

    @Test
    public void openContextMenu_opensContextMenu() {
        contextMenuManager.openContextMenu(anchorView, ITEMS, handler);

        assertThat(ShadowAlertDialog.getLatestAlertDialog()).isNotNull();
    }

    @Test
    public void performClick_triggersClickHandler() {
        contextMenuManager.openContextMenu(anchorView, ITEMS, handler);

        performClick(0);

        verify(handler).handleClick(0);
    }

    @Test
    public void performClick_dismissesMenu() {
        contextMenuManager.openContextMenu(anchorView, ITEMS, handler);

        // Should dismiss the menu.
        performClick(1);

        // Should open a new menu, as the old one has been dismissed.
        assertThat(contextMenuManager.openContextMenu(anchorView, ITEMS, handler)).isTrue();
    }

    @Test
    public void openContextMenu_multipleTimes_onlyOpensFirstMenu() {
        contextMenuManager.openContextMenu(anchorView, ITEMS, handler);

        // Fails to open as another menu is showing.
        assertThat(contextMenuManager.openContextMenu(anchorView, ImmutableList.of(), handler))
                .isFalse();
    }

    @Test
    public void openContextMenu_disallowsInterceptTouchEvent() {
        FrameLayout parent = new FrameLayout(context);
        parent.addView(anchorView);

        contextMenuManager.openContextMenu(anchorView, ITEMS, handler);

        assertThat(shadowOf(parent).getDisallowInterceptTouchEvent()).isTrue();
    }

    @Test
    public void dismissPopup_dismissesPopup() {
        contextMenuManager.openContextMenu(anchorView, ITEMS, handler);

        contextMenuManager.dismissPopup();

        assertThat(contextMenuManager.openContextMenu(anchorView, ITEMS, handler)).isTrue();
    }

    /**
     * Utility method to perform click on an index. Using the normal methods didn't seem to work,
     * probably because we set the {@link ListView} via {@link AlertDialog.Builder#setView(View)}
     * instead of {@link AlertDialog.Builder#setAdapter(ListAdapter, OnClickListener)} so it doesn't
     * know that the view is a {@link ListView}
     */
    private void performClick(int index) {
        AlertDialog dialog = ShadowAlertDialog.getLatestAlertDialog();

        // Can't just use shadowAlertDialog.getListView, which returns null, as does performing the
        // click on the shadow.
        ListView listView = (ListView) shadowOf(dialog).getView();
        listView.performItemClick(/* view= */ null, index, /* id= */ -1);
    }
}
