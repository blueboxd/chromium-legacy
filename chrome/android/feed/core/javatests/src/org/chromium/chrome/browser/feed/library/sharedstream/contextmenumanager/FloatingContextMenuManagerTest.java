// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.sharedstream.contextmenumanager;

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

import com.google.common.collect.ImmutableList;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowAlertDialog;

import org.chromium.chrome.browser.feed.library.sharedstream.contextmenumanager.ContextMenuManager.ContextMenuClickHandler;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests for {@link ContextMenuManagerImpl}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FloatingContextMenuManagerTest {
    private static final ImmutableList<String> ITEMS = ImmutableList.of("1", "2", "3");

    private Context mContext;
    private FloatingContextMenuManager mContextMenuManager;
    @Mock
    private ContextMenuClickHandler mHandler;
    private View mAnchorView;

    @Before
    public void createContextMenuManager() {
        initMocks(this);
        mContext = Robolectric.buildActivity(Activity.class).get();
        mContext.setTheme(R.style.Light);
        mContextMenuManager = new FloatingContextMenuManager(mContext);
        mAnchorView = new View(mContext);
    }

    @Test
    public void openContextMenu_opensContextMenu() {
        mContextMenuManager.openContextMenu(mAnchorView, ITEMS, mHandler);

        assertThat(ShadowAlertDialog.getLatestAlertDialog()).isNotNull();
    }

    @Test
    public void performClick_triggersClickHandler() {
        mContextMenuManager.openContextMenu(mAnchorView, ITEMS, mHandler);

        performClick(0);

        verify(mHandler).handleClick(0);
    }

    @Test
    public void performClick_dismissesMenu() {
        mContextMenuManager.openContextMenu(mAnchorView, ITEMS, mHandler);

        // Should dismiss the menu.
        performClick(1);

        // Should open a new menu, as the old one has been dismissed.
        assertThat(mContextMenuManager.openContextMenu(mAnchorView, ITEMS, mHandler)).isTrue();
    }

    @Test
    public void openContextMenu_multipleTimes_onlyOpensFirstMenu() {
        mContextMenuManager.openContextMenu(mAnchorView, ITEMS, mHandler);

        // Fails to open as another menu is showing.
        assertThat(mContextMenuManager.openContextMenu(mAnchorView, ImmutableList.of(), mHandler))
                .isFalse();
    }

    @Test
    public void openContextMenu_disallowsInterceptTouchEvent() {
        FrameLayout parent = new FrameLayout(mContext);
        parent.addView(mAnchorView);

        mContextMenuManager.openContextMenu(mAnchorView, ITEMS, mHandler);

        assertThat(shadowOf(parent).getDisallowInterceptTouchEvent()).isTrue();
    }

    @Test
    public void dismissPopup_dismissesPopup() {
        mContextMenuManager.openContextMenu(mAnchorView, ITEMS, mHandler);

        mContextMenuManager.dismissPopup();

        assertThat(mContextMenuManager.openContextMenu(mAnchorView, ITEMS, mHandler)).isTrue();
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
