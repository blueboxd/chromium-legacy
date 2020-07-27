// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SSL_CLIENT_CERTIFICATE_SELECTOR_TEST_H_
#define CHROME_BROWSER_SSL_SSL_CLIENT_CERTIFICATE_SELECTOR_TEST_H_

#include <memory>

#include "chrome/browser/ssl/ssl_client_auth_requestor_mock.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

class SSLClientCertificateSelectorTestBase : public InProcessBrowserTest {
 public:
  SSLClientCertificateSelectorTestBase();
  ~SSLClientCertificateSelectorTestBase() override;

  // InProcessBrowserTest:
  void SetUpInProcessBrowserTestFixture() override;
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

 protected:
  scoped_refptr<net::SSLCertRequestInfo> cert_request_info_;
  scoped_refptr<testing::StrictMock<SSLClientAuthRequestorMock> >
      auth_requestor_;
};

#endif  // CHROME_BROWSER_SSL_SSL_CLIENT_CERTIFICATE_SELECTOR_TEST_H_
