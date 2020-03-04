// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cert_verifier/cert_net_fetcher_test.h"

#include "base/memory/scoped_refptr.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace cert_verifier {
CertNetFetcherTestUtil::CertNetFetcherTestUtil() {
  mojo::PendingRemote<network::mojom::URLLoaderFactory>
      pending_remote_url_loader_factory;
  pending_receiver_ =
      pending_remote_url_loader_factory.InitWithNewPipeAndPassReceiver();
  fetcher_ = base::MakeRefCounted<CertNetFetcherURLLoader>(
      std::move(pending_remote_url_loader_factory));
}

CertNetFetcherTestUtil::~CertNetFetcherTestUtil() = default;

CertNetFetcherTestUtilFakeLoader::~CertNetFetcherTestUtilFakeLoader() = default;

CertNetFetcherTestUtilFakeLoader::CertNetFetcherTestUtilFakeLoader()
    : test_url_loader_factory_(
          std::make_unique<network::TestURLLoaderFactory>()),
      receiver_(test_url_loader_factory_.get(), std::move(pending_receiver_)) {}

CertNetFetcherTestUtilRealLoader::~CertNetFetcherTestUtilRealLoader() = default;

CertNetFetcherTestUtilRealLoader::CertNetFetcherTestUtilRealLoader()
    : test_shared_url_loader_factory_(
          base::MakeRefCounted<network::TestSharedURLLoaderFactory>(
              nullptr /* network_service */,
              true /* is_trusted */)),
      receiver_(test_shared_url_loader_factory_.get(),
                std::move(pending_receiver_)) {}
}  // namespace cert_verifier
