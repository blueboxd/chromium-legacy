// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;

import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.RenderTestRule;

/**
 * A TestRule for creating Render Tests for Chrome.
 *
 * <pre>
 * {@code
 *
 * @RunWith(ChromeJUnit4ClassRunner.class)
 * @CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
 * public class MyTest {
 *     // Provide RenderTestRule with the path from src/ to the golden directory.
 *     @Rule
 *     public ChromeRenderTestRule mRenderTestRule = new ChromeRenderTestRule();
 *
 *     @Test
 *     // The test must have the feature "RenderTest" for the bots to display renders.
 *     @Feature({"RenderTest"})
 *     public void testViewAppearance() {
 *         // Setup the UI.
 *         ...
 *
 *         // Render UI Elements.
 *         mRenderTestRule.render(bigWidgetView, "big_widget");
 *         mRenderTestRule.render(smallWidgetView, "small_widget");
 *     }
 * }
 *
 * }
 * </pre>
 */
public class ChromeRenderTestRule extends RenderTestRule {
    /**
     * Constructor using {@code "chrome/test/data/android/render_tests"} as default golden folder.
     */
    public ChromeRenderTestRule() {
        super("chrome/test/data/android/render_tests");
    }

    /**
     * Searches the View hierarchy and modifies the Views to provide better stability in tests. For
     * example it will disable the blinking cursor in EditTexts.
     */
    public static void sanitize(View view) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Add more sanitizations as we discover more flaky attributes.
            if (view instanceof ViewGroup) {
                ViewGroup viewGroup = (ViewGroup) view;
                for (int i = 0; i < viewGroup.getChildCount(); i++) {
                    sanitize(viewGroup.getChildAt(i));
                }
            } else if (view instanceof EditText) {
                EditText editText = (EditText) view;
                editText.setCursorVisible(false);
            }
        });
    }
}
