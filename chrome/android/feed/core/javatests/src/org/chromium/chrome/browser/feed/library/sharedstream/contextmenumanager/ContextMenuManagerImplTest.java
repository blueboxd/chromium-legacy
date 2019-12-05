// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.sharedstream.contextmenumanager;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.MockitoAnnotations.initMocks;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.FrameLayout;
import android.widget.ListView;
import android.widget.PopupWindow;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatchers;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.sharedstream.contextmenumanager.ContextMenuManager.ContextMenuClickHandler;
import org.chromium.chrome.browser.feed.library.sharedstream.publicapi.menumeasurer.MenuMeasurer;
import org.chromium.chrome.browser.feed.library.sharedstream.publicapi.menumeasurer.Size;
import org.chromium.testing.local.LocalRobolectricTestRunner;

import java.util.ArrayList;
import java.util.List;

/** Tests for {@link ContextMenuManagerImpl}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ContextMenuManagerImplTest {
    private static final int NO_ID = -1;

    @Mock
    private MenuMeasurer mMenuMeasurer;
    @Mock
    private ContextMenuClickHandler mClickHandler;

    private Activity mContext;
    private ContextMenuManagerImpl mContextMenuManager;
    private FrameLayout mParentView;
    private View mAnchorView;
    private List<String> mAdapterItems;

    @Before
    public void setup() {
        initMocks(this);

        mContext = Robolectric.buildActivity(Activity.class).get();
        mContext.setTheme(R.style.Light);
        mParentView = new FrameLayout(mContext);
        mAnchorView = new View(mContext);
        mParentView.addView(mAnchorView);
        mContextMenuManager = new ContextMenuManagerImpl(mMenuMeasurer, mContext);
        mContextMenuManager.setView(mParentView);
        mAdapterItems = mCreateAdapterItems();
        when(mMenuMeasurer.measureAdapterContent(any(ViewGroup.class),
                     ArgumentMatchers.<ArrayAdapter<? extends Object>>any(), anyInt(), anyInt(),
                     anyInt()))
                .thenReturn(new Size(1, 2));
    }

    @Test
    public void testGetHeight() {
        mParentView.measure(MeasureSpec.UNSPECIFIED, MeasureSpec.UNSPECIFIED);

        int top = 25;
        int bottom = 150;
        mParentView.layout(0, top, 0, bottom);

        assertThat(mContextMenuManager.getStreamHeight()).isEqualTo(bottom - top);
    }

    @Test
    public void testOpenContextMenu() {
        mContextMenuManager.openContextMenu(mAnchorView, mAdapterItems, mClickHandler);
        assertThat(shadowOf(mParentView).getDisallowInterceptTouchEvent()).isTrue();

        PopupWindow popupWindow = shadowOf(mContext.getApplication()).getLatestPopupWindow();
        ListView listView = (ListView) popupWindow.getContentView();

        assertThat(listView.getAdapter().getItem(0)).isSameInstanceAs(mAdapterItems.get(0));
        assertThat(listView.getAdapter().getItem(1)).isSameInstanceAs(mAdapterItems.get(1));
        assertThat(listView.getAdapter().getItem(2)).isSameInstanceAs(mAdapterItems.get(2));

        assertThat(popupWindow.isShowing()).isTrue();
        listView.performItemClick(null, /* position= */ 1, NO_ID);

        verify(mClickHandler).handleClick(1);
        assertThat(popupWindow.isShowing()).isFalse();
    }

    @Test
    public void testOnlyOpensOneMenu() {
        // Opens first context menu.
        assertThat(mContextMenuManager.openContextMenu(mAnchorView, mAdapterItems, mClickHandler))
                .isTrue();

        // Won't open a second one while the first one is open.
        assertThat(mContextMenuManager.openContextMenu(mAnchorView, mAdapterItems, mClickHandler))
                .isFalse();

        shadowOf(mContext.getApplication()).getLatestPopupWindow().dismiss();

        // After the menu is dismissed another can be opened
        assertThat(mContextMenuManager.openContextMenu(mAnchorView, mAdapterItems, mClickHandler))
                .isTrue();
    }

    @Test
    public void testDismiss_fromLockingPhone() {
        mContextMenuManager.openContextMenu(mAnchorView, mAdapterItems, mClickHandler);

        assertThat(shadowOf(mContext.getApplication()).getLatestPopupWindow().isShowing()).isTrue();

        mContextMenuManager.dismissPopup();

        assertThat(shadowOf(mContext.getApplication()).getLatestPopupWindow().isShowing())
                .isFalse();
    }

    @Test
    public void testClosesMenuWhenDimensionsChange() {
        mContextMenuManager.openContextMenu(mAnchorView, mAdapterItems, mClickHandler);

        PopupWindow popupWindow = shadowOf(mContext.getApplication()).getLatestPopupWindow();
        assertThat(popupWindow.isShowing()).isTrue();

        mContextMenuManager.dismissPopup();

        assertThat(popupWindow.isShowing()).isFalse();
    }

    @Test
    public void testOpenContextMenu_hasShadow() {
        mContextMenuManager.openContextMenu(mAnchorView, mAdapterItems, mClickHandler);

        PopupWindow popupWindow = shadowOf(mContext.getApplication()).getLatestPopupWindow();
        assertThat(popupWindow.getBackground()).isNotNull();
    }

    private List<String> mCreateAdapterItems() {
        List<String> adapter = new ArrayList<>();
        for (int i = 0; i < 3; i++) {
            adapter.add(String.valueOf(i));
        }
        return adapter;
    }
}
