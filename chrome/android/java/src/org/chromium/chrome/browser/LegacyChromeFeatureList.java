// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import org.chromium.chrome.browser.flags.ChromeFeatureList;

/**
 * Temporary class, identical to {@link ChromeFeatureList}, for migration.
 *
 * This class will be referenced temporarily while ChromeFeatureList is moved to the .flags
 * package.
 *
 * TODO(crbug.com/1041468): Remove this class after downstream uses flags.ChromeFeatureList.
 */
public abstract class LegacyChromeFeatureList extends ChromeFeatureList {
}
