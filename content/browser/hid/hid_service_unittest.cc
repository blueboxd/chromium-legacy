// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/test/bind_test_util.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/hid_delegate.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "services/device/public/cpp/hid/fake_hid_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/hid/hid.mojom.h"

namespace content {

namespace {

const char kTestUrl[] = "https://www.google.com";
const char kTestGuid[] = "test-guid";

class MockHidDelegate : public HidDelegate {
 public:
  MockHidDelegate() = default;
  ~MockHidDelegate() override = default;

  std::unique_ptr<HidChooser> RunChooser(
      RenderFrameHost* frame,
      std::vector<blink::mojom::HidDeviceFilterPtr> filters,
      HidChooser::Callback callback) override {
    std::move(callback).Run(RunChooserInternal());
    return nullptr;
  }

  MOCK_METHOD0(RunChooserInternal, device::mojom::HidDeviceInfoPtr());
  MOCK_METHOD2(CanRequestDevicePermission,
               bool(content::WebContents* web_contents,
                    const url::Origin& requesting_origin));
  MOCK_METHOD3(HasDevicePermission,
               bool(content::WebContents* web_contents,
                    const url::Origin& requesting_origin,
                    const device::mojom::HidDeviceInfo& device));
  MOCK_METHOD1(GetHidManager,
               device::mojom::HidManager*(content::WebContents* web_contents));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockHidDelegate);
};

class HidTestContentBrowserClient : public ContentBrowserClient {
 public:
  HidTestContentBrowserClient() = default;
  ~HidTestContentBrowserClient() override = default;

  MockHidDelegate& delegate() { return delegate_; }

  // ContentBrowserClient:
  HidDelegate* GetHidDelegate() override { return &delegate_; }

 private:
  MockHidDelegate delegate_;

  DISALLOW_COPY_AND_ASSIGN(HidTestContentBrowserClient);
};

class FakeHidConnectionClient : public device::mojom::HidConnectionClient {
 public:
  FakeHidConnectionClient() : binding_(this) {}

  void Bind(device::mojom::HidConnectionClientRequest request) {
    binding_.Bind(std::move(request));
  }

  // mojom::HidConnectionClient:
  void OnInputReport(uint8_t report_id,
                     const std::vector<uint8_t>& buffer) override {}

 private:
  mojo::Binding<device::mojom::HidConnectionClient> binding_;

  DISALLOW_COPY_AND_ASSIGN(FakeHidConnectionClient);
};

class HidServiceTest : public RenderViewHostImplTestHarness {
 public:
  HidServiceTest() {
    ON_CALL(hid_delegate(), GetHidManager)
        .WillByDefault(testing::Return(&hid_manager_));
  }

  ~HidServiceTest() override = default;

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    original_client_ = SetBrowserClientForTesting(&test_client_);
    RenderViewHostTestHarness::SetUp();
  }

  void TearDown() override {
    RenderViewHostTestHarness::TearDown();
    if (original_client_)
      SetBrowserClientForTesting(original_client_);
  }

  MockHidDelegate& hid_delegate() { return test_client_.delegate(); }
  device::FakeHidManager* hid_manager() { return &hid_manager_; }
  FakeHidConnectionClient* connection_client() { return &connection_client_; }

 private:
  HidTestContentBrowserClient test_client_;
  ContentBrowserClient* original_client_ = nullptr;
  device::FakeHidManager hid_manager_;
  FakeHidConnectionClient connection_client_;

  DISALLOW_COPY_AND_ASSIGN(HidServiceTest);
};

}  // namespace

TEST_F(HidServiceTest, GetDevicesWithPermission) {
  NavigateAndCommit(GURL(kTestUrl));

  blink::mojom::HidServicePtr service;
  contents()->GetMainFrame()->BinderRegistryForTesting().BindInterface(
      blink::mojom::HidService::Name_,
      mojo::MakeRequest(&service).PassMessagePipe());

  auto device_info = device::mojom::HidDeviceInfo::New();
  device_info->guid = kTestGuid;
  hid_manager()->AddDevice(std::move(device_info));

  EXPECT_CALL(hid_delegate(), HasDevicePermission)
      .WillOnce(testing::Return(true));

  base::RunLoop run_loop;
  std::vector<device::mojom::HidDeviceInfoPtr> devices;
  service->GetDevices(base::BindLambdaForTesting(
      [&run_loop, &devices](std::vector<device::mojom::HidDeviceInfoPtr> d) {
        devices = std::move(d);
        run_loop.Quit();
      }));
  run_loop.Run();
  EXPECT_EQ(1u, devices.size());
}

TEST_F(HidServiceTest, GetDevicesWithoutPermission) {
  NavigateAndCommit(GURL(kTestUrl));

  blink::mojom::HidServicePtr service;
  contents()->GetMainFrame()->BinderRegistryForTesting().BindInterface(
      blink::mojom::HidService::Name_,
      mojo::MakeRequest(&service).PassMessagePipe());

  auto device_info = device::mojom::HidDeviceInfo::New();
  device_info->guid = kTestGuid;
  hid_manager()->AddDevice(std::move(device_info));

  EXPECT_CALL(hid_delegate(), HasDevicePermission)
      .WillOnce(testing::Return(false));

  base::RunLoop run_loop;
  std::vector<device::mojom::HidDeviceInfoPtr> devices;
  service->GetDevices(base::BindLambdaForTesting(
      [&run_loop, &devices](std::vector<device::mojom::HidDeviceInfoPtr> d) {
        devices = std::move(d);
        run_loop.Quit();
      }));
  run_loop.Run();
  EXPECT_EQ(0u, devices.size());
}

TEST_F(HidServiceTest, RequestDevice) {
  NavigateAndCommit(GURL(kTestUrl));

  blink::mojom::HidServicePtr service;
  contents()->GetMainFrame()->BinderRegistryForTesting().BindInterface(
      blink::mojom::HidService::Name_,
      mojo::MakeRequest(&service).PassMessagePipe());

  auto device_info = device::mojom::HidDeviceInfo::New();
  device_info->guid = kTestGuid;
  hid_manager()->AddDevice(device_info.Clone());

  EXPECT_CALL(hid_delegate(), CanRequestDevicePermission)
      .WillOnce(testing::Return(true));
  EXPECT_CALL(hid_delegate(), RunChooserInternal)
      .WillOnce(testing::Return(testing::ByMove(std::move(device_info))));

  base::RunLoop run_loop;
  device::mojom::HidDeviceInfoPtr chosen_device;
  service->RequestDevice(
      std::vector<blink::mojom::HidDeviceFilterPtr>(),
      base::BindLambdaForTesting(
          [&run_loop, &chosen_device](device::mojom::HidDeviceInfoPtr d) {
            chosen_device = std::move(d);
            run_loop.Quit();
          }));
  run_loop.Run();
  EXPECT_TRUE(chosen_device);
}

TEST_F(HidServiceTest, OpenAndCloseHidConnection) {
  NavigateAndCommit(GURL(kTestUrl));

  blink::mojom::HidServicePtr service;
  contents()->GetMainFrame()->BinderRegistryForTesting().BindInterface(
      blink::mojom::HidService::Name_,
      mojo::MakeRequest(&service).PassMessagePipe());

  auto device_info = device::mojom::HidDeviceInfo::New();
  device_info->guid = kTestGuid;
  hid_manager()->AddDevice(std::move(device_info));

  device::mojom::HidConnectionClientPtr hid_connection_client;
  connection_client()->Bind(mojo::MakeRequest(&hid_connection_client));

  base::RunLoop run_loop;
  device::mojom::HidConnectionPtr connection;
  service->Connect(
      kTestGuid, std::move(hid_connection_client),
      base::BindLambdaForTesting(
          [&run_loop, &connection](device::mojom::HidConnectionPtr c) {
            connection = std::move(c);
            run_loop.Quit();
          }));
  run_loop.Run();
  EXPECT_TRUE(connection);

  connection.reset();
}

}  // namespace content
