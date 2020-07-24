// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/sct_auditing_cache.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/rand_util.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "net/base/hash_value.h"
#include "net/base/host_port_pair.h"
#include "net/cert/ct_serialization.h"
#include "net/cert/signed_certificate_timestamp.h"
#include "net/cert/signed_certificate_timestamp_and_status.h"
#include "net/cert/x509_certificate.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/features.h"
#include "third_party/boringssl/src/include/openssl/pool.h"
#include "third_party/boringssl/src/include/openssl/sha.h"

namespace network {

SCTAuditReport::SCTAuditReport() = default;
SCTAuditReport::~SCTAuditReport() = default;
SCTAuditReport::SCTAuditReport(const SCTAuditReport& other) = default;
SCTAuditReport::SCTAuditReport(SCTAuditReport&& other) = default;

SCTAuditingCache::SCTAuditingCache(size_t cache_size) : cache_(cache_size) {}
SCTAuditingCache::~SCTAuditingCache() = default;

void SCTAuditingCache::MaybeEnqueueReport(
    NetworkContext* context,
    const net::HostPortPair& host_port_pair,
    const net::X509Certificate* validated_certificate_chain,
    const net::SignedCertificateTimestampAndStatusList&
        signed_certificate_timestamps) {
  if (!base::FeatureList::IsEnabled(features::kSCTAuditing) ||
      !context->is_sct_auditing_enabled()) {
    return;
  }

  // Generate the cache key for this report. In order to have the cache
  // deduplicate reports for the same SCTs, we compute the cache key as the
  // hash of the SCTs. The digest is converted to a string for use over Mojo.
  SHA256_CTX ctx;
  SHA256_Init(&ctx);
  for (const auto& sct : signed_certificate_timestamps) {
    std::string serialized_sct;
    net::ct::EncodeSignedCertificateTimestamp(sct.sct, &serialized_sct);
    SHA256_Update(&ctx, serialized_sct.data(), serialized_sct.size());
  }
  net::SHA256HashValue cache_key;
  SHA256_Final(reinterpret_cast<uint8_t*>(&cache_key), &ctx);

  // Check if the SCTs are already in the cache. This will update the last seen
  // time if they are present in the cache.
  auto it = cache_.Get(cache_key);
  if (it != cache_.end())
    return;

  // Insert SCTs into cache.
  // TODO(crbug.com/1082860): Construct the proto object directly and store that
  // in the cache instead of this intermediate form, once the proto is added.
  auto report = std::make_unique<SCTAuditReport>();
  report->time_seen = base::Time::Now();
  report->host_port_pair = host_port_pair;

  // GetPEMEncodedChain() can fail, but we still want to enqueue the report for
  // the SCTs (in that case, |report->certificate_chain| is not guaranteed to
  // be valid).
  report->certificate_chain = std::vector<std::string>();
  validated_certificate_chain->GetPEMEncodedChain(&report->certificate_chain);

  for (const auto& sct : signed_certificate_timestamps) {
    report->sct_list.push_back(sct);
  }

  cache_.Put(cache_key, std::move(report));

  double sampling_rate = features::kSCTAuditingSamplingRate.Get();
  if (base::RandDouble() > sampling_rate)
    return;

  context->client()->OnSCTReportReady(net::HashValue(cache_key).ToString());
}

SCTAuditReport* SCTAuditingCache::GetPendingReport(
    const net::SHA256HashValue& cache_key) {
  NOTIMPLEMENTED();
  return nullptr;
}

void SCTAuditingCache::ClearCache() {
  cache_.Clear();
}

}  // namespace network
