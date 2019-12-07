// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CERT_VERIFIER_CERT_NET_FETCHER_URL_LOADER_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CERT_VERIFIER_CERT_NET_FETCHER_URL_LOADER_H_

#include "base/component_export.h"
#include "base/memory/ref_counted.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/cert/cert_net_fetcher.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace base {
class SequencedTaskRunner;
}

namespace cert_verifier {

// A CertNetFetcher that issues requests through the provided
// URLLoaderFactory. The CertNetFetcher MUST be Shutdown on the same thread it
// was created on, prior to destruction, and the actual fetching will be done on
// that thread. The CertNetFetcher's Fetch methods are to be used on a
// *different* thread, since it gives a blocking interface to URL fetching.
class COMPONENT_EXPORT(CERT_VERIFIER_CPP) CertNetFetcherURLLoader
    : public net::CertNetFetcher {
 public:
  class AsyncCertNetFetcherURLLoader;
  class RequestCore;
  struct RequestParams;

  // Creates the CertNetFetcherURLLoader, using the provided URLLoaderFactory.
  // If the other side of the remote disconnects, this CertNetFetcherURLLoader
  // will call Shutdown().
  explicit CertNetFetcherURLLoader(
      mojo::PendingRemote<network::mojom::URLLoaderFactory> factory);

  // Returns the default timeout value. Intended for test use only.
  static base::TimeDelta GetDefaultTimeoutForTesting();

  // CertNetFetcher impl:
  void Shutdown() override;
  std::unique_ptr<Request> FetchCaIssuers(
      const GURL& url,
      const net::NetworkIsolationKey& network_isolation_key,
      int timeout_milliseconds,
      int max_response_bytes) override;
  std::unique_ptr<Request> FetchCrl(
      const GURL& url,
      const net::NetworkIsolationKey& network_isolation_key,
      int timeout_milliseconds,
      int max_response_bytes) override;
  WARN_UNUSED_RESULT std::unique_ptr<Request> FetchOcsp(
      const GURL& url,
      const net::NetworkIsolationKey& network_isolation_key,
      int timeout_milliseconds,
      int max_response_bytes) override;

 private:
  ~CertNetFetcherURLLoader() override;

  void DoFetchOnTaskRunner(std::unique_ptr<RequestParams> request_params,
                           scoped_refptr<RequestCore> request);

  std::unique_ptr<Request> DoFetch(
      std::unique_ptr<RequestParams> request_params);

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  // |factory_| should stay valid until Shutdown() is called. If it disconnects,
  // the CertNetFetcherURLLoader will automatically shutdown and cancel all
  // outstanding requests.
  mojo::Remote<network::mojom::URLLoaderFactory> factory_;
  std::unique_ptr<AsyncCertNetFetcherURLLoader> impl_;
};

}  // namespace cert_verifier

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CERT_VERIFIER_CERT_NET_FETCHER_URL_LOADER_H_
