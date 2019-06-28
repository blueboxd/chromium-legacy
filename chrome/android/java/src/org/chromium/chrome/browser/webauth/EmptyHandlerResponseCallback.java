// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauth;

import org.chromium.blink.mojom.GetAssertionAuthenticatorResponse;
import org.chromium.blink.mojom.MakeCredentialAuthenticatorResponse;

/**
 * Empty HandlerResponseCallback Temporarily used for landing CLs for
 * IsUserVerifyingPlatformAuthenticatorAvailable error response.
 */
public class EmptyHandlerResponseCallback implements HandlerResponseCallback {
    @Override
    public void onRegisterResponse(Integer status, MakeCredentialAuthenticatorResponse response){};

    @Override
    public void onSignResponse(Integer status, GetAssertionAuthenticatorResponse response){};

    @Override
    public void onIsUserVerifyingPlatformAuthenticatorAvailableResponse(boolean isUVPAA){};

    @Override
    public void onError(Integer status){};
}
