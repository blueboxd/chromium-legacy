// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/serial/serial_chooser_context.h"

#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/serial/serial_chooser_context_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/permissions/test/chooser_context_base_mock_permission_observer.h"
#include "content/public/test/browser_task_environment.h"
#include "services/device/public/cpp/test/fake_serial_port_manager.h"
#include "services/device/public/mojom/serial.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockPortObserver : public SerialChooserContext::PortObserver {
 public:
  MockPortObserver() = default;
  MockPortObserver(MockPortObserver&) = delete;
  MockPortObserver& operator=(MockPortObserver&) = delete;
  ~MockPortObserver() override = default;

  MOCK_METHOD1(OnPortAdded, void(const device::mojom::SerialPortInfo&));
  MOCK_METHOD1(OnPortRemoved, void(const device::mojom::SerialPortInfo&));
  MOCK_METHOD0(OnPortManagerConnectionError, void());
};

class SerialChooserContextTest : public testing::Test {
 public:
  SerialChooserContextTest() {
    mojo::PendingRemote<device::mojom::SerialPortManager> port_manager;
    port_manager_.AddReceiver(port_manager.InitWithNewPipeAndPassReceiver());

    context_ = SerialChooserContextFactory::GetForProfile(&profile_);
    context_->SetPortManagerForTesting(std::move(port_manager));
    scoped_permission_observer_.Add(context_);
    scoped_port_observer_.Add(context_);

    // Ensure |context_| is ready to receive SerialPortManagerClient messages.
    context_->FlushPortManagerConnectionForTesting();
  }

  ~SerialChooserContextTest() override = default;

  // Disallow copy and assignment.
  SerialChooserContextTest(SerialChooserContextTest&) = delete;
  SerialChooserContextTest& operator=(SerialChooserContextTest&) = delete;

  device::FakeSerialPortManager& port_manager() { return port_manager_; }
  Profile* profile() { return &profile_; }
  SerialChooserContext* context() { return context_; }
  permissions::MockPermissionObserver& permission_observer() {
    return permission_observer_;
  }
  MockPortObserver& port_observer() { return port_observer_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  device::FakeSerialPortManager port_manager_;
  TestingProfile profile_;
  SerialChooserContext* context_;
  permissions::MockPermissionObserver permission_observer_;
  ScopedObserver<permissions::ChooserContextBase,
                 permissions::ChooserContextBase::PermissionObserver>
      scoped_permission_observer_{&permission_observer_};
  MockPortObserver port_observer_;
  ScopedObserver<SerialChooserContext,
                 SerialChooserContext::PortObserver,
                 &SerialChooserContext::AddPortObserver,
                 &SerialChooserContext::RemovePortObserver>
      scoped_port_observer_{&port_observer_};
};

}  // namespace

TEST_F(SerialChooserContextTest, GrantAndRevokeEphemeralPermission) {
  const auto origin = url::Origin::Create(GURL("https://google.com"));

  auto port = device::mojom::SerialPortInfo::New();
  port->token = base::UnguessableToken::Create();

  EXPECT_FALSE(context()->HasPortPermission(origin, origin, *port));

  EXPECT_CALL(permission_observer(),
              OnChooserObjectPermissionChanged(
                  ContentSettingsType::SERIAL_GUARD,
                  ContentSettingsType::SERIAL_CHOOSER_DATA));

  context()->GrantPortPermission(origin, origin, *port);
  EXPECT_TRUE(context()->HasPortPermission(origin, origin, *port));

  std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
      origin_objects = context()->GetGrantedObjects(origin, origin);
  ASSERT_EQ(1u, origin_objects.size());

  std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
      objects = context()->GetAllGrantedObjects();
  ASSERT_EQ(1u, objects.size());
  EXPECT_EQ(origin.GetURL(), objects[0]->requesting_origin);
  EXPECT_EQ(origin.GetURL(), objects[0]->embedding_origin);
  EXPECT_EQ(origin_objects[0]->value, objects[0]->value);
  EXPECT_EQ(content_settings::SettingSource::SETTING_SOURCE_USER,
            objects[0]->source);
  EXPECT_FALSE(objects[0]->incognito);

  EXPECT_CALL(permission_observer(),
              OnChooserObjectPermissionChanged(
                  ContentSettingsType::SERIAL_GUARD,
                  ContentSettingsType::SERIAL_CHOOSER_DATA));
  EXPECT_CALL(permission_observer(), OnPermissionRevoked(origin, origin));

  context()->RevokeObjectPermission(origin, origin, objects[0]->value);
  EXPECT_FALSE(context()->HasPortPermission(origin, origin, *port));
  origin_objects = context()->GetGrantedObjects(origin, origin);
  EXPECT_EQ(0u, origin_objects.size());
  objects = context()->GetAllGrantedObjects();
  EXPECT_EQ(0u, objects.size());
}

TEST_F(SerialChooserContextTest, EphemeralPermissionRevokedOnDisconnect) {
  const auto origin = url::Origin::Create(GURL("https://google.com"));

  auto port = device::mojom::SerialPortInfo::New();
  port->token = base::UnguessableToken::Create();
  port_manager().AddPort(port.Clone());

  context()->GrantPortPermission(origin, origin, *port);
  EXPECT_TRUE(context()->HasPortPermission(origin, origin, *port));

  EXPECT_CALL(permission_observer(),
              OnChooserObjectPermissionChanged(
                  ContentSettingsType::SERIAL_GUARD,
                  ContentSettingsType::SERIAL_CHOOSER_DATA));
  EXPECT_CALL(permission_observer(), OnPermissionRevoked(origin, origin));

  port_manager().RemovePort(port->token);
  {
    base::RunLoop run_loop;
    EXPECT_CALL(port_observer(), OnPortRemoved(testing::_))
        .WillOnce(
            testing::Invoke([&](const device::mojom::SerialPortInfo& info) {
              EXPECT_EQ(port->token, info.token);
              EXPECT_TRUE(context()->HasPortPermission(origin, origin, info));
              run_loop.Quit();
            }));
    run_loop.Run();
  }
  EXPECT_FALSE(context()->HasPortPermission(origin, origin, *port));
  auto origin_objects = context()->GetGrantedObjects(origin, origin);
  EXPECT_EQ(0u, origin_objects.size());
  auto objects = context()->GetAllGrantedObjects();
  EXPECT_EQ(0u, objects.size());
}

TEST_F(SerialChooserContextTest, GuardPermission) {
  const auto origin = url::Origin::Create(GURL("https://google.com"));

  auto port = device::mojom::SerialPortInfo::New();
  port->token = base::UnguessableToken::Create();

  context()->GrantPortPermission(origin, origin, *port);
  EXPECT_TRUE(context()->HasPortPermission(origin, origin, *port));

  auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
  map->SetContentSettingDefaultScope(origin.GetURL(), origin.GetURL(),
                                     ContentSettingsType::SERIAL_GUARD,
                                     std::string(), CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(context()->HasPortPermission(origin, origin, *port));

  std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
      objects = context()->GetGrantedObjects(origin, origin);
  EXPECT_EQ(0u, objects.size());

  std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
      all_origin_objects = context()->GetAllGrantedObjects();
  EXPECT_EQ(0u, all_origin_objects.size());
}
