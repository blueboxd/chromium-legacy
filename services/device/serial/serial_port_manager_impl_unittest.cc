// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/serial/serial_port_manager_impl.h"

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/threading/thread.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/device/device_service_test_base.h"
#include "services/device/public/cpp/serial/serial_switches.h"
#include "services/device/public/mojom/serial.mojom.h"
#include "services/device/serial/bluetooth_serial_device_enumerator.h"
#include "services/device/serial/fake_serial_device_enumerator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;

namespace device {

namespace {

const base::FilePath kFakeDevicePath1(FILE_PATH_LITERAL("/dev/fakeserialmojo"));
const base::FilePath kFakeDevicePath2(FILE_PATH_LITERAL("\\\\COM800\\"));
constexpr char kDeviceAddress[] = "00:00:00:00:00:00";

class MockSerialPortManagerClient : public mojom::SerialPortManagerClient {
 public:
  MockSerialPortManagerClient() = default;
  MockSerialPortManagerClient(const MockSerialPortManagerClient&) = delete;
  MockSerialPortManagerClient& operator=(const MockSerialPortManagerClient&) =
      delete;
  ~MockSerialPortManagerClient() override = default;

  mojo::PendingRemote<mojom::SerialPortManagerClient>
  BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  // mojom::SerialPortManagerClient
  MOCK_METHOD1(OnPortAdded, void(mojom::SerialPortInfoPtr));
  MOCK_METHOD1(OnPortRemoved, void(mojom::SerialPortInfoPtr));

 private:
  mojo::Receiver<mojom::SerialPortManagerClient> receiver_{this};
};

}  // namespace

class SerialPortManagerImplTest : public DeviceServiceTestBase {
 public:
  SerialPortManagerImplTest() {
    auto enumerator = std::make_unique<FakeSerialEnumerator>();
    enumerator_ = enumerator.get();
    enumerator_->AddDevicePath(kFakeDevicePath1);
    enumerator_->AddDevicePath(kFakeDevicePath2);

    manager_ = std::make_unique<SerialPortManagerImpl>(
        io_task_runner_, base::ThreadTaskRunnerHandle::Get());
    manager_->SetSerialEnumeratorForTesting(std::move(enumerator));
  }

  ~SerialPortManagerImplTest() override = default;

  // Since not all functions need to use a MockBluetoothAdapter, this function
  // is called at the beginning of test cases that do require a
  // MockBluetoothAdapter.
  void SetupBluetoothEnumerator() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableBluetoothSerialPortProfileInSerialApi);

    ON_CALL(*adapter_, GetDevices())
        .WillByDefault(
            Invoke(adapter_.get(), &MockBluetoothAdapter::GetConstMockDevices));
    device::BluetoothAdapterFactory::SetAdapterForTesting(adapter_);

    auto mock_device = std::make_unique<MockBluetoothDevice>(
        adapter_.get(), 0, "Test Device", kDeviceAddress, false, false);
    static const BluetoothUUID kSerialPortProfileUUID("1101");
    mock_device->AddUUID(kSerialPortProfileUUID);
    adapter_->AddMockDevice(std::move(mock_device));

    auto bluetooth_enumerator =
        std::make_unique<BluetoothSerialDeviceEnumerator>();
    bluetooth_enumerator_ = bluetooth_enumerator.get();

    manager_->SetBluetoothSerialEnumeratorForTesting(
        std::move(bluetooth_enumerator));
  }

 protected:
  FakeSerialEnumerator* enumerator_;
  BluetoothSerialDeviceEnumerator* bluetooth_enumerator_;
  scoped_refptr<MockBluetoothAdapter> adapter_ =
      base::MakeRefCounted<MockBluetoothAdapter>();

  void Bind(mojo::PendingReceiver<mojom::SerialPortManager> receiver) {
    manager_->Bind(std::move(receiver));
  }

 private:
  std::unique_ptr<SerialPortManagerImpl> manager_;

  DISALLOW_COPY_AND_ASSIGN(SerialPortManagerImplTest);
};

// This is to simply test that we can enumerate devices on the platform without
// hanging or crashing.
TEST_F(SerialPortManagerImplTest, SimpleConnectTest) {
  // DeviceService has its own instance of SerialPortManagerImpl that is used to
  // bind the receiver over the one created for this test.
  mojo::Remote<mojom::SerialPortManager> port_manager;
  device_service()->BindSerialPortManager(
      port_manager.BindNewPipeAndPassReceiver());

  base::RunLoop loop;
  port_manager->GetDevices(base::BindLambdaForTesting(
      [&](std::vector<mojom::SerialPortInfoPtr> results) {
        for (auto& device : results) {
          mojo::Remote<mojom::SerialPort> serial_port;
          port_manager->GetPort(device->token,
                                /*use_alternate_path=*/false,
                                serial_port.BindNewPipeAndPassReceiver(),
                                /*watcher=*/mojo::NullRemote());
          // Send a message on the pipe and wait for the response to make sure
          // that the interface request was bound successfully.
          serial_port.FlushForTesting();
          EXPECT_TRUE(serial_port.is_connected());
        }
        loop.Quit();
      }));
  loop.Run();
}

TEST_F(SerialPortManagerImplTest, GetDevices) {
  SetupBluetoothEnumerator();
  mojo::Remote<mojom::SerialPortManager> port_manager;
  Bind(port_manager.BindNewPipeAndPassReceiver());
  const std::string address_identifier =
      std::string(kDeviceAddress) + "-Identifier";
  const std::set<base::FilePath> expected_paths = {
      kFakeDevicePath1, kFakeDevicePath2,
      base::FilePath::FromUTF8Unsafe(address_identifier)};

  base::RunLoop loop;
  port_manager->GetDevices(base::BindLambdaForTesting(
      [&](std::vector<mojom::SerialPortInfoPtr> results) {
        EXPECT_EQ(expected_paths.size(), results.size());
        std::set<base::FilePath> actual_paths;
        for (size_t i = 0; i < results.size(); ++i)
          actual_paths.insert(results[i]->path);
        EXPECT_EQ(expected_paths, actual_paths);
        loop.Quit();
      }));
  loop.Run();
}

TEST_F(SerialPortManagerImplTest, PortRemovedAndAdded) {
  SetupBluetoothEnumerator();
  mojo::Remote<mojom::SerialPortManager> port_manager;
  Bind(port_manager.BindNewPipeAndPassReceiver());

  MockSerialPortManagerClient client;
  port_manager->SetClient(client.BindNewPipeAndPassRemote());

  base::UnguessableToken port1_token;
  {
    base::RunLoop run_loop;
    port_manager->GetDevices(base::BindLambdaForTesting(
        [&](std::vector<mojom::SerialPortInfoPtr> results) {
          for (const auto& port : results) {
            if (port->path == kFakeDevicePath1) {
              port1_token = port->token;
              break;
            }
          }
          run_loop.Quit();
        }));
    run_loop.Run();
  }
  ASSERT_FALSE(port1_token.is_empty());

  enumerator_->RemoveDevicePath(kFakeDevicePath1);
  {
    base::RunLoop run_loop;
    EXPECT_CALL(client, OnPortRemoved(_))
        .WillOnce(Invoke([&](mojom::SerialPortInfoPtr port) {
          EXPECT_EQ(port1_token, port->token);
          EXPECT_EQ(kFakeDevicePath1, port->path);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  enumerator_->AddDevicePath(kFakeDevicePath1);
  {
    base::RunLoop run_loop;
    EXPECT_CALL(client, OnPortAdded(_))
        .WillOnce(Invoke([&](mojom::SerialPortInfoPtr port) {
          EXPECT_NE(port1_token, port->token);
          EXPECT_EQ(kFakeDevicePath1, port->path);
          run_loop.Quit();
        }));
    run_loop.Run();
  }
}

TEST_F(SerialPortManagerImplTest, GetPort) {
  SetupBluetoothEnumerator();
  mojo::Remote<mojom::SerialPortManager> port_manager;
  Bind(port_manager.BindNewPipeAndPassReceiver());

  base::RunLoop loop;
  port_manager->GetDevices(base::BindLambdaForTesting(
      [&](std::vector<mojom::SerialPortInfoPtr> results) {
        EXPECT_GT(results.size(), 0u);

        mojo::Remote<mojom::SerialPort> serial_port;
        port_manager->GetPort(results[0]->token,
                              /*use_alternate_path=*/false,
                              serial_port.BindNewPipeAndPassReceiver(),
                              /*watcher=*/mojo::NullRemote());
        // Send a message on the pipe and wait for the response to make sure
        // that the interface request was bound successfully.
        serial_port.FlushForTesting();
        EXPECT_TRUE(serial_port.is_connected());
        loop.Quit();
      }));
  loop.Run();
}

TEST_F(SerialPortManagerImplTest, BluetoothPortRemovedAndAdded) {
  SetupBluetoothEnumerator();
  mojo::Remote<mojom::SerialPortManager> port_manager;
  Bind(port_manager.BindNewPipeAndPassReceiver());

  MockSerialPortManagerClient client;
  port_manager->SetClient(client.BindNewPipeAndPassRemote());

  const std::string address_identifier =
      std::string(kDeviceAddress) + "-Identifier";
  base::UnguessableToken port1_token;
  {
    base::RunLoop run_loop;
    port_manager->GetDevices(base::BindLambdaForTesting(
        [&](std::vector<mojom::SerialPortInfoPtr> results) {
          for (const auto& port : results) {
            if (port->path ==
                base::FilePath::FromUTF8Unsafe(address_identifier)) {
              port1_token = port->token;
              break;
            }
          }
          run_loop.Quit();
        }));
    run_loop.Run();
  }
  ASSERT_FALSE(port1_token.is_empty());

  bluetooth_enumerator_->DeviceRemoved(
      adapter_.get(), adapter_->RemoveMockDevice(kDeviceAddress).get());
  {
    base::RunLoop run_loop;
    EXPECT_CALL(client, OnPortRemoved(_))
        .WillOnce(Invoke([&](mojom::SerialPortInfoPtr port) {
          EXPECT_EQ(port1_token, port->token);
          EXPECT_EQ(port->path,
                    base::FilePath::FromUTF8Unsafe(address_identifier));
          EXPECT_EQ(mojom::DeviceType::SPP_DEVICE, port->type);
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  auto mock_device = std::make_unique<MockBluetoothDevice>(
      adapter_.get(), 0, "Test Device", kDeviceAddress, false, false);
  static const BluetoothUUID kSerialPortProfileUUID("1101");
  mock_device->AddUUID(kSerialPortProfileUUID);
  MockBluetoothDevice* mock_device_ptr = mock_device.get();
  adapter_->AddMockDevice(std::move(mock_device));

  bluetooth_enumerator_->DeviceAdded(adapter_.get(), mock_device_ptr);
  {
    base::RunLoop run_loop;
    EXPECT_CALL(client, OnPortAdded(_))
        .WillOnce(Invoke([&](mojom::SerialPortInfoPtr port) {
          EXPECT_NE(port1_token, port->token);
          EXPECT_EQ(port->path,
                    base::FilePath::FromUTF8Unsafe(address_identifier));
          EXPECT_EQ(mojom::DeviceType::SPP_DEVICE, port->type);
          run_loop.Quit();
        }));
    run_loop.Run();
  }
}

}  // namespace device
