// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

// TODO(http://crbug.com/1025606): Remove this file once all references in internal repos have been
// updated.
public class SSLClientCertificateRequest {
    public static void notifyClientCertificatesChangedOnIOThread() {
        org.chromium.components.browser_ui.client_certificate.SSLClientCertificateRequest
                .notifyClientCertificatesChangedOnIOThread();
    }
}
