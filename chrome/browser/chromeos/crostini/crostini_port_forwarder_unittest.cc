// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_port_forwarder.h"

#include "base/bind_helpers.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_test_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/permission_broker/fake_permission_broker_client.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Mock;
using testing::Return;

void TestingCallback(bool* out, bool in) {
  *out = in;
}

namespace crostini {

class CrostiniPortForwarderTest : public testing::Test {
 public:
  CrostiniPortForwarderTest()
      : default_container_id_(
            ContainerId(kCrostiniDefaultVmName, kCrostiniDefaultContainerName)),
        other_container_id_(ContainerId("other", "other")),
        inactive_container_id_(ContainerId("inactive", "inactive")) {}

  ~CrostiniPortForwarderTest() override {}

  void SetUp() override {
    chromeos::DBusThreadManager::Initialize();
    chromeos::PermissionBrokerClient::InitializeFake();
    profile_ = std::make_unique<TestingProfile>();
    CrostiniManager::GetForProfile(profile())->AddRunningVmForTesting(
        kCrostiniDefaultVmName);
    CrostiniManager::GetForProfile(profile())->AddRunningContainerForTesting(
        kCrostiniDefaultVmName,
        ContainerInfo(kCrostiniDefaultContainerName, kCrostiniDefaultUsername,
                      "home/testuser1", "CONTAINER_IP_ADDRESS"));
    test_helper_ = std::make_unique<CrostiniTestHelper>(profile_.get());
    crostini_port_forwarder_ =
        std::make_unique<CrostiniPortForwarder>(profile());
    crostini_port_forwarder_->AddObserver(&mock_observer_);
  }

  void TearDown() override {
    chromeos::PermissionBrokerClient::Shutdown();
    crostini_port_forwarder_->RemoveObserver(&mock_observer_);
    crostini_port_forwarder_.reset();
    test_helper_.reset();
    profile_.reset();
    chromeos::DBusThreadManager::Shutdown();
  }

 protected:
  class MockPortObserver : public CrostiniPortForwarder::Observer {
   public:
    MOCK_METHOD(void,
                OnActivePortsChanged,
                (const base::ListValue& activePorts),
                (override));
  };

  Profile* profile() { return profile_.get(); }

  void SetupActiveContainer(const ContainerId& container_id) {
    CrostiniManager::GetForProfile(profile())->AddRunningVmForTesting(
        container_id.vm_name);
    CrostiniManager::GetForProfile(profile())->AddRunningContainerForTesting(
        container_id.vm_name,
        ContainerInfo(container_id.container_name, kCrostiniDefaultUsername,
                      "home/testuser1", "CONTAINER_IP_ADDRESS"));
  }

  CrostiniPortForwarder::PortRuleKey GetDefaultActiveTcpPortKey() {
    return {
        .port_number = 5000,
        .protocol_type = CrostiniPortForwarder::Protocol::TCP,
        .input_ifname = crostini::kDefaultInterfaceToForward,
        .container_id = default_container_id_,
    };
  }

  CrostiniPortForwarder::PortRuleKey GetDefaultActiveUdpPortKey() {
    return {
        .port_number = 5000,
        .protocol_type = CrostiniPortForwarder::Protocol::UDP,
        .input_ifname = crostini::kDefaultInterfaceToForward,
        .container_id = default_container_id_,
    };
  }

  CrostiniPortForwarder::PortRuleKey GetDefaultInactiveTcpPortKey() {
    return {
        .port_number = 5000,
        .protocol_type = CrostiniPortForwarder::Protocol::TCP,
        .input_ifname = crostini::kDefaultInterfaceToForward,
        .container_id = inactive_container_id_,
    };
  }

  CrostiniPortForwarder::PortRuleKey GetDefaultInactiveUdpPortKey() {
    return {
        .port_number = 5000,
        .protocol_type = CrostiniPortForwarder::Protocol::UDP,
        .input_ifname = crostini::kDefaultInterfaceToForward,
        .container_id = inactive_container_id_,
    };
  }

  void MakePermissionBrokerPortForwardingExpectation(
      int port_number,
      CrostiniPortForwarder::Protocol protocol,
      bool exists) {
    // TODO(matterchen): Expectations on all forwarded interfaces.
    switch (protocol) {
      case CrostiniPortForwarder::Protocol::TCP:
        EXPECT_EQ(
            chromeos::FakePermissionBrokerClient::Get()->HasTcpPortForward(
                port_number, crostini::kWlanInterface),
            exists);
        break;
      case CrostiniPortForwarder::Protocol::UDP:
        EXPECT_EQ(
            chromeos::FakePermissionBrokerClient::Get()->HasUdpPortForward(
                port_number, crostini::kWlanInterface),
            exists);
        break;
    }
  }

  void MakePortPreferenceExpectation(CrostiniPortForwarder::PortRuleKey key,
                                     bool exists,
                                     std::string label) {
    base::Optional<base::Value> pref =
        crostini_port_forwarder_->ReadPortPreferenceForTesting(key);
    EXPECT_EQ(exists, pref.has_value());
    if (!exists) {
      return;
    }
    EXPECT_EQ(key.port_number,
              pref.value().FindIntKey(crostini::kPortNumberKey).value());
    EXPECT_EQ(static_cast<int>(key.protocol_type),
              pref.value().FindIntKey(crostini::kPortProtocolKey).value());

    EXPECT_EQ(key.input_ifname,
              *pref.value().FindStringKey(crostini::kPortInterfaceKey));
    EXPECT_EQ(key.container_id.vm_name,
              *pref.value().FindStringKey(crostini::kPortVmNameKey));
    EXPECT_EQ(key.container_id.container_name,
              *pref.value().FindStringKey(crostini::kPortContainerNameKey));
    EXPECT_EQ(label, *pref.value().FindStringKey(crostini::kPortLabelKey));
  }

  ContainerId default_container_id_;
  ContainerId other_container_id_;
  ContainerId inactive_container_id_;

  testing::NiceMock<MockPortObserver> mock_observer_;

  std::unique_ptr<CrostiniTestHelper> test_helper_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<CrostiniPortForwarder> crostini_port_forwarder_;
  content::BrowserTaskEnvironment task_environment_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CrostiniPortForwarderTest);
};

TEST_F(CrostiniPortForwarderTest, InactiveContainerInfoFail) {
  EXPECT_CALL(mock_observer_, OnActivePortsChanged).Times(4);

  bool success = false;
  crostini_port_forwarder_->AddPort(
      inactive_container_id_, 5000, CrostiniPortForwarder::Protocol::TCP,
      "tcp-label", base::BindOnce(&TestingCallback, &success));
  EXPECT_FALSE(success);

  crostini_port_forwarder_->DeactivatePort(
      inactive_container_id_, 5000, CrostiniPortForwarder::Protocol::TCP,
      base::BindOnce(&TestingCallback, &success));
  EXPECT_FALSE(success);

  success = false;
  crostini_port_forwarder_->ActivatePort(
      inactive_container_id_, 5000, CrostiniPortForwarder::Protocol::TCP,
      base::BindOnce(&TestingCallback, &success));
  EXPECT_FALSE(success);

  success = false;
  crostini_port_forwarder_->RemovePort(
      inactive_container_id_, 5000, CrostiniPortForwarder::Protocol::TCP,
      base::BindOnce(&TestingCallback, &success));
  EXPECT_FALSE(success);
}

TEST_F(CrostiniPortForwarderTest, AddPortTcpSuccess) {
  EXPECT_CALL(mock_observer_, OnActivePortsChanged).Times(1);
  MakePermissionBrokerPortForwardingExpectation(
      /*port_number=*/5000, /*protocol=*/CrostiniPortForwarder::Protocol::TCP,
      /*exists=*/false);
  MakePermissionBrokerPortForwardingExpectation(
      /*port_number=*/5000, /*protocol=*/CrostiniPortForwarder::Protocol::UDP,
      /*exists=*/false);

  bool success = false;
  crostini_port_forwarder_->AddPort(
      default_container_id_, 5000, CrostiniPortForwarder::Protocol::TCP,
      "tcp-port", base::BindOnce(&TestingCallback, &success));
  EXPECT_TRUE(success);
  MakePermissionBrokerPortForwardingExpectation(
      /*port_number=*/5000, /*protocol=*/CrostiniPortForwarder::Protocol::TCP,
      /*exists=*/true);
  MakePermissionBrokerPortForwardingExpectation(
      /*port_number=*/5000, /*protocol=*/CrostiniPortForwarder::Protocol::UDP,
      /*exists=*/false);
}

TEST_F(CrostiniPortForwarderTest, AddPortUdpSuccess) {
  EXPECT_CALL(mock_observer_, OnActivePortsChanged).Times(1);
  MakePermissionBrokerPortForwardingExpectation(
      /*port_number=*/5000, /*protocol=*/CrostiniPortForwarder::Protocol::TCP,
      /*exists=*/false);
  MakePermissionBrokerPortForwardingExpectation(
      /*port_number=*/5000, /*protocol=*/CrostiniPortForwarder::Protocol::UDP,
      /*exists=*/false);

  bool success = false;
  crostini_port_forwarder_->AddPort(
      default_container_id_, 5000, CrostiniPortForwarder::Protocol::UDP,
      "udp-port", base::BindOnce(&TestingCallback, &success));
  EXPECT_TRUE(success);
  MakePermissionBrokerPortForwardingExpectation(
      /*port_number=*/5000, /*protocol=*/CrostiniPortForwarder::Protocol::UDP,
      /*exists=*/true);
  MakePermissionBrokerPortForwardingExpectation(
      /*port_number=*/5000, /*protocol=*/CrostiniPortForwarder::Protocol::TCP,
      /*exists=*/false);
}

TEST_F(CrostiniPortForwarderTest, AddPortDuplicateFail) {
  EXPECT_CALL(mock_observer_, OnActivePortsChanged).Times(1);
  MakePermissionBrokerPortForwardingExpectation(
      /*port_number=*/5000, /*protocol=*/CrostiniPortForwarder::Protocol::TCP,
      /*exists=*/false);
  MakePermissionBrokerPortForwardingExpectation(
      /*port_number=*/5000, /*protocol=*/CrostiniPortForwarder::Protocol::UDP,
      /*exists=*/false);

  bool success = false;
  crostini_port_forwarder_->AddPort(
      default_container_id_, 5000, CrostiniPortForwarder::Protocol::UDP,
      "udp-port", base::BindOnce(&TestingCallback, &success));
  EXPECT_TRUE(success);
  MakePermissionBrokerPortForwardingExpectation(
      /*port_number=*/5000, /*protocol=*/CrostiniPortForwarder::Protocol::UDP,
      /*exists=*/true);
  EXPECT_EQ(crostini_port_forwarder_->GetNumberOfForwardedPortsForTesting(),
            1U);

  // Leave success as == true.
  crostini_port_forwarder_->AddPort(
      default_container_id_, 5000, CrostiniPortForwarder::Protocol::UDP,
      "udp-port-duplicate", base::BindOnce(&TestingCallback, &success));
  EXPECT_FALSE(success);
  MakePermissionBrokerPortForwardingExpectation(
      /*port_number=*/5000, /*protocol=*/CrostiniPortForwarder::Protocol::TCP,
      /*exists=*/false);
  MakePermissionBrokerPortForwardingExpectation(
      /*port_number=*/5000, /*protocol=*/CrostiniPortForwarder::Protocol::UDP,
      /*exists=*/true);
  EXPECT_EQ(crostini_port_forwarder_->GetNumberOfForwardedPortsForTesting(),
            1U);
}

TEST_F(CrostiniPortForwarderTest, AddPortUdpAndTcpSuccess) {
  EXPECT_CALL(mock_observer_, OnActivePortsChanged).Times(2);
  MakePermissionBrokerPortForwardingExpectation(
      /*port_number=*/5000, /*protocol=*/CrostiniPortForwarder::Protocol::TCP,
      /*exists=*/false);
  MakePermissionBrokerPortForwardingExpectation(
      /*port_number=*/5000, /*protocol=*/CrostiniPortForwarder::Protocol::UDP,
      /*exists=*/false);

  bool success = false;
  crostini_port_forwarder_->AddPort(
      default_container_id_, 5000, CrostiniPortForwarder::Protocol::UDP,
      "udp-port", base::BindOnce(&TestingCallback, &success));
  EXPECT_TRUE(success);

  success = false;
  crostini_port_forwarder_->AddPort(
      default_container_id_, 5000, CrostiniPortForwarder::Protocol::TCP,
      "tcp-port", base::BindOnce(&TestingCallback, &success));
  EXPECT_TRUE(success);
  EXPECT_EQ(crostini_port_forwarder_->GetNumberOfForwardedPortsForTesting(),
            2U);
}

TEST_F(CrostiniPortForwarderTest, AddPortMultipleSuccess) {
  EXPECT_CALL(mock_observer_, OnActivePortsChanged).Times(3);
  EXPECT_EQ(crostini_port_forwarder_->GetNumberOfForwardedPortsForTesting(),
            0U);
  crostini_port_forwarder_->AddPort(default_container_id_, 5000,
                                    CrostiniPortForwarder::Protocol::UDP,
                                    "udp-port", base::DoNothing());
  crostini_port_forwarder_->AddPort(default_container_id_, 5001,
                                    CrostiniPortForwarder::Protocol::TCP,
                                    "tcp-port", base::DoNothing());
  crostini_port_forwarder_->AddPort(default_container_id_, 5002,
                                    CrostiniPortForwarder::Protocol::UDP,
                                    "udp-port-other", base::DoNothing());
  EXPECT_EQ(crostini_port_forwarder_->GetNumberOfForwardedPortsForTesting(),
            3U);
}

TEST_F(CrostiniPortForwarderTest, TryActivatePortPermissionBrokerClientFail) {
  CrostiniPortForwarder::PortRuleKey tcp_key_second = {
      .port_number = 5001,
      .protocol_type = CrostiniPortForwarder::Protocol::TCP,
      .input_ifname = crostini::kDefaultInterfaceToForward,
      .container_id = default_container_id_,
  };

  bool success = false;
  crostini_port_forwarder_->TryActivatePort(
      GetDefaultActiveTcpPortKey(), default_container_id_,
      base::BindOnce(&TestingCallback, &success));
  EXPECT_TRUE(success);
  EXPECT_EQ(crostini_port_forwarder_->GetNumberOfForwardedPortsForTesting(),
            1U);
  chromeos::PermissionBrokerClient::Shutdown();

  // Leave success as == true.
  crostini_port_forwarder_->TryActivatePort(
      tcp_key_second, default_container_id_,
      base::BindOnce(&TestingCallback, &success));
  EXPECT_FALSE(success);
  EXPECT_EQ(crostini_port_forwarder_->GetNumberOfForwardedPortsForTesting(),
            1U);
  // Re-initialize otherwise Shutdown in TearDown phase will break.
  chromeos::PermissionBrokerClient::InitializeFake();
}

TEST_F(CrostiniPortForwarderTest, DeactivatePortTcpSuccess) {
  EXPECT_CALL(mock_observer_, OnActivePortsChanged).Times(2);
  crostini_port_forwarder_->AddPort(default_container_id_, 5000,
                                    CrostiniPortForwarder::Protocol::TCP,
                                    "tcp-port", base::DoNothing());
  MakePermissionBrokerPortForwardingExpectation(
      /*port_number=*/5000, /*protocol=*/CrostiniPortForwarder::Protocol::TCP,
      /*exists=*/true);

  bool success = false;
  crostini_port_forwarder_->DeactivatePort(
      default_container_id_, 5000, CrostiniPortForwarder::Protocol::TCP,
      base::BindOnce(&TestingCallback, &success));
  EXPECT_TRUE(success);
  MakePermissionBrokerPortForwardingExpectation(
      /*port_number=*/5000, /*protocol=*/CrostiniPortForwarder::Protocol::TCP,
      /*exists=*/false);
  EXPECT_EQ(crostini_port_forwarder_->GetNumberOfForwardedPortsForTesting(),
            0U);
}

TEST_F(CrostiniPortForwarderTest, DeactivatePortUdpSuccess) {
  EXPECT_CALL(mock_observer_, OnActivePortsChanged).Times(2);
  crostini_port_forwarder_->AddPort(default_container_id_, 5000,
                                    CrostiniPortForwarder::Protocol::UDP,
                                    "udp-port", base::DoNothing());
  MakePermissionBrokerPortForwardingExpectation(
      /*port_number=*/5000, /*protocol=*/CrostiniPortForwarder::Protocol::UDP,
      /*exists=*/true);

  bool success = false;
  crostini_port_forwarder_->DeactivatePort(
      default_container_id_, 5000, CrostiniPortForwarder::Protocol::UDP,
      base::BindOnce(&TestingCallback, &success));
  EXPECT_TRUE(success);
  MakePermissionBrokerPortForwardingExpectation(
      /*port_number=*/5000, /*protocol=*/CrostiniPortForwarder::Protocol::UDP,
      /*exists=*/false);
  EXPECT_EQ(crostini_port_forwarder_->GetNumberOfForwardedPortsForTesting(),
            0U);
}

TEST_F(CrostiniPortForwarderTest, DeactivateNonExistentPortFail) {
  EXPECT_CALL(mock_observer_, OnActivePortsChanged).Times(0);
  EXPECT_EQ(crostini_port_forwarder_->GetNumberOfForwardedPortsForTesting(),
            0U);
  MakePermissionBrokerPortForwardingExpectation(
      /*port_number=*/5000, /*protocol=*/CrostiniPortForwarder::Protocol::UDP,
      /*exists=*/false);

  bool success = false;
  crostini_port_forwarder_->DeactivatePort(
      default_container_id_, 5000, CrostiniPortForwarder::Protocol::TCP,
      base::BindOnce(&TestingCallback, &success));
  EXPECT_FALSE(success);

  crostini_port_forwarder_->DeactivatePort(
      default_container_id_, 5000, CrostiniPortForwarder::Protocol::UDP,
      base::BindOnce(&TestingCallback, &success));
  EXPECT_FALSE(success);
  EXPECT_EQ(crostini_port_forwarder_->GetNumberOfForwardedPortsForTesting(),
            0U);
}

TEST_F(CrostiniPortForwarderTest, DeactivateWrongProtocolFail) {
  EXPECT_CALL(mock_observer_, OnActivePortsChanged).Times(1);
  crostini_port_forwarder_->AddPort(default_container_id_, 5000,
                                    CrostiniPortForwarder::Protocol::UDP,
                                    "udp-port", base::DoNothing());
  MakePermissionBrokerPortForwardingExpectation(
      /*port_number=*/5000, /*protocol=*/CrostiniPortForwarder::Protocol::UDP,
      /*exists=*/true);

  bool success = false;
  crostini_port_forwarder_->DeactivatePort(
      default_container_id_, 5000, CrostiniPortForwarder::Protocol::TCP,
      base::BindOnce(&TestingCallback, &success));
  EXPECT_FALSE(success);
  MakePermissionBrokerPortForwardingExpectation(
      /*port_number=*/5000, /*protocol=*/CrostiniPortForwarder::Protocol::UDP,
      /*exists=*/true);
  EXPECT_EQ(crostini_port_forwarder_->GetNumberOfForwardedPortsForTesting(),
            1U);
}

TEST_F(CrostiniPortForwarderTest, DeactivateMultiplePortsSameProtocolSuccess) {
  crostini_port_forwarder_->AddPort(default_container_id_, 5000,
                                    CrostiniPortForwarder::Protocol::TCP,
                                    "tcp-port", base::DoNothing());
  crostini_port_forwarder_->AddPort(default_container_id_, 5000,
                                    CrostiniPortForwarder::Protocol::UDP,
                                    "udp-port", base::DoNothing());
  EXPECT_EQ(crostini_port_forwarder_->GetNumberOfForwardedPortsForTesting(),
            2U);

  bool success = false;
  crostini_port_forwarder_->DeactivatePort(
      default_container_id_, 5000, CrostiniPortForwarder::Protocol::TCP,
      base::BindOnce(&TestingCallback, &success));
  EXPECT_TRUE(success);

  success = false;
  crostini_port_forwarder_->DeactivatePort(
      default_container_id_, 5000, CrostiniPortForwarder::Protocol::UDP,
      base::BindOnce(&TestingCallback, &success));
  EXPECT_TRUE(success);
  EXPECT_EQ(crostini_port_forwarder_->GetNumberOfForwardedPortsForTesting(),
            0U);
}

TEST_F(CrostiniPortForwarderTest, PrefsAddTcpPortActiveContainerSuccess) {
  CrostiniPortForwarder::PortRuleKey tcp_key = GetDefaultActiveTcpPortKey();
  crostini_port_forwarder_->AddPort(tcp_key.container_id, tcp_key.port_number,
                                    tcp_key.protocol_type, "tcp-port-label",
                                    base::DoNothing());
  MakePortPreferenceExpectation(tcp_key, /*exists=*/true,
                                /*label=*/"tcp-port-label");
}

TEST_F(CrostiniPortForwarderTest, PrefsAddUdpPortActiveContainerSuccess) {
  CrostiniPortForwarder::PortRuleKey udp_key = GetDefaultActiveUdpPortKey();
  crostini_port_forwarder_->AddPort(udp_key.container_id, udp_key.port_number,
                                    udp_key.protocol_type, "udp-port-label",
                                    base::DoNothing());
  MakePortPreferenceExpectation(udp_key, /*exists=*/true,
                                /*label=*/"udp-port-label");
}

TEST_F(CrostiniPortForwarderTest, PrefsAddPortInactiveContainerFail) {
  CrostiniPortForwarder::PortRuleKey tcp_key = GetDefaultInactiveTcpPortKey();
  bool success = false;
  crostini_port_forwarder_->AddPort(
      tcp_key.container_id, tcp_key.port_number, tcp_key.protocol_type,
      "tcp-port-label-inactive", base::BindOnce(&TestingCallback, &success));
  EXPECT_FALSE(success);
  MakePortPreferenceExpectation(tcp_key, /*exists=*/true,
                                /*label=*/"tcp-port-label-inactive");
}

TEST_F(CrostiniPortForwarderTest, PrefsAddTcpAndUdpPortSuccess) {
  CrostiniPortForwarder::PortRuleKey tcp_key = GetDefaultActiveTcpPortKey();
  CrostiniPortForwarder::PortRuleKey udp_key = GetDefaultActiveUdpPortKey();
  crostini_port_forwarder_->AddPort(tcp_key.container_id, tcp_key.port_number,
                                    tcp_key.protocol_type, "tcp-port-label",
                                    base::DoNothing());
  crostini_port_forwarder_->AddPort(udp_key.container_id, udp_key.port_number,
                                    udp_key.protocol_type, "udp-port-label",
                                    base::DoNothing());

  MakePortPreferenceExpectation(tcp_key, /*exists=*/true,
                                /*label=*/"tcp-port-label");
  MakePortPreferenceExpectation(udp_key, /*exists=*/true,
                                /*label=*/"udp-port-label");
}

TEST_F(CrostiniPortForwarderTest, PrefsAddDuplicatePortInactiveContainerFail) {
  CrostiniPortForwarder::PortRuleKey tcp_key = GetDefaultActiveTcpPortKey();
  crostini_port_forwarder_->AddPort(
      tcp_key.container_id, tcp_key.port_number, tcp_key.protocol_type,
      "tcp-port-label-original", base::DoNothing());

  bool success = false;
  crostini_port_forwarder_->AddPort(tcp_key.container_id, tcp_key.port_number,
                                    tcp_key.protocol_type, "tcp-port-label-dup",
                                    base::BindOnce(&TestingCallback, &success));
  EXPECT_FALSE(success);
  MakePortPreferenceExpectation(tcp_key, /*exists=*/true,
                                /*label=*/"tcp-port-label-original");
}

TEST_F(CrostiniPortForwarderTest, PrefsAddUniquePortsSuccess) {
  CrostiniPortForwarder::PortRuleKey tcp_key = GetDefaultActiveTcpPortKey();
  CrostiniPortForwarder::PortRuleKey tcp_key_second = {
      .port_number = tcp_key.port_number + 1,
      .protocol_type = tcp_key.protocol_type,
      .input_ifname = tcp_key.input_ifname,
      .container_id = tcp_key.container_id,
  };
  crostini_port_forwarder_->AddPort(tcp_key.container_id, tcp_key.port_number,
                                    tcp_key.protocol_type, "tcp-port-1",
                                    base::DoNothing());

  crostini_port_forwarder_->AddPort(
      tcp_key_second.container_id, tcp_key_second.port_number,
      tcp_key_second.protocol_type, "tcp-port-2", base::DoNothing());

  MakePortPreferenceExpectation(tcp_key, /*exists=*/true,
                                /*label=*/"tcp-port-1");
  MakePortPreferenceExpectation(tcp_key_second, /*exists=*/true,

                                /*label=*/"tcp-port-2");
}

TEST_F(CrostiniPortForwarderTest, PrefsRemoveActiveTcpPortSuccess) {
  EXPECT_CALL(mock_observer_, OnActivePortsChanged).Times(2);
  CrostiniPortForwarder::PortRuleKey tcp_key = GetDefaultActiveTcpPortKey();
  crostini_port_forwarder_->AddPort(tcp_key.container_id, tcp_key.port_number,
                                    tcp_key.protocol_type, "tcp-port-label",
                                    base::DoNothing());

  bool success = false;
  crostini_port_forwarder_->RemovePort(
      tcp_key.container_id, tcp_key.port_number, tcp_key.protocol_type,
      base::BindOnce(&TestingCallback, &success));
  EXPECT_TRUE(success);
  MakePortPreferenceExpectation(tcp_key, /*exists=*/false,
                                /*label=*/"");
}

TEST_F(CrostiniPortForwarderTest, PrefsRemoveActiveUdpPortSuccess) {
  EXPECT_CALL(mock_observer_, OnActivePortsChanged).Times(2);
  CrostiniPortForwarder::PortRuleKey udp_key = GetDefaultActiveUdpPortKey();
  crostini_port_forwarder_->AddPort(udp_key.container_id, udp_key.port_number,
                                    udp_key.protocol_type, "udp-port-label",
                                    base::DoNothing());

  bool success = false;
  crostini_port_forwarder_->RemovePort(
      udp_key.container_id, udp_key.port_number, udp_key.protocol_type,
      base::BindOnce(&TestingCallback, &success));
  EXPECT_TRUE(success);
  MakePortPreferenceExpectation(udp_key, /*exists=*/false,
                                /*label=*/"");
}

TEST_F(CrostiniPortForwarderTest, PrefsRemoveActivePortInactiveContainerFail) {
  CrostiniPortForwarder::PortRuleKey udp_key = GetDefaultInactiveUdpPortKey();
  crostini_port_forwarder_->AddPort(udp_key.container_id, udp_key.port_number,
                                    udp_key.protocol_type, "udp-port-label",
                                    base::DoNothing());

  bool success = false;
  crostini_port_forwarder_->RemovePort(
      udp_key.container_id, udp_key.port_number, udp_key.protocol_type,
      base::BindOnce(&TestingCallback, &success));
  EXPECT_FALSE(success);
  MakePortPreferenceExpectation(udp_key, /*exists=*/false,
                                /*label=*/"");
}

TEST_F(CrostiniPortForwarderTest, PrefsRemoveOnePortSuccess) {
  EXPECT_CALL(mock_observer_, OnActivePortsChanged).Times(3);
  CrostiniPortForwarder::PortRuleKey tcp_key = GetDefaultActiveTcpPortKey();
  CrostiniPortForwarder::PortRuleKey udp_key = GetDefaultActiveUdpPortKey();
  crostini_port_forwarder_->AddPort(tcp_key.container_id, tcp_key.port_number,
                                    tcp_key.protocol_type, "tcp-port-label",
                                    base::DoNothing());
  crostini_port_forwarder_->AddPort(udp_key.container_id, udp_key.port_number,
                                    udp_key.protocol_type, "udp-port-label",
                                    base::DoNothing());
  bool success = false;
  crostini_port_forwarder_->RemovePort(
      udp_key.container_id, udp_key.port_number, udp_key.protocol_type,
      base::BindOnce(&TestingCallback, &success));
  EXPECT_TRUE(success);
  MakePortPreferenceExpectation(udp_key, /*exists=*/false,
                                /*label=*/"");
  MakePortPreferenceExpectation(tcp_key, /*exists=*/true,
                                /*label=*/"tcp-port-label");
}

TEST_F(CrostiniPortForwarderTest, PrefsDeactivateAndActivateTcpPortSuccess) {
  EXPECT_CALL(mock_observer_, OnActivePortsChanged).Times(3);
  CrostiniPortForwarder::PortRuleKey tcp_key = GetDefaultActiveTcpPortKey();
  crostini_port_forwarder_->AddPort(tcp_key.container_id, tcp_key.port_number,
                                    tcp_key.protocol_type, "tcp-port-label",
                                    base::DoNothing());
  bool success = false;
  crostini_port_forwarder_->DeactivatePort(
      tcp_key.container_id, tcp_key.port_number, tcp_key.protocol_type,
      base::BindOnce(&TestingCallback, &success));
  EXPECT_TRUE(success);
  MakePortPreferenceExpectation(tcp_key, /*exists=*/true,
                                /*label=*/"tcp-port-label");

  success = false;
  crostini_port_forwarder_->ActivatePort(
      tcp_key.container_id, tcp_key.port_number, tcp_key.protocol_type,
      base::BindOnce(&TestingCallback, &success));
  EXPECT_TRUE(success);
  MakePortPreferenceExpectation(tcp_key, /*exists=*/true,
                                /*label=*/"tcp-port-label");
}

TEST_F(CrostiniPortForwarderTest, PrefsDeactivateAndActivateUdpPortSuccess) {
  EXPECT_CALL(mock_observer_, OnActivePortsChanged).Times(3);
  CrostiniPortForwarder::PortRuleKey udp_key = GetDefaultActiveUdpPortKey();
  crostini_port_forwarder_->AddPort(udp_key.container_id, udp_key.port_number,
                                    udp_key.protocol_type, "udp-port-label",
                                    base::DoNothing());
  bool success = false;
  crostini_port_forwarder_->DeactivatePort(
      udp_key.container_id, udp_key.port_number, udp_key.protocol_type,
      base::BindOnce(&TestingCallback, &success));
  EXPECT_TRUE(success);
  MakePortPreferenceExpectation(udp_key, /*exists=*/true,
                                /*label=*/"udp-port-label");

  success = false;
  crostini_port_forwarder_->ActivatePort(
      udp_key.container_id, udp_key.port_number, udp_key.protocol_type,
      base::BindOnce(&TestingCallback, &success));
  EXPECT_TRUE(success);
  MakePortPreferenceExpectation(udp_key, /*exists=*/true,
                                /*label=*/"udp-port-label");
}

TEST_F(CrostiniPortForwarderTest, PrefsDeactivateNonExistentPortFail) {
  CrostiniPortForwarder::PortRuleKey tcp_key = GetDefaultActiveTcpPortKey();
  bool success = false;
  crostini_port_forwarder_->DeactivatePort(
      tcp_key.container_id, tcp_key.port_number, tcp_key.protocol_type,
      base::BindOnce(&TestingCallback, &success));
  EXPECT_FALSE(success);
  MakePortPreferenceExpectation(tcp_key, /*exists=*/false,
                                /*label=*/"");
}

TEST_F(CrostiniPortForwarderTest, PrefsDeactivateInactiveContainerFail) {
  CrostiniPortForwarder::PortRuleKey udp_key = GetDefaultInactiveUdpPortKey();
  crostini_port_forwarder_->AddPort(udp_key.container_id, udp_key.port_number,
                                    udp_key.protocol_type, "udp-port-label",
                                    base::DoNothing());
  bool success = false;
  crostini_port_forwarder_->DeactivatePort(
      udp_key.container_id, udp_key.port_number, udp_key.protocol_type,
      base::BindOnce(&TestingCallback, &success));
  EXPECT_FALSE(success);
  MakePortPreferenceExpectation(udp_key, /*exists=*/true,
                                /*label=*/"udp-port-label");
}

TEST_F(CrostiniPortForwarderTest,
       PrefsDeactivateAndActivateOneOfMultipleProtocolSuccess) {
  CrostiniPortForwarder::PortRuleKey tcp_key = GetDefaultActiveTcpPortKey();
  CrostiniPortForwarder::PortRuleKey udp_key = GetDefaultActiveUdpPortKey();
  crostini_port_forwarder_->AddPort(tcp_key.container_id, tcp_key.port_number,
                                    tcp_key.protocol_type, "tcp-port-label",
                                    base::DoNothing());
  crostini_port_forwarder_->AddPort(udp_key.container_id, udp_key.port_number,
                                    udp_key.protocol_type, "udp-port-label",
                                    base::DoNothing());
  bool success = false;
  crostini_port_forwarder_->DeactivatePort(
      udp_key.container_id, udp_key.port_number, udp_key.protocol_type,
      base::BindOnce(&TestingCallback, &success));
  EXPECT_TRUE(success);
  MakePortPreferenceExpectation(tcp_key, /*exists=*/true,
                                /*label=*/"tcp-port-label");
  MakePortPreferenceExpectation(udp_key, /*exists=*/true,
                                /*label=*/"udp-port-label");
  success = false;
  crostini_port_forwarder_->ActivatePort(
      udp_key.container_id, udp_key.port_number, udp_key.protocol_type,
      base::BindOnce(&TestingCallback, &success));
  EXPECT_TRUE(success);
  MakePortPreferenceExpectation(udp_key, /*exists=*/true,
                                /*label=*/"udp-port-label");
}

TEST_F(CrostiniPortForwarderTest, PrefsActivateNonExistentPortFail) {
  CrostiniPortForwarder::PortRuleKey tcp_key = GetDefaultActiveTcpPortKey();
  bool success = false;
  crostini_port_forwarder_->ActivatePort(
      tcp_key.container_id, tcp_key.port_number, tcp_key.protocol_type,
      base::BindOnce(&TestingCallback, &success));
  EXPECT_FALSE(success);
  MakePortPreferenceExpectation(tcp_key, /*exists=*/false,
                                /*label=*/"");
}

TEST_F(CrostiniPortForwarderTest, PrefsActivateInactiveContainerFail) {
  CrostiniPortForwarder::PortRuleKey udp_key = GetDefaultInactiveUdpPortKey();
  crostini_port_forwarder_->AddPort(udp_key.container_id, udp_key.port_number,
                                    udp_key.protocol_type, "udp-port-label",
                                    base::DoNothing());
  crostini_port_forwarder_->DeactivatePort(
      udp_key.container_id, udp_key.port_number, udp_key.protocol_type,
      base::DoNothing());
  MakePortPreferenceExpectation(udp_key, /*exists=*/true,
                                /*label=*/"udp-port-label");
  bool success = false;
  crostini_port_forwarder_->ActivatePort(
      udp_key.container_id, udp_key.port_number, udp_key.protocol_type,
      base::BindOnce(&TestingCallback, &success));
  EXPECT_FALSE(success);
  MakePortPreferenceExpectation(udp_key, /*exists=*/true,
                                /*label=*/"udp-port-label");
}

TEST_F(CrostiniPortForwarderTest, DeactivateAllActiveContainerPortsSuccess) {
  // Test behaviour of 3 port rules:
  // Port 1: Belonging to other container.
  // Port 2&3: Added and activated.
  // Result should be: port 1 is unaffected, ports 2&3 are now deactivated.
  SetupActiveContainer(other_container_id_);
  CrostiniPortForwarder::PortRuleKey other_key = {
      .port_number = 5001,
      .protocol_type = CrostiniPortForwarder::Protocol::TCP,
      .input_ifname = crostini::kDefaultInterfaceToForward,
      .container_id = other_container_id_,
  };

  CrostiniPortForwarder::PortRuleKey tcp_key = GetDefaultActiveTcpPortKey();
  CrostiniPortForwarder::PortRuleKey udp_key = GetDefaultActiveUdpPortKey();
  crostini_port_forwarder_->AddPort(tcp_key.container_id, tcp_key.port_number,
                                    tcp_key.protocol_type, "tcp-port-label",
                                    base::DoNothing());
  crostini_port_forwarder_->AddPort(udp_key.container_id, udp_key.port_number,
                                    udp_key.protocol_type, "udp-port-label",
                                    base::DoNothing());
  crostini_port_forwarder_->AddPort(
      other_key.container_id, other_key.port_number, other_key.protocol_type,
      "other-port-label", base::DoNothing());
  MakePortPreferenceExpectation(tcp_key, /*exists=*/true,
                                /*label=*/"tcp-port-label");
  MakePortPreferenceExpectation(udp_key, /*exists=*/true,
                                /*label=*/"udp-port-label");
  MakePortPreferenceExpectation(other_key, /*exists=*/true,
                                /*label=*/"other-port-label");
  EXPECT_EQ(crostini_port_forwarder_->GetNumberOfForwardedPortsForTesting(),
            3U);

  crostini_port_forwarder_->DeactivateAllActivePorts(tcp_key.container_id);
  EXPECT_EQ(crostini_port_forwarder_->GetNumberOfForwardedPortsForTesting(),
            1U);  // The other container's ports are still active.
  MakePermissionBrokerPortForwardingExpectation(
      /*port_number=*/5000, /*protocol=*/CrostiniPortForwarder::Protocol::TCP,
      /*exists=*/false);
  MakePermissionBrokerPortForwardingExpectation(
      /*port_number=*/5000, /*protocol=*/CrostiniPortForwarder::Protocol::UDP,
      /*exists=*/false);
  MakePermissionBrokerPortForwardingExpectation(
      /*port_number=*/5001, /*protocol=*/CrostiniPortForwarder::Protocol::TCP,
      /*exists=*/true);
  MakePortPreferenceExpectation(tcp_key, /*exists=*/true,
                                /*label=*/"tcp-port-label");
  MakePortPreferenceExpectation(udp_key, /*exists=*/true,
                                /*label=*/"udp-port-label");
  MakePortPreferenceExpectation(other_key, /*exists=*/true,
                                /*label=*/"other-port-label");
}

TEST_F(CrostiniPortForwarderTest, RemoveAllActiveContainerPortsSuccess) {
  // Test behaviour of 3 port rules:
  // Port 1: Belonging to other container.
  // Port 2&3: Added and activated.
  // Result should be: port 1 is unaffected, ports 2&3 are now deactivated and
  // removed.
  SetupActiveContainer(other_container_id_);
  CrostiniPortForwarder::PortRuleKey other_key = {
      .port_number = 5001,
      .protocol_type = CrostiniPortForwarder::Protocol::TCP,
      .input_ifname = crostini::kDefaultInterfaceToForward,
      .container_id = other_container_id_,
  };

  CrostiniPortForwarder::PortRuleKey tcp_key = GetDefaultActiveTcpPortKey();
  CrostiniPortForwarder::PortRuleKey udp_key = GetDefaultActiveUdpPortKey();
  crostini_port_forwarder_->AddPort(tcp_key.container_id, tcp_key.port_number,
                                    tcp_key.protocol_type, "tcp-port-label",
                                    base::DoNothing());
  crostini_port_forwarder_->AddPort(udp_key.container_id, udp_key.port_number,
                                    udp_key.protocol_type, "udp-port-label",
                                    base::DoNothing());
  crostini_port_forwarder_->AddPort(
      other_key.container_id, other_key.port_number, other_key.protocol_type,
      "other-port-label", base::DoNothing());
  EXPECT_EQ(crostini_port_forwarder_->GetNumberOfForwardedPortsForTesting(),
            3U);

  crostini_port_forwarder_->RemoveAllPorts(tcp_key.container_id);
  EXPECT_EQ(crostini_port_forwarder_->GetNumberOfForwardedPortsForTesting(),
            1U);  // The other container's ports are still active.
  MakePermissionBrokerPortForwardingExpectation(
      /*port_number=*/5000, /*protocol=*/CrostiniPortForwarder::Protocol::TCP,
      /*exists=*/false);
  MakePermissionBrokerPortForwardingExpectation(
      /*port_number=*/5000, /*protocol=*/CrostiniPortForwarder::Protocol::UDP,
      /*exists=*/false);
  MakePermissionBrokerPortForwardingExpectation(
      /*port_number=*/5001, /*protocol=*/CrostiniPortForwarder::Protocol::TCP,
      /*exists=*/true);
  MakePortPreferenceExpectation(tcp_key, /*exists=*/false,
                                /*label=*/"tcp-port-label");
  MakePortPreferenceExpectation(udp_key, /*exists=*/false,
                                /*label=*/"udp-port-label");
  MakePortPreferenceExpectation(other_key, /*exists=*/true,
                                /*label=*/"other-port-label");
}

TEST_F(CrostiniPortForwarderTest, GetActivePortsForUI) {
  CrostiniPortForwarder::PortRuleKey tcp_key = GetDefaultActiveTcpPortKey();
  CrostiniPortForwarder::PortRuleKey udp_key = GetDefaultActiveUdpPortKey();
  crostini_port_forwarder_->AddPort(tcp_key.container_id, tcp_key.port_number,
                                    tcp_key.protocol_type, "tcp-port-label",
                                    base::DoNothing());
  crostini_port_forwarder_->AddPort(udp_key.container_id, udp_key.port_number,
                                    udp_key.protocol_type, "udp-port-label",
                                    base::DoNothing());
  base::ListValue activePorts = crostini_port_forwarder_->GetActivePorts();
  EXPECT_EQ(crostini_port_forwarder_->GetNumberOfForwardedPortsForTesting(),
            activePorts.GetSize());
  for (const auto& port : activePorts) {
    crostini::CrostiniPortForwarder::PortRuleKey key = {
        .port_number = port.FindIntKey(kPortNumberKey).value(),
        .protocol_type = static_cast<crostini::CrostiniPortForwarder::Protocol>(
            port.FindIntKey(kPortProtocolKey).value()),
        .input_ifname = "all",
        .container_id = tcp_key.container_id,
    };
    EXPECT_FALSE(crostini_port_forwarder_->forwarded_ports_.find(key) ==
                 crostini_port_forwarder_->forwarded_ports_.end());
  }
}

}  // namespace crostini
