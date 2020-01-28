// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_port_forwarder.h"

#include "chrome/browser/chromeos/crostini/crostini_test_helper.h"
#include "chrome/test/base/testing_profile.h"
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
  CrostiniPortForwarderTest() {}

  ~CrostiniPortForwarderTest() override {}

  void SetUp() override {
    chromeos::PermissionBrokerClient::InitializeFake();
    profile_ = std::make_unique<TestingProfile>();
    test_helper_ = std::make_unique<CrostiniTestHelper>(profile_.get());
    crostini_port_forwarder_ =
        std::make_unique<CrostiniPortForwarder>(profile());
  }

  void TearDown() override {
    chromeos::PermissionBrokerClient::Shutdown();
    crostini_port_forwarder_.reset();
    test_helper_.reset();
    profile_.reset();
  }

 protected:
  Profile* profile() { return profile_.get(); }

  std::unique_ptr<CrostiniTestHelper> test_helper_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<CrostiniPortForwarder> crostini_port_forwarder_;
  content::BrowserTaskEnvironment task_environment_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CrostiniPortForwarderTest);
};

TEST_F(CrostiniPortForwarderTest, AddPortTcpSuccess) {
  EXPECT_FALSE(chromeos::FakePermissionBrokerClient::Get()->HasTcpPortForward(
      5000, "wlan0"));
  EXPECT_FALSE(chromeos::FakePermissionBrokerClient::Get()->HasUdpPortForward(
      5000, "wlan0"));

  bool success = false;
  crostini_port_forwarder_->AddPort(5000, CrostiniPortForwarder::Protocol::TCP,
                                    "label0",
                                    base::BindOnce(&TestingCallback, &success));
  EXPECT_TRUE(success);
  EXPECT_TRUE(chromeos::FakePermissionBrokerClient::Get()->HasTcpPortForward(
      5000, "wlan0"));
  EXPECT_FALSE(chromeos::FakePermissionBrokerClient::Get()->HasUdpPortForward(
      5000, "wlan0"));
}

TEST_F(CrostiniPortForwarderTest, AddPortUdpSuccess) {
  EXPECT_FALSE(chromeos::FakePermissionBrokerClient::Get()->HasTcpPortForward(
      5000, "wlan0"));
  EXPECT_FALSE(chromeos::FakePermissionBrokerClient::Get()->HasUdpPortForward(
      5000, "wlan0"));

  bool success = false;
  crostini_port_forwarder_->AddPort(5000, CrostiniPortForwarder::Protocol::UDP,
                                    "label0",
                                    base::BindOnce(&TestingCallback, &success));
  EXPECT_TRUE(success);
  EXPECT_TRUE(chromeos::FakePermissionBrokerClient::Get()->HasUdpPortForward(
      5000, "wlan0"));
  EXPECT_FALSE(chromeos::FakePermissionBrokerClient::Get()->HasTcpPortForward(
      5000, "wlan0"));
}

TEST_F(CrostiniPortForwarderTest, AddPortDuplicateFail) {
  EXPECT_FALSE(chromeos::FakePermissionBrokerClient::Get()->HasTcpPortForward(
      5000, "wlan0"));
  EXPECT_FALSE(chromeos::FakePermissionBrokerClient::Get()->HasUdpPortForward(
      5000, "wlan0"));

  bool success = false;
  crostini_port_forwarder_->AddPort(5000, CrostiniPortForwarder::Protocol::UDP,
                                    "label0",
                                    base::BindOnce(&TestingCallback, &success));
  EXPECT_TRUE(success);
  EXPECT_TRUE(chromeos::FakePermissionBrokerClient::Get()->HasUdpPortForward(
      5000, "wlan0"));
  EXPECT_EQ(crostini_port_forwarder_->forwarded_ports_.size(), 1U);

  // Leave success as == true.
  crostini_port_forwarder_->AddPort(5000, CrostiniPortForwarder::Protocol::UDP,
                                    "label1",
                                    base::BindOnce(&TestingCallback, &success));
  EXPECT_FALSE(success);
  EXPECT_FALSE(chromeos::FakePermissionBrokerClient::Get()->HasTcpPortForward(
      5000, "wlan0"));
  EXPECT_TRUE(chromeos::FakePermissionBrokerClient::Get()->HasUdpPortForward(
      5000, "wlan0"));
  EXPECT_EQ(crostini_port_forwarder_->forwarded_ports_.size(), 1U);
}

TEST_F(CrostiniPortForwarderTest, AddPortUdpAndTcpSuccess) {
  EXPECT_FALSE(chromeos::FakePermissionBrokerClient::Get()->HasTcpPortForward(
      5000, "wlan0"));
  EXPECT_FALSE(chromeos::FakePermissionBrokerClient::Get()->HasUdpPortForward(
      5000, "wlan0"));

  bool success = false;
  crostini_port_forwarder_->AddPort(5000, CrostiniPortForwarder::Protocol::UDP,
                                    "label0",
                                    base::BindOnce(&TestingCallback, &success));
  EXPECT_TRUE(success);

  success = false;
  crostini_port_forwarder_->AddPort(5000, CrostiniPortForwarder::Protocol::TCP,
                                    "label0",
                                    base::BindOnce(&TestingCallback, &success));
  EXPECT_TRUE(success);
  EXPECT_EQ(crostini_port_forwarder_->forwarded_ports_.size(), 2U);
}

TEST_F(CrostiniPortForwarderTest, AddPortMultipleSuccess) {
  EXPECT_EQ(crostini_port_forwarder_->forwarded_ports_.size(), 0U);
  crostini_port_forwarder_->AddPort(5000, CrostiniPortForwarder::Protocol::UDP,
                                    "label0", base::DoNothing());
  crostini_port_forwarder_->AddPort(5001, CrostiniPortForwarder::Protocol::TCP,
                                    "label1", base::DoNothing());
  crostini_port_forwarder_->AddPort(5002, CrostiniPortForwarder::Protocol::UDP,
                                    "label2", base::DoNothing());
  EXPECT_EQ(crostini_port_forwarder_->forwarded_ports_.size(), 3U);
}

TEST_F(CrostiniPortForwarderTest, ActivatePortTcpSuccess) {
  EXPECT_FALSE(chromeos::FakePermissionBrokerClient::Get()->HasTcpPortForward(
      5000, "wlan0"));
  EXPECT_FALSE(chromeos::FakePermissionBrokerClient::Get()->HasUdpPortForward(
      5000, "wlan0"));

  bool success = false;
  crostini_port_forwarder_->ActivatePort(
      5000, base::BindOnce(&TestingCallback, &success));
  EXPECT_TRUE(success);
  EXPECT_TRUE(chromeos::FakePermissionBrokerClient::Get()->HasTcpPortForward(
      5000, "wlan0"));
  EXPECT_FALSE(chromeos::FakePermissionBrokerClient::Get()->HasUdpPortForward(
      5000, "wlan0"));
}

TEST_F(CrostiniPortForwarderTest, TryActivatePortPermissionBrokerClientFail) {
  bool success = false;
  crostini_port_forwarder_->TryActivatePort(
      5000, CrostiniPortForwarder::Protocol::TCP, "label0",
      base::BindOnce(&TestingCallback, &success));
  EXPECT_TRUE(success);
  EXPECT_EQ(crostini_port_forwarder_->forwarded_ports_.size(), 1U);
  chromeos::PermissionBrokerClient::Shutdown();

  // Leave success as == true.
  crostini_port_forwarder_->TryActivatePort(
      5001, CrostiniPortForwarder::Protocol::TCP, "label1",
      base::BindOnce(&TestingCallback, &success));
  EXPECT_FALSE(success);
  EXPECT_EQ(crostini_port_forwarder_->forwarded_ports_.size(), 1U);
  // Re-initialize otherwise Shutdown in TearDown phase will break.
  chromeos::PermissionBrokerClient::InitializeFake();
}

}  // namespace crostini
