// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/mpris/mpris_service_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/containers/flat_map.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_task_environment.h"
#include "components/dbus/thread_linux/dbus_thread_linux.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_exported_object.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/mpris/mpris_service_observer.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::Unused;
using ::testing::WithArg;

namespace mpris {

namespace {

constexpr uint32_t kFakeSerial = 123;

}  // anonymous namespace

class MockMprisServiceObserver : public MprisServiceObserver {
 public:
  MockMprisServiceObserver() = default;
  ~MockMprisServiceObserver() override = default;

  // MprisServiceObserver implementation.
  MOCK_METHOD0(OnServiceReady, void());
  MOCK_METHOD0(OnNext, void());
  MOCK_METHOD0(OnPrevious, void());
  MOCK_METHOD0(OnPause, void());
  MOCK_METHOD0(OnPlayPause, void());
  MOCK_METHOD0(OnStop, void());
  MOCK_METHOD0(OnPlay, void());
};

class MprisServiceImplTest : public testing::Test, public MprisServiceObserver {
 public:
  MprisServiceImplTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::UI) {}
  ~MprisServiceImplTest() override = default;

  void SetUp() override { StartMprisServiceAndWaitForReady(); }

  void AddObserver(MockMprisServiceObserver* observer) {
    service_->AddObserver(observer);
  }

  void CallMediaPlayer2PlayerMethodAndBlock(const std::string& method_name) {
    EXPECT_TRUE(player_interface_exported_methods_.contains(method_name));

    response_wait_loop_ = std::make_unique<base::RunLoop>();

    // We need to supply a serial or the test will crash.
    dbus::MethodCall method_call(kMprisAPIPlayerInterfaceName, method_name);
    method_call.SetSerial(kFakeSerial);

    // Call the method and await a response.
    player_interface_exported_methods_[method_name].Run(
        &method_call, base::BindRepeating(&MprisServiceImplTest::OnResponse,
                                          base::Unretained(this)));
    response_wait_loop_->Run();
  }

  MprisService* GetService() { return service_.get(); }

  dbus::MockExportedObject* GetExportedObject() {
    return mock_exported_object_.get();
  }

 private:
  void StartMprisServiceAndWaitForReady() {
    service_wait_loop_ = std::make_unique<base::RunLoop>();
    service_ = std::make_unique<mpris::MprisServiceImpl>();

    SetUpMocks();

    service_->SetBusForTesting(mock_bus_);
    service_->AddObserver(this);
    service_->StartService();
    service_wait_loop_->Run();
  }

  // Sets up the mock Bus and ExportedObject. The ExportedObject will store the
  // org.mpris.MediaPlayer2.Player exported methods in the
  // |player_interface_exported_methods_| map so we can call them for testing.
  void SetUpMocks() {
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SESSION;
    options.connection_type = dbus::Bus::PRIVATE;
    options.dbus_task_runner = dbus_thread_linux::GetTaskRunner();
    mock_bus_ = base::MakeRefCounted<dbus::MockBus>(options);
    mock_exported_object_ = base::MakeRefCounted<dbus::MockExportedObject>(
        mock_bus_.get(), dbus::ObjectPath(kMprisAPIObjectPath));

    EXPECT_CALL(*mock_bus_,
                GetExportedObject(dbus::ObjectPath(kMprisAPIObjectPath)))
        .WillOnce(Return(mock_exported_object_.get()));
    EXPECT_CALL(*mock_bus_, RequestOwnership(service_->GetServiceName(), _, _))
        .WillOnce(Invoke(this, &MprisServiceImplTest::OnOwnership));

    // The service must call ShutdownAndBlock in order to properly clean up the
    // DBus service.
    EXPECT_CALL(*mock_bus_, ShutdownAndBlock());

    EXPECT_CALL(*mock_exported_object_, ExportMethod(_, _, _, _))
        .WillRepeatedly(Invoke(this, &MprisServiceImplTest::OnExported));
  }

  // Tell the service that ownership was successful.
  void OnOwnership(const std::string& service_name,
                   Unused,
                   dbus::Bus::OnOwnershipCallback callback) {
    callback.Run(service_name, true);
  }

  // Store the exported method if necessary and tell the service that the export
  // was successful.
  void OnExported(const std::string& interface_name,
                  const std::string& method_name,
                  dbus::ExportedObject::MethodCallCallback exported_method,
                  dbus::ExportedObject::OnExportedCallback callback) {
    if (interface_name == kMprisAPIPlayerInterfaceName)
      player_interface_exported_methods_[method_name] = exported_method;
    callback.Run(interface_name, method_name, true);
  }

  void OnResponse(std::unique_ptr<dbus::Response> response) {
    // A response of nullptr means an error has occurred.
    EXPECT_NE(nullptr, response.get());
    if (response_wait_loop_)
      response_wait_loop_->Quit();
  }

  // mpris::MprisServiceObserver implementation.
  void OnServiceReady() override {
    if (service_wait_loop_)
      service_wait_loop_->Quit();
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<base::RunLoop> service_wait_loop_;
  std::unique_ptr<base::RunLoop> response_wait_loop_;
  std::unique_ptr<MprisServiceImpl> service_;
  scoped_refptr<dbus::MockBus> mock_bus_;
  scoped_refptr<dbus::MockExportedObject> mock_exported_object_;

  base::flat_map<std::string, dbus::ExportedObject::MethodCallCallback>
      player_interface_exported_methods_;

  DISALLOW_COPY_AND_ASSIGN(MprisServiceImplTest);
};

TEST_F(MprisServiceImplTest, ObserverNotifiedOfServiceReadyWhenAdded) {
  MockMprisServiceObserver observer;
  EXPECT_CALL(observer, OnServiceReady());
  AddObserver(&observer);
}

TEST_F(MprisServiceImplTest, ObserverNotifiedOfNextCalls) {
  MockMprisServiceObserver observer;
  EXPECT_CALL(observer, OnNext());
  AddObserver(&observer);
  CallMediaPlayer2PlayerMethodAndBlock("Next");
}

TEST_F(MprisServiceImplTest, ObserverNotifiedOfPreviousCalls) {
  MockMprisServiceObserver observer;
  EXPECT_CALL(observer, OnPrevious());
  AddObserver(&observer);
  CallMediaPlayer2PlayerMethodAndBlock("Previous");
}

TEST_F(MprisServiceImplTest, ObserverNotifiedOfPauseCalls) {
  MockMprisServiceObserver observer;
  EXPECT_CALL(observer, OnPause());
  AddObserver(&observer);
  CallMediaPlayer2PlayerMethodAndBlock("Pause");
}

TEST_F(MprisServiceImplTest, ObserverNotifiedOfPlayPauseCalls) {
  MockMprisServiceObserver observer;
  EXPECT_CALL(observer, OnPlayPause());
  AddObserver(&observer);
  CallMediaPlayer2PlayerMethodAndBlock("PlayPause");
}

TEST_F(MprisServiceImplTest, ObserverNotifiedOfStopCalls) {
  MockMprisServiceObserver observer;
  EXPECT_CALL(observer, OnStop());
  AddObserver(&observer);
  CallMediaPlayer2PlayerMethodAndBlock("Stop");
}

TEST_F(MprisServiceImplTest, ObserverNotifiedOfPlayCalls) {
  MockMprisServiceObserver observer;
  EXPECT_CALL(observer, OnPlay());
  AddObserver(&observer);
  CallMediaPlayer2PlayerMethodAndBlock("Play");
}

TEST_F(MprisServiceImplTest, ChangingPropertyEmitsSignal) {
  base::RunLoop wait_for_signal;

  // The returned signal should give the changed property.
  EXPECT_CALL(*GetExportedObject(), SendSignal(_))
      .WillOnce(WithArg<0>([&wait_for_signal](dbus::Signal* signal) {
        EXPECT_NE(nullptr, signal);
        dbus::MessageReader reader(signal);

        std::string interface_name;
        ASSERT_TRUE(reader.PopString(&interface_name));
        EXPECT_EQ(kMprisAPIPlayerInterfaceName, interface_name);

        dbus::MessageReader changed_properties_reader(nullptr);
        ASSERT_TRUE(reader.PopArray(&changed_properties_reader));

        dbus::MessageReader dict_entry_reader(nullptr);
        ASSERT_TRUE(changed_properties_reader.PopDictEntry(&dict_entry_reader));

        // The changed property name should be "CanPlay".
        std::string property_name;
        ASSERT_TRUE(dict_entry_reader.PopString(&property_name));
        EXPECT_EQ("CanPlay", property_name);

        // The new value should be true.
        bool value;
        ASSERT_TRUE(dict_entry_reader.PopVariantOfBool(&value));
        EXPECT_EQ(true, value);

        // CanPlay should be the only entry.
        EXPECT_FALSE(changed_properties_reader.HasMoreData());

        wait_for_signal.Quit();
      }));

  // CanPlay is initialized as false, so setting it to true should emit an
  // org.freedesktop.DBus.Properties.PropertiesChanged signal.
  GetService()->SetCanPlay(true);
  wait_for_signal.Run();

  // Setting it to true again should not re-signal.
  GetService()->SetCanPlay(true);
}

TEST_F(MprisServiceImplTest, ChangingMetadataEmitsSignal) {
  base::RunLoop wait_for_signal;

  // The returned signal should give the changed property.
  EXPECT_CALL(*GetExportedObject(), SendSignal(_))
      .WillOnce(WithArg<0>([&wait_for_signal](dbus::Signal* signal) {
        ASSERT_NE(nullptr, signal);
        dbus::MessageReader reader(signal);

        std::string interface_name;
        ASSERT_TRUE(reader.PopString(&interface_name));
        EXPECT_EQ(kMprisAPIPlayerInterfaceName, interface_name);

        dbus::MessageReader changed_properties_reader(nullptr);
        ASSERT_TRUE(reader.PopArray(&changed_properties_reader));

        dbus::MessageReader dict_entry_reader(nullptr);
        ASSERT_TRUE(changed_properties_reader.PopDictEntry(&dict_entry_reader));

        // The changed property name should be "Metadata".
        std::string property_name;
        ASSERT_TRUE(dict_entry_reader.PopString(&property_name));
        EXPECT_EQ("Metadata", property_name);

        // The new metadata should have the new title.
        dbus::MessageReader metadata_variant_reader(nullptr);
        ASSERT_TRUE(dict_entry_reader.PopVariant(&metadata_variant_reader));
        dbus::MessageReader metadata_reader(nullptr);
        ASSERT_TRUE(metadata_variant_reader.PopArray(&metadata_reader));

        dbus::MessageReader metadata_entry_reader(nullptr);
        ASSERT_TRUE(metadata_reader.PopDictEntry(&metadata_entry_reader));

        std::string metadata_property_name;
        ASSERT_TRUE(metadata_entry_reader.PopString(&metadata_property_name));
        EXPECT_EQ("xesam:title", metadata_property_name);

        std::string value;
        ASSERT_TRUE(metadata_entry_reader.PopVariantOfString(&value));
        EXPECT_EQ("Foo", value);

        // Metadata should be the only changed property.
        EXPECT_FALSE(changed_properties_reader.HasMoreData());

        wait_for_signal.Quit();
      }));

  // Setting the title should emit an
  // org.freedesktop.DBus.Properties.PropertiesChanged signal.
  GetService()->SetTitle(base::ASCIIToUTF16("Foo"));
  wait_for_signal.Run();

  // Setting the title to the same value as before should not emit a new signal.
  GetService()->SetTitle(base::ASCIIToUTF16("Foo"));
}

}  // namespace mpris
