// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.services.gcm;

import android.os.Bundle;

import com.google.ipc.invalidation.ticl.android2.channel.GcmUpstreamSenderService;

/**
 * Sends Upstream messages for Invalidations using GCM.
 */
public class InvalidationGcmUpstreamSender extends GcmUpstreamSenderService {
    @Override
    public void deliverMessage(final String to, final Bundle data) {}
}
