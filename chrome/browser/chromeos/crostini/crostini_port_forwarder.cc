// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>

#include "chrome/browser/chromeos/crostini/crostini_port_forwarder.h"

#include "base/bind.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/dbus/permission_broker/permission_broker_client.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace crostini {

// Currently, we are not supporting ethernet/mlan/usb port forwarding.
constexpr char kDefaultInterfaceToForward[] = "wlan0";

class CrostiniPortForwarderFactory : public BrowserContextKeyedServiceFactory {
 public:
  static CrostiniPortForwarder* GetForProfile(Profile* profile) {
    return static_cast<CrostiniPortForwarder*>(
        GetInstance()->GetServiceForBrowserContext(profile, true));
  }

  static CrostiniPortForwarderFactory* GetInstance() {
    static base::NoDestructor<CrostiniPortForwarderFactory> factory;
    return factory.get();
  }

 private:
  friend class base::NoDestructor<CrostiniPortForwarderFactory>;

  CrostiniPortForwarderFactory()
      : BrowserContextKeyedServiceFactory(
            "CrostiniPortForwarderService",
            BrowserContextDependencyManager::GetInstance()) {}

  ~CrostiniPortForwarderFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override {
    Profile* profile = Profile::FromBrowserContext(context);
    return new CrostiniPortForwarder(profile);
  }
};

CrostiniPortForwarder* CrostiniPortForwarder::GetForProfile(Profile* profile) {
  return CrostiniPortForwarderFactory::GetForProfile(profile);
}

CrostiniPortForwarder::CrostiniPortForwarder(Profile* profile)
    : profile_(profile) {
  // TODO(matterchen): Profile will be used later, this is to remove warnings.
  (void)profile_;
}

CrostiniPortForwarder::~CrostiniPortForwarder() = default;

void CrostiniPortForwarder::OnActivatePortCompleted(
    ResultCallback result_callback,
    const PortRuleKey& key,
    bool success) {
  if (!success) {
    forwarded_ports_.erase(key);
    LOG(ERROR) << "Failed to activate port, port preference not added: "
               << key.port_number;
    std::move(result_callback).Run(success);
    return;
  }
  // TODO(matterchen): Update current port forwarding preference.
  std::move(result_callback).Run(success);
}

void CrostiniPortForwarder::OnAddPortCompleted(ResultCallback result_callback,
                                               const std::string& label,
                                               const PortRuleKey& key,
                                               bool success) {
  if (!success) {
    forwarded_ports_.erase(key);
    LOG(ERROR) << "Failed to activate port, port preference not added: "
               << key.port_number;
    std::move(result_callback).Run(success);
    return;
  }
  // TODO(matterchen): Add new port forwarding preference.
  std::move(result_callback).Run(success);
}

void CrostiniPortForwarder::TryActivatePort(
    uint16_t port_number,
    const Protocol& protocol_type,
    const std::string& ipv4_addr,
    chromeos::PermissionBrokerClient::ResultCallback result_callback) {
  chromeos::PermissionBrokerClient* client =
      chromeos::PermissionBrokerClient::Get();
  if (!client) {
    LOG(ERROR) << "Could not get permission broker client.";
    std::move(result_callback).Run(false);
    return;
  }

  int lifeline[2] = {-1, -1};
  if (pipe(lifeline) < 0) {
    LOG(ERROR) << "Failed to create a lifeline pipe";
    std::move(result_callback).Run(false);
    return;
  }

  PortRuleKey port_key = {
      .port_number = port_number,
      .protocol_type = protocol_type,
      .input_ifname = kDefaultInterfaceToForward,
  };

  base::ScopedFD lifeline_local(lifeline[0]);
  base::ScopedFD lifeline_remote(lifeline[1]);

  forwarded_ports_[port_key] = std::move(lifeline_local);

  if (Protocol::TCP == protocol_type) {
    client->RequestTcpPortForward(
        port_number, kDefaultInterfaceToForward, ipv4_addr, port_number,
        std::move(lifeline_remote.get()), std::move(result_callback));
  } else if (Protocol::UDP == protocol_type) {
    client->RequestUdpPortForward(
        port_number, kDefaultInterfaceToForward, ipv4_addr, port_number,
        std::move(lifeline_remote.get()), std::move(result_callback));
  }
}

void CrostiniPortForwarder::AddPort(uint16_t port_number,
                                    const Protocol& protocol_type,
                                    const std::string& label,
                                    ResultCallback result_callback) {
  PortRuleKey new_port_key = {
      .port_number = port_number,
      .protocol_type = protocol_type,
      .input_ifname = kDefaultInterfaceToForward,
  };

  if (forwarded_ports_.find(new_port_key) != forwarded_ports_.end()) {
    LOG(ERROR) << "Trying to add an already forwarded port.";
    std::move(result_callback).Run(false);
    return;
  }

  base::OnceCallback<void(bool)> on_add_port_completed =
      base::BindOnce(&CrostiniPortForwarder::OnAddPortCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(result_callback),
                     label, new_port_key);

  // TODO(matterchen): Extract container IPv4 address.
  TryActivatePort(port_number, protocol_type, "PLACEHOLDER_IP_ADDRESS",
                  std::move(on_add_port_completed));
}

void CrostiniPortForwarder::ActivatePort(uint16_t port_number,
                                         ResultCallback result_callback) {
  // TODO(matterchen): Find the current port setting from profile preferences.
  PortRuleKey existing_port_key = {
      .port_number = port_number,
      .protocol_type = Protocol::TCP,
      .input_ifname = kDefaultInterfaceToForward,
  };

  base::OnceCallback<void(bool)> on_activate_port_completed =
      base::BindOnce(&CrostiniPortForwarder::OnActivatePortCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(result_callback),
                     existing_port_key);

  // TODO(matterchen): Extract container IPv4 address.
  CrostiniPortForwarder::TryActivatePort(port_number, Protocol::TCP,
                                         "PLACEHOLDER_IP_ADDRESS",
                                         std::move(on_activate_port_completed));
}

}  // namespace crostini
