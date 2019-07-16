// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/dbus/gesture_properties_service_provider.h"

#include "chromeos/dbus/services/service_provider_test_helper.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "gmock/gmock.h"
#include "services/service_manager/public/cpp/test/test_connector_factory.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/ozone/testhelpers/mock_gesture_properties_service.h"

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::InvokeArgument;
using ::testing::Unused;

namespace ash {

namespace {

uint32_t expect_uint32(dbus::MessageReader* reader) {
  uint32_t result;
  EXPECT_TRUE(reader->PopUint32(&result));
  return result;
}

int32_t expect_int32(dbus::MessageReader* reader) {
  int32_t result;
  EXPECT_TRUE(reader->PopInt32(&result));
  return result;
}

std::string expect_string(dbus::MessageReader* reader) {
  std::string result;
  EXPECT_TRUE(reader->PopString(&result));
  return result;
}

}  // namespace

class GesturePropertiesServiceProviderTest : public testing::Test {
 public:
  GesturePropertiesServiceProviderTest() {
    service_manager::TestConnectorFactory test_connector_factory;
    mock_service_ = std::make_unique<MockGesturePropertiesService>();
    ON_CALL(*mock_service_, ListDevices(_))
        .WillByDefault(Invoke(
            this, &GesturePropertiesServiceProviderTest::FakeListDevices));
    ON_CALL(*mock_service_, ListProperties(_, _))
        .WillByDefault(Invoke(
            this, &GesturePropertiesServiceProviderTest::FakeListProperties));

    service_provider_ = std::make_unique<GesturePropertiesServiceProvider>();
    service_provider_->set_service_for_test(mock_service_.get());
  }

  ~GesturePropertiesServiceProviderTest() override { test_helper_.TearDown(); }

  void FakeListDevices(
      ui::ozone::mojom::GesturePropertiesService::ListDevicesCallback
          callback) {
    std::move(callback).Run(list_devices_response_);
  }

  void FakeListProperties(
      Unused,
      ui::ozone::mojom::GesturePropertiesService::ListPropertiesCallback
          callback) {
    std::move(callback).Run(list_properties_response_);
  }

 protected:
  void CallDBusMethod(std::string name,
                      dbus::MethodCall* method_call,
                      std::unique_ptr<dbus::Response>& response) {
    test_helper_.SetUp(
        chromeos::kGesturePropertiesServiceName,
        dbus::ObjectPath(chromeos::kGesturePropertiesServicePath),
        chromeos::kGesturePropertiesServiceInterface, name,
        service_provider_.get());
    response = test_helper_.CallMethod(method_call);
    ASSERT_TRUE(response);
  }

  void CallWithoutParameters(std::string name,
                             std::unique_ptr<dbus::Response>& response) {
    dbus::MethodCall* method_call = new dbus::MethodCall(
        chromeos::kGesturePropertiesServiceInterface, name);
    CallDBusMethod(name, std::move(method_call), response);
  }

  void CheckMethodErrorsWithNoParameters(std::string name) {
    std::unique_ptr<dbus::Response> response = nullptr;
    CallWithoutParameters(name, response);
    EXPECT_EQ(dbus::Message::MESSAGE_ERROR, response->GetMessageType());
  }

  base::flat_map<int, std::string> list_devices_response_ = {};
  std::vector<std::string> list_properties_response_ = {};

  std::unique_ptr<MockGesturePropertiesService> mock_service_;

  std::unique_ptr<GesturePropertiesServiceProvider> service_provider_;
  chromeos::ServiceProviderTestHelper test_helper_;

  DISALLOW_COPY_AND_ASSIGN(GesturePropertiesServiceProviderTest);
};

TEST_F(GesturePropertiesServiceProviderTest, ListDevicesEmpty) {
  list_devices_response_ = {};
  EXPECT_CALL(*mock_service_, ListDevices(_));

  std::unique_ptr<dbus::Response> response = nullptr;
  CallWithoutParameters(chromeos::kGesturePropertiesServiceListDevicesMethod,
                        response);

  dbus::MessageReader reader(response.get());
  EXPECT_EQ(0u, expect_uint32(&reader));
  dbus::MessageReader array_reader(nullptr);
  ASSERT_TRUE(reader.PopArray(&array_reader));
  EXPECT_FALSE(array_reader.HasMoreData());
  EXPECT_FALSE(reader.HasMoreData());
}

TEST_F(GesturePropertiesServiceProviderTest, ListDevicesSuccess) {
  list_devices_response_ = {
      {4, "dev 1"},
      {7, "dev 2"},
  };
  EXPECT_CALL(*mock_service_, ListDevices(_));

  std::unique_ptr<dbus::Response> response = nullptr;
  CallWithoutParameters(chromeos::kGesturePropertiesServiceListDevicesMethod,
                        response);

  dbus::MessageReader reader(response.get());
  EXPECT_EQ(2u, expect_uint32(&reader));
  dbus::MessageReader array_reader(nullptr);
  ASSERT_TRUE(reader.PopArray(&array_reader));

  dbus::MessageReader dict_entry_reader(nullptr);
  ASSERT_TRUE(array_reader.PopDictEntry(&dict_entry_reader));
  EXPECT_EQ(4, expect_int32(&dict_entry_reader));
  EXPECT_EQ("dev 1", expect_string(&dict_entry_reader));

  ASSERT_TRUE(array_reader.PopDictEntry(&dict_entry_reader));
  EXPECT_EQ(7, expect_int32(&dict_entry_reader));
  EXPECT_EQ("dev 2", expect_string(&dict_entry_reader));

  EXPECT_FALSE(array_reader.HasMoreData());
  EXPECT_FALSE(reader.HasMoreData());
}

}  // namespace ash
