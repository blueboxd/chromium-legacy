// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.host.action;

/**
 * The ActionApi combines the {@link ActionPeformerApi} and the {@link ActionEnabledApi} to allow
 * the Feed to query the host as to what actions can be performed and instruct the host to perform
 * such actions.
 */
public interface ActionApi extends ActionEnabledApi, ActionPeformerApi {}
