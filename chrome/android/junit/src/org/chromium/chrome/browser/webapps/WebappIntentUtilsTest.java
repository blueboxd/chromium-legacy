// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;

import android.content.Intent;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ShortcutHelper;

/**
 * Tests for {@link WebappIntentUtils}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class WebappIntentUtilsTest {
    /**
     * Test that {@link WebappIntentUtils#copyWebappLaunchIntentExtras()} does not set intent
     * extras on the destination intent if they are not present in the source intent.
     */
    @Test
    public void testCopyWebappLaunchIntentExtrasMissingKeys() {
        Intent toIntent = new Intent();
        WebappIntentUtils.copyWebappLaunchIntentExtras(new Intent(), toIntent);
        assertFalse(toIntent.hasExtra(ShortcutHelper.EXTRA_NAME));
        assertFalse(toIntent.hasExtra(ShortcutHelper.EXTRA_IS_ICON_ADAPTIVE));
        assertFalse(toIntent.hasExtra(ShortcutHelper.EXTRA_DISPLAY_MODE));
        assertFalse(toIntent.hasExtra(ShortcutHelper.EXTRA_BACKGROUND_COLOR));
    }

    /**
     * Test that {@link WebappIntentUtils#copyWebappLaunchIntentExtras()} properly copies intent
     * extras of each type.
     */
    @Test
    public void testCopyWebappLaunchIntentExtras() {
        Intent fromIntent = new Intent();
        fromIntent.putExtra(ShortcutHelper.EXTRA_NAME, "name");
        fromIntent.putExtra(ShortcutHelper.EXTRA_IS_ICON_ADAPTIVE, false);
        fromIntent.putExtra(ShortcutHelper.EXTRA_IS_ICON_GENERATED, true);
        fromIntent.putExtra(ShortcutHelper.EXTRA_DISPLAY_MODE, 1);
        fromIntent.putExtra(ShortcutHelper.EXTRA_BACKGROUND_COLOR, 1L);

        Intent toIntent = new Intent();
        WebappIntentUtils.copyWebappLaunchIntentExtras(fromIntent, toIntent);
        assertEquals("name", toIntent.getStringExtra(ShortcutHelper.EXTRA_NAME));
        assertEquals(false, toIntent.getBooleanExtra(ShortcutHelper.EXTRA_IS_ICON_ADAPTIVE, true));
        assertEquals(true, toIntent.getBooleanExtra(ShortcutHelper.EXTRA_IS_ICON_GENERATED, false));
        assertEquals(1, toIntent.getIntExtra(ShortcutHelper.EXTRA_DISPLAY_MODE, 0));
        assertEquals(1L, toIntent.getLongExtra(ShortcutHelper.EXTRA_BACKGROUND_COLOR, 0L));
    }

    /**
     * Test that {@link WebappIntentUtils#copyWebappLaunchIntentExtras()} does not copy non white
     * listed intent extras.
     */
    @Test
    public void testCopyWebappLaunchIntentExtrasWhitelist() {
        final String extraKey = "not_in_whitelist";
        Intent fromIntent = new Intent();
        fromIntent.putExtra(extraKey, "random");
        Intent toIntent = new Intent();
        WebappIntentUtils.copyWebappLaunchIntentExtras(fromIntent, toIntent);
        assertNull(toIntent.getStringExtra(extraKey));
    }

    /**
     * Test that {@link WebappIntentUtils#copyWebappLaunchIntentExtras()} does not modify the source
     * intent.
     */
    @Test
    public void testCopyWebappLaunchIntentExtrasDoesNotModifyFromIntent() {
        final String notInWhitelistKey = "not_in_whitelist";
        Intent fromIntent = new Intent();
        fromIntent.putExtra(ShortcutHelper.EXTRA_NAME, "name");
        fromIntent.putExtra(notInWhitelistKey, "random");
        Intent toIntent = new Intent();
        WebappIntentUtils.copyWebappLaunchIntentExtras(fromIntent, toIntent);
        assertEquals("name", fromIntent.getStringExtra(ShortcutHelper.EXTRA_NAME));
        assertEquals("random", fromIntent.getStringExtra(notInWhitelistKey));
        assertEquals("name", toIntent.getStringExtra(ShortcutHelper.EXTRA_NAME));
    }
}
