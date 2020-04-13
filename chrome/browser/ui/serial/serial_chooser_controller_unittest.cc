// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/serial/serial_chooser_controller.h"

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/chooser_controller/mock_chooser_controller_view.h"
#include "chrome/browser/serial/serial_chooser_context.h"
#include "chrome/browser/serial/serial_chooser_context_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/device/public/cpp/test/fake_serial_port_manager.h"
#include "services/device/public/mojom/serial.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/serial/serial.mojom.h"

using testing::_;
using testing::Invoke;

class SerialChooserControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    mojo::PendingRemote<device::mojom::SerialPortManager> port_manager;
    port_manager_.AddReceiver(port_manager.InitWithNewPipeAndPassReceiver());
    SerialChooserContextFactory::GetForProfile(profile())
        ->SetPortManagerForTesting(std::move(port_manager));
  }

  device::FakeSerialPortManager& port_manager() { return port_manager_; }

 private:
  device::FakeSerialPortManager port_manager_;
};

TEST_F(SerialChooserControllerTest, GetPortsLateResponse) {
  std::vector<blink::mojom::SerialPortFilterPtr> filters;

  bool callback_run = false;
  auto callback = base::BindLambdaForTesting(
      [&](device::mojom::SerialPortInfoPtr port_info) {
        EXPECT_FALSE(port_info);
        callback_run = true;
      });

  auto controller = std::make_unique<SerialChooserController>(
      main_rfh(), std::move(filters), std::move(callback));
  controller.reset();

  // Allow any tasks posted by |controller| to run, such as asynchronous
  // requests to the Device Service to get the list of available serial ports.
  // These should be safely discarded since |controller| was destroyed.
  base::RunLoop().RunUntilIdle();

  // Even if |controller| is destroyed without user interaction the callback
  // should be run.
  EXPECT_TRUE(callback_run);
}

TEST_F(SerialChooserControllerTest, PortsAddedAndRemoved) {
  std::vector<blink::mojom::SerialPortFilterPtr> filters;
  auto controller = std::make_unique<SerialChooserController>(
      main_rfh(), std::move(filters), base::DoNothing());

  MockChooserControllerView view;
  controller->set_view(&view);

  {
    base::RunLoop run_loop;
    EXPECT_CALL(view, OnOptionsInitialized).WillOnce(Invoke([&] {
      run_loop.Quit();
    }));
    run_loop.Run();
  }
  EXPECT_EQ(0u, controller->NumOptions());

  auto port = device::mojom::SerialPortInfo::New();
  port->token = base::UnguessableToken::Create();
  port->display_name = "Test Port 1";
  port->path = base::FilePath(FILE_PATH_LITERAL("/dev/ttyS0"));
  base::UnguessableToken port1_token = port->token;
  port_manager().AddPort(std::move(port));
  {
    base::RunLoop run_loop;
    EXPECT_CALL(view, OnOptionAdded(_)).WillOnce(Invoke([&](size_t index) {
      EXPECT_EQ(0u, index);
      run_loop.Quit();
    }));
    run_loop.Run();
  }
  EXPECT_EQ(1u, controller->NumOptions());
  EXPECT_EQ(base::ASCIIToUTF16("Test Port 1 (ttyS0)"),
            controller->GetOption(0));

  port = device::mojom::SerialPortInfo::New();
  port->token = base::UnguessableToken::Create();
  port->display_name = "Test Port 2";
  port->path = base::FilePath(FILE_PATH_LITERAL("/dev/ttyS1"));
  port_manager().AddPort(std::move(port));
  {
    base::RunLoop run_loop;
    EXPECT_CALL(view, OnOptionAdded(_)).WillOnce(Invoke([&](size_t index) {
      EXPECT_EQ(1u, index);
      run_loop.Quit();
    }));
    run_loop.Run();
  }
  EXPECT_EQ(2u, controller->NumOptions());
  EXPECT_EQ(base::ASCIIToUTF16("Test Port 1 (ttyS0)"),
            controller->GetOption(0));
  EXPECT_EQ(base::ASCIIToUTF16("Test Port 2 (ttyS1)"),
            controller->GetOption(1));

  port_manager().RemovePort(port1_token);
  {
    base::RunLoop run_loop;
    EXPECT_CALL(view, OnOptionRemoved(_)).WillOnce(Invoke([&](size_t index) {
      EXPECT_EQ(0u, index);
      run_loop.Quit();
    }));
    run_loop.Run();
  }
  EXPECT_EQ(1u, controller->NumOptions());
  EXPECT_EQ(base::ASCIIToUTF16("Test Port 2 (ttyS1)"),
            controller->GetOption(0));
}

TEST_F(SerialChooserControllerTest, PortSelected) {
  auto port = device::mojom::SerialPortInfo::New();
  port->token = base::UnguessableToken::Create();
  port->display_name = "Test Port";
  port->path = base::FilePath(FILE_PATH_LITERAL("/dev/ttyS0"));
  base::UnguessableToken port_token = port->token;
  port_manager().AddPort(std::move(port));

  base::MockCallback<content::SerialChooser::Callback> callback;
  std::vector<blink::mojom::SerialPortFilterPtr> filters;
  auto controller = std::make_unique<SerialChooserController>(
      main_rfh(), std::move(filters), callback.Get());

  MockChooserControllerView view;
  controller->set_view(&view);

  {
    base::RunLoop run_loop;
    EXPECT_CALL(view, OnOptionsInitialized).WillOnce(Invoke([&] {
      EXPECT_EQ(1u, controller->NumOptions());
      EXPECT_EQ(base::ASCIIToUTF16("Test Port (ttyS0)"),
                controller->GetOption(0));
      run_loop.Quit();
    }));
    run_loop.Run();
  }

  EXPECT_CALL(callback, Run(_))
      .WillOnce(Invoke([&](device::mojom::SerialPortInfoPtr port) {
        EXPECT_EQ(port_token, port->token);

        // Regression test for https://crbug.com/1069057. Ensure that the set of
        // options is still valid after the callback is run.
        EXPECT_EQ(1u, controller->NumOptions());
        EXPECT_EQ(base::ASCIIToUTF16("Test Port (ttyS0)"),
                  controller->GetOption(0));
      }));
  controller->Select({0});
}
