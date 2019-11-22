// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.google.android.libraries.feed.sharedstream.contextmenumanager;

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

import com.google.android.libraries.feed.sharedstream.contextmenumanager.ContextMenuManager.ContextMenuClickHandler;
import com.google.android.libraries.feed.sharedstream.publicapi.menumeasurer.MenuMeasurer;
import com.google.android.libraries.feed.sharedstream.publicapi.menumeasurer.Size;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatchers;
import org.mockito.Mock;
import org.robolectric.Robolectric;
import org.robolectric.RobolectricTestRunner;

import java.util.ArrayList;
import java.util.List;

/** Tests for {@link ContextMenuManagerImpl}. */
@RunWith(RobolectricTestRunner.class)
public class ContextMenuManagerImplTest {
    private static final int NO_ID = -1;

    @Mock
    private MenuMeasurer menuMeasurer;
    @Mock
    private ContextMenuClickHandler clickHandler;

    private Activity context;
    private ContextMenuManagerImpl contextMenuManager;
    private FrameLayout parentView;
    private View anchorView;
    private List<String> adapterItems;

    @Before
    public void setup() {
        initMocks(this);

        context = Robolectric.buildActivity(Activity.class).get();
        context.setTheme(R.style.Light);
        parentView = new FrameLayout(context);
        anchorView = new View(context);
        parentView.addView(anchorView);
        contextMenuManager = new ContextMenuManagerImpl(menuMeasurer, context);
        contextMenuManager.setView(parentView);
        adapterItems = createAdapterItems();
        when(menuMeasurer.measureAdapterContent(any(ViewGroup.class),
                     ArgumentMatchers.<ArrayAdapter<? extends Object>>any(), anyInt(), anyInt(),
                     anyInt()))
                .thenReturn(new Size(1, 2));
    }

    @Test
    public void testGetHeight() {
        parentView.measure(MeasureSpec.UNSPECIFIED, MeasureSpec.UNSPECIFIED);

        int top = 25;
        int bottom = 150;
        parentView.layout(0, top, 0, bottom);

        assertThat(contextMenuManager.getStreamHeight()).isEqualTo(bottom - top);
    }

    @Test
    public void testOpenContextMenu() {
        contextMenuManager.openContextMenu(anchorView, adapterItems, clickHandler);
        assertThat(shadowOf(parentView).getDisallowInterceptTouchEvent()).isTrue();

        PopupWindow popupWindow = shadowOf(context.getApplication()).getLatestPopupWindow();
        ListView listView = (ListView) popupWindow.getContentView();

        assertThat(listView.getAdapter().getItem(0)).isSameInstanceAs(adapterItems.get(0));
        assertThat(listView.getAdapter().getItem(1)).isSameInstanceAs(adapterItems.get(1));
        assertThat(listView.getAdapter().getItem(2)).isSameInstanceAs(adapterItems.get(2));

        assertThat(popupWindow.isShowing()).isTrue();
        listView.performItemClick(null, /* position= */ 1, NO_ID);

        verify(clickHandler).handleClick(1);
        assertThat(popupWindow.isShowing()).isFalse();
    }

    @Test
    public void testOnlyOpensOneMenu() {
        // Opens first context menu.
        assertThat(contextMenuManager.openContextMenu(anchorView, adapterItems, clickHandler))
                .isTrue();

        // Won't open a second one while the first one is open.
        assertThat(contextMenuManager.openContextMenu(anchorView, adapterItems, clickHandler))
                .isFalse();

        shadowOf(context.getApplication()).getLatestPopupWindow().dismiss();

        // After the menu is dismissed another can be opened
        assertThat(contextMenuManager.openContextMenu(anchorView, adapterItems, clickHandler))
                .isTrue();
    }

    @Test
    public void testDismiss_fromLockingPhone() {
        contextMenuManager.openContextMenu(anchorView, adapterItems, clickHandler);

        assertThat(shadowOf(context.getApplication()).getLatestPopupWindow().isShowing()).isTrue();

        contextMenuManager.dismissPopup();

        assertThat(shadowOf(context.getApplication()).getLatestPopupWindow().isShowing()).isFalse();
    }

    @Test
    public void testClosesMenuWhenDimensionsChange() {
        contextMenuManager.openContextMenu(anchorView, adapterItems, clickHandler);

        PopupWindow popupWindow = shadowOf(context.getApplication()).getLatestPopupWindow();
        assertThat(popupWindow.isShowing()).isTrue();

        contextMenuManager.dismissPopup();

        assertThat(popupWindow.isShowing()).isFalse();
    }

    @Test
    public void testOpenContextMenu_hasShadow() {
        contextMenuManager.openContextMenu(anchorView, adapterItems, clickHandler);

        PopupWindow popupWindow = shadowOf(context.getApplication()).getLatestPopupWindow();
        assertThat(popupWindow.getBackground()).isNotNull();
    }

    private List<String> createAdapterItems() {
        List<String> adapter = new ArrayList<>();
        for (int i = 0; i < 3; i++) {
            adapter.add(String.valueOf(i));
        }
        return adapter;
    }
}
