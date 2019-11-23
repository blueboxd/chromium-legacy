// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CERT_VERIFIER_CERT_NET_FETCHER_TEST_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CERT_VERIFIER_CERT_NET_FETCHER_TEST_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/cert/cert_net_fetcher.h"
#include "services/network/public/cpp/cert_verifier/cert_net_fetcher_url_loader.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"

namespace cert_verifier {

// Holds a CertNetFetcher, and either a network::TestURLLoaderFactory (for mock
// response to network requests) or a network::TestSharedURLLoaderFactory (for
// real network requests) These test-only classes should be created only on the
// network thread.

class CertNetFetcherTestUtil {
 public:
  CertNetFetcherTestUtil();

  virtual ~CertNetFetcherTestUtil();

  scoped_refptr<CertNetFetcherURLLoader>& fetcher() { return fetcher_; }

 protected:
  scoped_refptr<CertNetFetcherURLLoader> fetcher_;
  mojo::PendingReceiver<network::mojom::URLLoaderFactory> pending_receiver_;
};

class CertNetFetcherTestUtilFakeLoader : public CertNetFetcherTestUtil {
 public:
  CertNetFetcherTestUtilFakeLoader();
  ~CertNetFetcherTestUtilFakeLoader() override;

  network::TestURLLoaderFactory* url_loader_factory() {
    return test_url_loader_factory_.get();
  }

 private:
  std::unique_ptr<network::TestURLLoaderFactory> test_url_loader_factory_;
  mojo::Receiver<network::mojom::URLLoaderFactory> receiver_;
};

class CertNetFetcherTestUtilRealLoader : public CertNetFetcherTestUtil {
 public:
  CertNetFetcherTestUtilRealLoader();
  ~CertNetFetcherTestUtilRealLoader() override;

  scoped_refptr<network::TestSharedURLLoaderFactory>
  shared_url_loader_factory() {
    return test_shared_url_loader_factory_;
  }

 private:
  scoped_refptr<network::TestSharedURLLoaderFactory>
      test_shared_url_loader_factory_;
  mojo::Receiver<network::mojom::URLLoaderFactory> receiver_;
};

}  // namespace cert_verifier

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CERT_VERIFIER_CERT_NET_FETCHER_TEST_H_
