// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_client.h"

#include <utility>

#include "base/bind.h"
#include "base/rand_util.h"
#include "base/values.h"
#include "net/dns/address_sorter.h"
#include "net/dns/dns_session.h"
#include "net/dns/dns_socket_pool.h"
#include "net/dns/dns_transaction.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"
#include "net/socket/client_socket_factory.h"

namespace net {

namespace {

// Creates NetLog parameters for the DNS_CONFIG_CHANGED event.
base::Value NetLogDnsConfigParams(const DnsConfig* config) {
  if (!config)
    return base::DictionaryValue();

  return base::Value::FromUniquePtrValue(config->ToValue());
}

bool IsEqual(const base::Optional<DnsConfig>& c1, const DnsConfig* c2) {
  if (!c1.has_value() && c2 == nullptr)
    return true;

  if (!c1.has_value() || c2 == nullptr)
    return false;

  return c1.value() == *c2;
}

class DnsClientImpl : public DnsClient {
 public:
  DnsClientImpl(NetLog* net_log,
                ClientSocketFactory* socket_factory,
                const RandIntCallback& rand_int_callback)
      : net_log_(net_log),
        socket_factory_(socket_factory),
        rand_int_callback_(rand_int_callback) {}

  bool CanUseSecureDnsTransactions() const override {
    return HasAvailableDohServer();
  }

  bool CanUseInsecureDnsTransactions() const override {
    return session_ != nullptr && insecure_enabled_ &&
           !GetEffectiveConfig()->dns_over_tls_active;
  }

  void SetInsecureEnabled(bool enabled) override {
    insecure_enabled_ = enabled;
  }

  bool FallbackFromInsecureTransactionPreferred() const override {
    return !CanUseInsecureDnsTransactions() ||
           insecure_fallback_failures_ >= kMaxInsecureFallbackFailures;
  }

  bool SetSystemConfig(base::Optional<DnsConfig> system_config) override {
    if (system_config == system_config_)
      return false;

    system_config_ = std::move(system_config);

    return UpdateDnsConfig();
  }

  bool SetConfigOverrides(DnsConfigOverrides config_overrides) override {
    if (config_overrides == config_overrides_)
      return false;

    config_overrides_ = std::move(config_overrides);

    return UpdateDnsConfig();
  }

  const DnsConfig* GetEffectiveConfig() const override {
    if (!session_)
      return nullptr;

    DCHECK(session_->config().IsValid());
    return &session_->config();
  }

  const DnsHosts* GetHosts() const override {
    const DnsConfig* config = GetEffectiveConfig();
    if (!config)
      return nullptr;

    return &config->hosts;
  }

  DnsTransactionFactory* GetTransactionFactory() override {
    return session_.get() ? factory_.get() : nullptr;
  }

  AddressSorter* GetAddressSorter() override { return address_sorter_.get(); }

  void IncrementInsecureFallbackFailures() override {
    ++insecure_fallback_failures_;
  }

  void ClearInsecureFallbackFailures() override {
    insecure_fallback_failures_ = 0;
  }

  base::Optional<DnsConfig> GetSystemConfigForTesting() const override {
    return system_config_;
  }

  DnsConfigOverrides GetConfigOverridesForTesting() const override {
    return config_overrides_;
  }

 private:
  base::Optional<DnsConfig> BuildEffectiveConfig() const {
    DnsConfig config;
    if (config_overrides_.OverridesEverything()) {
      config = config_overrides_.ApplyOverrides(DnsConfig());
    } else {
      if (!system_config_)
        return base::nullopt;

      config = config_overrides_.ApplyOverrides(system_config_.value());
    }

    if (!config.IsValid() || config.unhandled_options)
      return base::nullopt;

    return config;
  }

  bool UpdateDnsConfig() {
    base::Optional<DnsConfig> new_effective_config = BuildEffectiveConfig();

    if (IsEqual(new_effective_config, GetEffectiveConfig()))
      return false;

    insecure_fallback_failures_ = 0;
    UpdateSession(std::move(new_effective_config));

    if (net_log_) {
      net_log_->AddGlobalEntry(NetLogEventType::DNS_CONFIG_CHANGED, [&] {
        return NetLogDnsConfigParams(GetEffectiveConfig());
      });
    }

    return true;
  }

  void UpdateSession(base::Optional<DnsConfig> new_effective_config) {
    factory_.reset();
    session_ = nullptr;

    if (new_effective_config) {
      DCHECK(new_effective_config.value().IsValid());
      DCHECK(!new_effective_config.value().unhandled_options);

      std::unique_ptr<DnsSocketPool> socket_pool(
          new_effective_config.value().randomize_ports
              ? DnsSocketPool::CreateDefault(socket_factory_,
                                             rand_int_callback_)
              : DnsSocketPool::CreateNull(socket_factory_, rand_int_callback_));
      session_ =
          new DnsSession(std::move(new_effective_config).value(),
                         std::move(socket_pool), rand_int_callback_, net_log_);
      factory_ = DnsTransactionFactory::CreateFactory(session_.get());
    }
  }

  bool HasAvailableDohServer() const {
    // TODO(crbug.com/985589): Once DoH probes are sent, update this such that a
    // DoH server is considered successful if it has a successful probe state.
    const DnsConfig* config = GetEffectiveConfig();
    return config && config->dns_over_https_servers.size() > 0;
  }

  bool insecure_enabled_ = false;
  int insecure_fallback_failures_ = 0;

  base::Optional<DnsConfig> system_config_;
  DnsConfigOverrides config_overrides_;

  scoped_refptr<DnsSession> session_;
  std::unique_ptr<DnsTransactionFactory> factory_;
  std::unique_ptr<AddressSorter> address_sorter_ =
      AddressSorter::CreateAddressSorter();

  NetLog* net_log_;

  ClientSocketFactory* socket_factory_;
  const RandIntCallback rand_int_callback_;

  DISALLOW_COPY_AND_ASSIGN(DnsClientImpl);
};

}  // namespace

// static
std::unique_ptr<DnsClient> DnsClient::CreateClient(NetLog* net_log) {
  return std::make_unique<DnsClientImpl>(
      net_log, ClientSocketFactory::GetDefaultFactory(),
      base::Bind(&base::RandInt));
}

// static
std::unique_ptr<DnsClient> DnsClient::CreateClientForTesting(
    NetLog* net_log,
    ClientSocketFactory* socket_factory,
    const RandIntCallback& rand_int_callback) {
  return std::make_unique<DnsClientImpl>(net_log, socket_factory,
                                         rand_int_callback);
}

}  // namespace net
