// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.view.View;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.widget.ScrimView;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Tests for {@link TabGridDialogMediator}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabGridIphDialogMediatorUnitTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock
    View mView;

    private PropertyModel mModel;
    private TabGridIphDialogMediator mMediator;

    @Before
    public void setUp() {
        RecordHistogram.setDisabledForTests(true);

        MockitoAnnotations.initMocks(this);

        mModel = new PropertyModel(TabGridIphDialogProperties.ALL_KEYS);
        mMediator = new TabGridIphDialogMediator(mModel);
    }

    @After
    public void tearDown() {
        RecordHistogram.setDisabledForTests(false);
    }

    @Test
    public void testCloseIphDialogButtonListener() {
        View.OnClickListener listener =
                mModel.get(TabGridIphDialogProperties.CLOSE_BUTTON_LISTENER);
        assertNotNull(listener);
        mModel.set(TabGridIphDialogProperties.IS_VISIBLE, true);

        listener.onClick(mView);
        assertFalse(mModel.get(TabGridIphDialogProperties.IS_VISIBLE));
    }

    @Test
    public void testScrimViewObserver() {
        ScrimView.ScrimObserver observer =
                mModel.get(TabGridIphDialogProperties.SCRIM_VIEW_OBSERVER);
        assertNotNull(observer);
        mModel.set(TabGridIphDialogProperties.IS_VISIBLE, true);

        observer.onScrimClick();
        assertFalse(mModel.get(TabGridIphDialogProperties.IS_VISIBLE));
    }

    @Test
    public void testShowIph() {
        mModel.set(TabGridIphDialogProperties.IS_VISIBLE, false);

        mMediator.showIph();
        assertTrue(mModel.get(TabGridIphDialogProperties.IS_VISIBLE));
    }
}
