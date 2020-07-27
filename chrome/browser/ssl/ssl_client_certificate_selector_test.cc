// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/ssl_client_certificate_selector_test.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/request_priority.h"
#include "net/http/http_transaction_factory.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"

using ::testing::Mock;
using ::testing::StrictMock;

SSLClientCertificateSelectorTestBase::SSLClientCertificateSelectorTestBase() =
    default;

SSLClientCertificateSelectorTestBase::~SSLClientCertificateSelectorTestBase() =
    default;

void SSLClientCertificateSelectorTestBase::SetUpInProcessBrowserTestFixture() {
  cert_request_info_ = base::MakeRefCounted<net::SSLCertRequestInfo>();
  cert_request_info_->host_and_port = net::HostPortPair("foo", 123);
}

void SSLClientCertificateSelectorTestBase::SetUpOnMainThread() {
  auth_requestor_ =
      new StrictMock<SSLClientAuthRequestorMock>(cert_request_info_.get());

  EXPECT_TRUE(content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents()));
}

// Have to release our reference to the auth handler during the test to allow
// it to be destroyed while the Browser still exists.
void SSLClientCertificateSelectorTestBase::TearDownOnMainThread() {
  auth_requestor_.reset();
}
