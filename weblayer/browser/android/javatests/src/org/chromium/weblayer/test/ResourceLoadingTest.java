// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.content.Context;
import android.graphics.drawable.ColorDrawable;
import android.support.test.filters.SmallTest;
import android.util.TypedValue;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.weblayer.TestWebLayer;
import org.chromium.weblayer.shell.InstrumentationActivity;

/** Tests that resources are loaded correctly. */
@RunWith(WebLayerJUnit4ClassRunner.class)
public final class ResourceLoadingTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private Context mRemoteContext;

    @Before
    public void setUp() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl("about:blank");
        mRemoteContext = TestWebLayer.getRemoteContext(activity);
    }

    @Test
    @SmallTest
    public void testLayout() throws Exception {
        Context themedContext =
                new ContextThemeWrapper(mRemoteContext, getIdentifier("style/TestStyle"));
        View view = LayoutInflater.from(themedContext)
                            .inflate(getIdentifier("layout/test_layout"), null);
        Assert.assertEquals(((ColorDrawable) view.getBackground()).getColor(), 0xff010101);
    }

    @Test
    @SmallTest
    public void testStyle() throws Exception {
        Context themedContext =
                new ContextThemeWrapper(mRemoteContext, getIdentifier("style/TestStyle"));
        TypedValue tv = new TypedValue();
        Assert.assertTrue(themedContext.getTheme().resolveAttribute(
                getIdentifier("attr/testAttrColor"), tv, true));
        Assert.assertEquals(tv.type, TypedValue.TYPE_INT_COLOR_RGB8);
        Assert.assertEquals(tv.data, 0xff010101);
    }

    private int getIdentifier(String name) {
        int id = mRemoteContext.getResources().getIdentifier(
                name, null, mRemoteContext.getPackageName());
        // Force the returned ID to use our magic package ID.
        id &= 0x00ffffff;
        id |= 0x0c000000;
        return id;
    }
}
