// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/web_test/web_test_url_request_context_getter.h"

#include <utility>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/shell/browser/shell_network_delegate.h"
#include "content/shell/common/web_test/web_test_switches.h"
#include "net/cert/cert_verifier.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "services/network/ignore_errors_cert_verifier.h"

namespace content {

WebTestURLRequestContextGetter::WebTestURLRequestContextGetter(
    bool ignore_certificate_errors,
    bool off_the_record,
    const base::FilePath& base_path,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    ProtocolHandlerMap* protocol_handlers,
    URLRequestInterceptorScopedVector request_interceptors)
    : ShellURLRequestContextGetter(ignore_certificate_errors,
                                   off_the_record,
                                   base_path,
                                   std::move(io_task_runner),
                                   protocol_handlers,
                                   std::move(request_interceptors)) {
  // Must first be created on the UI thread.
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

WebTestURLRequestContextGetter::~WebTestURLRequestContextGetter() {}

std::unique_ptr<net::NetworkDelegate>
WebTestURLRequestContextGetter::CreateNetworkDelegate() {
  ShellNetworkDelegate::SetBlockThirdPartyCookies(false);
  return base::WrapUnique(new ShellNetworkDelegate);
}

std::unique_ptr<net::CertVerifier>
WebTestURLRequestContextGetter::GetCertVerifier() {
  return network::IgnoreErrorsCertVerifier::MaybeWrapCertVerifier(
      *base::CommandLine::ForCurrentProcess(), switches::kRunWebTests,
      net::CertVerifier::CreateDefault(/*cert_net_fetcher=*/nullptr));
}

std::unique_ptr<net::ProxyConfigService>
WebTestURLRequestContextGetter::GetProxyConfigService() {
  return nullptr;
}

std::unique_ptr<net::ProxyResolutionService>
WebTestURLRequestContextGetter::GetProxyService() {
  return net::ProxyResolutionService::CreateDirect();
}

}  // namespace content
