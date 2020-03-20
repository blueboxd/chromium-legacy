// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/serial/serial_port_manager_impl.h"

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/threading/thread.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/device/device_service_test_base.h"
#include "services/device/public/mojom/serial.mojom.h"
#include "services/device/serial/fake_serial_device_enumerator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;

namespace device {

namespace {

const base::FilePath kFakeDevicePath1(FILE_PATH_LITERAL("/dev/fakeserialmojo"));
const base::FilePath kFakeDevicePath2(FILE_PATH_LITERAL("\\\\COM800\\"));

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

 protected:
  FakeSerialEnumerator* enumerator_;

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
  mojo::Remote<mojom::SerialPortManager> port_manager;
  device_service()->BindSerialPortManager(
      port_manager.BindNewPipeAndPassReceiver());

  base::RunLoop loop;
  port_manager->GetDevices(base::BindLambdaForTesting(
      [&](std::vector<mojom::SerialPortInfoPtr> results) {
        for (auto& device : results) {
          mojo::Remote<mojom::SerialPort> serial_port;
          port_manager->GetPort(device->token,
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
  mojo::Remote<mojom::SerialPortManager> port_manager;
  Bind(port_manager.BindNewPipeAndPassReceiver());
  const std::set<base::FilePath> expected_paths = {kFakeDevicePath1,
                                                   kFakeDevicePath2};

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
  mojo::Remote<mojom::SerialPortManager> port_manager;
  Bind(port_manager.BindNewPipeAndPassReceiver());

  base::RunLoop loop;
  port_manager->GetDevices(base::BindLambdaForTesting(
      [&](std::vector<mojom::SerialPortInfoPtr> results) {
        EXPECT_GT(results.size(), 0u);

        mojo::Remote<mojom::SerialPort> serial_port;
        port_manager->GetPort(results[0]->token,
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

}  // namespace device
