// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_PORT_FORWARDER_H_
#define CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_PORT_FORWARDER_H_

#include <string>

#include "base/files/scoped_file.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace crostini {

class CrostiniPortForwarder : public KeyedService {
 public:
  enum class Protocol {
    TCP,
    UDP,
  };

  struct PortRuleKey {
    uint16_t port_number;
    Protocol protocol_type;
    std::string input_ifname;

    bool operator==(const PortRuleKey& other) const {
      return port_number == other.port_number &&
             protocol_type == other.protocol_type &&
             input_ifname == other.input_ifname;
    }
  };

  // Helper for using PortRuleKey as key entries in std::unordered_maps.
  struct PortRuleKeyHasher {
    std::size_t operator()(const PortRuleKey& k) const {
      return ((std::hash<uint16_t>()(k.port_number) ^
               (std::hash<Protocol>()(k.protocol_type) << 1)) >>
              1) ^
             (std::hash<std::string>()(k.input_ifname) << 1);
    }
  };

  using ResultCallback = base::OnceCallback<void(bool)>;
  void ActivatePort(uint16_t port_number, ResultCallback result_callback);
  void AddPort(uint16_t port_number,
               const Protocol& protocol_type,
               const std::string& label,
               ResultCallback result_callback);

  static CrostiniPortForwarder* GetForProfile(Profile* profile);

  explicit CrostiniPortForwarder(Profile* profile);
  ~CrostiniPortForwarder() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(CrostiniPortForwarderTest, AddPortDuplicateFail);
  FRIEND_TEST_ALL_PREFIXES(CrostiniPortForwarderTest, AddPortMultipleSuccess);
  FRIEND_TEST_ALL_PREFIXES(CrostiniPortForwarderTest, AddPortUdpAndTcpSuccess);
  FRIEND_TEST_ALL_PREFIXES(CrostiniPortForwarderTest,
                           TryActivatePortPermissionBrokerClientFail);
  void OnActivatePortCompleted(ResultCallback result_callback,
                               PortRuleKey key,
                               bool success);
  void OnAddPortCompleted(ResultCallback result_callback,
                          std::string label,
                          PortRuleKey key,
                          bool success);
  void TryActivatePort(uint16_t port_number,
                       const Protocol& protocol_type,
                       const std::string& ipv4_addr,
                       base::OnceCallback<void(bool)> result_callback);

  // For each port rule (protocol, port, interface), keep track of the fd which
  // requested it so we can release it on removal / deactivate.
  std::unordered_map<PortRuleKey, base::ScopedFD, PortRuleKeyHasher>
      forwarded_ports_;

  Profile* profile_;

  base::WeakPtrFactory<CrostiniPortForwarder> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CrostiniPortForwarder);

};  // class

}  // namespace crostini

#endif  // CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_PORT_FORWARDER_H_
