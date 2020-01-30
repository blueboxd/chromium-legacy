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
    PortRuleKey key,
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
                                               std::string label,
                                               PortRuleKey key,
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

void CrostiniPortForwarder::OnDeactivatePortCompleted(
    ResultCallback result_callback,
    PortRuleKey key,
    bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to deactivate port, port is still being forwarded: "
               << key.port_number;
    std::move(result_callback).Run(success);
    return;
  }
  // TODO(matterchen): Set existing port forward preference active state ==
  // False.
  forwarded_ports_.erase(key);
  std::move(result_callback).Run(success);
}

void CrostiniPortForwarder::OnRemovePortCompleted(
    ResultCallback result_callback,
    PortRuleKey key,
    bool success) {
  if (!success) {
    LOG(ERROR) << "Failed to remove port, port is still being forwarded: "
               << key.port_number;
    std::move(result_callback).Run(success);
    return;
  }
  // TODO(matterchen): Remove existing port forward preference.
  forwarded_ports_.erase(key);
  std::move(result_callback).Run(success);
}

void CrostiniPortForwarder::TryActivatePort(
    uint16_t port_number,
    const Protocol& protocol_type,
    const std::string& ipv4_addr,
    base::OnceCallback<void(bool)> result_callback) {
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

  switch (protocol_type) {
    case Protocol::TCP:
      client->RequestTcpPortForward(
          port_number, kDefaultInterfaceToForward, ipv4_addr, port_number,
          std::move(lifeline_remote.get()), std::move(result_callback));
      break;
    case Protocol::UDP:
      client->RequestUdpPortForward(
          port_number, kDefaultInterfaceToForward, ipv4_addr, port_number,
          std::move(lifeline_remote.get()), std::move(result_callback));
      break;
  }
}

void CrostiniPortForwarder::TryDeactivatePort(
    const PortRuleKey& key,
    base::OnceCallback<void(bool)> result_callback) {
  if (forwarded_ports_.find(key) == forwarded_ports_.end()) {
    LOG(ERROR) << "Trying to deactivate a non-active port.";
    std::move(result_callback).Run(false);
    return;
  }

  chromeos::PermissionBrokerClient* client =
      chromeos::PermissionBrokerClient::Get();
  if (!client) {
    LOG(ERROR) << "Could not get permission broker client.";
    std::move(result_callback).Run(false);
    return;
  }

  switch (key.protocol_type) {
    case Protocol::TCP:
      client->ReleaseTcpPortForward(key.port_number, kDefaultInterfaceToForward,
                                    std::move(result_callback));
      break;
    case Protocol::UDP:
      client->ReleaseUdpPortForward(key.port_number, kDefaultInterfaceToForward,
                                    std::move(result_callback));
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
                                         const Protocol& protocol_type,
                                         ResultCallback result_callback) {
  PortRuleKey existing_port_key = {
      .port_number = port_number,
      .protocol_type = protocol_type,
      .input_ifname = kDefaultInterfaceToForward,
  };

  if (forwarded_ports_.find(existing_port_key) != forwarded_ports_.end()) {
    LOG(ERROR) << "Trying to activate an already active port.";
    std::move(result_callback).Run(false);
    return;
  }

  base::OnceCallback<void(bool)> on_activate_port_completed =
      base::BindOnce(&CrostiniPortForwarder::OnActivatePortCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(result_callback),
                     existing_port_key);

  // TODO(matterchen): Extract container IPv4 address.
  CrostiniPortForwarder::TryActivatePort(port_number, protocol_type,
                                         "PLACEHOLDER_IP_ADDRESS",
                                         std::move(on_activate_port_completed));
}

void CrostiniPortForwarder::DeactivatePort(uint16_t port_number,
                                           const Protocol& protocol_type,
                                           ResultCallback result_callback) {
  PortRuleKey existing_port_key = {
      .port_number = port_number,
      .protocol_type = protocol_type,
      .input_ifname = kDefaultInterfaceToForward,
  };

  base::OnceCallback<void(bool)> on_deactivate_port_completed =
      base::BindOnce(&CrostiniPortForwarder::OnDeactivatePortCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(result_callback),
                     existing_port_key);

  CrostiniPortForwarder::TryDeactivatePort(
      existing_port_key, std::move(on_deactivate_port_completed));
}

void CrostiniPortForwarder::RemovePort(uint16_t port_number,
                                       const Protocol& protocol_type,
                                       ResultCallback result_callback) {
  // TODO(matterchen): Check if port is active in preferences, if active,
  // deactivate port using on_remove_port_completed callback. Otherwise, just
  // remove from preferences by calling OnRemovePortCompleted directly.
  PortRuleKey existing_port_key = {
      .port_number = port_number,
      .protocol_type = protocol_type,
      .input_ifname = kDefaultInterfaceToForward,
  };

  base::OnceCallback<void(bool)> on_remove_port_completed =
      base::BindOnce(&CrostiniPortForwarder::OnRemovePortCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(result_callback),
                     existing_port_key);

  CrostiniPortForwarder::TryDeactivatePort(existing_port_key,
                                           std::move(on_remove_port_completed));
}

size_t CrostiniPortForwarder::GetNumberOfForwardedPortsForTesting() {
  return forwarded_ports_.size();
}

}  // namespace crostini
