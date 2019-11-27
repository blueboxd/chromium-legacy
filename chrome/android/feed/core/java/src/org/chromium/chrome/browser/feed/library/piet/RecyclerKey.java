// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet;

/**
 * Key for a {@link RecyclerPool}; objects that can be bound to the same model have an equal key.
 * Extend and override hashCode and equals for more granular specification of which objects are
 * compatible for recycling. (ex. {@link TextElementAdapter} is only compatible with other adapters
 * that have the same size, font weight, and italic)
 */
class RecyclerKey {}
