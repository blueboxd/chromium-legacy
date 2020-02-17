// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TRUST_TOKENS_IN_MEMORY_TRUST_TOKEN_PERSISTER_H_
#define SERVICES_NETWORK_TRUST_TOKENS_IN_MEMORY_TRUST_TOKEN_PERSISTER_H_

#include <map>
#include <memory>
#include <utility>

#include "services/network/trust_tokens/proto/public.pb.h"
#include "services/network/trust_tokens/proto/storage.pb.h"
#include "services/network/trust_tokens/trust_token_persister.h"
#include "url/origin.h"

namespace network {

// An InMemoryTrustTokenPersister stores Trust Tokens state during its lifetime,
// but does not write it through to a backend. It is suitable for use in tests
// (as a fake) and in environments without access to SQL.
class InMemoryTrustTokenPersister : public TrustTokenPersister {
 public:
  InMemoryTrustTokenPersister();
  ~InMemoryTrustTokenPersister() override;

  // TrustTokenPersister implementation:
  std::unique_ptr<TrustTokenIssuerConfig> GetIssuerConfig(
      const url::Origin& issuer) override;
  std::unique_ptr<TrustTokenToplevelConfig> GetToplevelConfig(
      const url::Origin& toplevel) override;
  std::unique_ptr<TrustTokenIssuerToplevelPairConfig>
  GetIssuerToplevelPairConfig(const url::Origin& issuer,
                              const url::Origin& toplevel) override;

  void SetIssuerConfig(const url::Origin& issuer,
                       std::unique_ptr<TrustTokenIssuerConfig> config) override;
  void SetToplevelConfig(
      const url::Origin& toplevel,
      std::unique_ptr<TrustTokenToplevelConfig> config) override;
  void SetIssuerToplevelPairConfig(
      const url::Origin& issuer,
      const url::Origin& toplevel,
      std::unique_ptr<TrustTokenIssuerToplevelPairConfig> config) override;

 private:
  std::map<url::Origin, std::unique_ptr<TrustTokenToplevelConfig>>
      toplevel_configs_;
  std::map<url::Origin, std::unique_ptr<TrustTokenIssuerConfig>>
      issuer_configs_;
  std::map<std::pair<url::Origin, url::Origin>,
           std::unique_ptr<TrustTokenIssuerToplevelPairConfig>>
      issuer_toplevel_pair_configs_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_TRUST_TOKENS_IN_MEMORY_TRUST_TOKEN_PERSISTER_H_
