// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <wayland-server.h>

#include <memory>

#include "base/bind.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/events/base_event_utils.h"
#include "ui/ozone/platform/wayland/test/constants.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/test_data_device.h"
#include "ui/ozone/platform/wayland/test/test_data_device_manager.h"
#include "ui/ozone/platform/wayland/test/test_data_offer.h"
#include "ui/ozone/platform/wayland/test/test_data_source.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"
#include "ui/ozone/public/platform_clipboard.h"

namespace ui {

// This class mocks how a real clipboard/ozone client would
// hook to PlatformClipboard, with one difference: real clients
// have no access to the WaylandConnection instance like this
// MockClipboardClient impl does. Instead, clients and ozone gets
// plumbbed up by calling the appropriated Ozone API,
// OzonePlatform::GetPlatformClipboard.
class MockClipboardClient {
 public:
  explicit MockClipboardClient(WaylandConnection* connection) {
    DCHECK(connection);
    // See comment above for reasoning to access the WaylandConnection
    // directly from here.
    delegate_ = connection->clipboard();

    DCHECK(delegate_);
  }
  ~MockClipboardClient() = default;

  // Fill the clipboard backing store with sample data.
  void SetData(const std::string& utf8_text,
               const std::string& mime_type,
               PlatformClipboard::OfferDataClosure callback) {
    // This mimics how Mus' ClipboardImpl writes data to the DataMap.
    std::vector<char> object_map(utf8_text.begin(), utf8_text.end());
    char* object_data = &object_map.front();
    data_types_[mime_type] =
        std::vector<uint8_t>(object_data, object_data + object_map.size());

    delegate_->OfferClipboardData(ClipboardBuffer::kCopyPaste, data_types_,
                                  std::move(callback));
  }

  void ReadData(const std::string& mime_type,
                PlatformClipboard::RequestDataClosure callback) {
    delegate_->RequestClipboardData(ClipboardBuffer::kCopyPaste, mime_type,
                                    &data_types_, std::move(callback));
  }

  bool IsSelectionOwner() {
    return delegate_->IsSelectionOwner(ClipboardBuffer::kCopyPaste);
  }

 private:
  PlatformClipboard* delegate_ = nullptr;
  PlatformClipboard::DataMap data_types_;

  DISALLOW_COPY_AND_ASSIGN(MockClipboardClient);
};

class WaylandDataDeviceManagerTest : public WaylandTest {
 public:
  WaylandDataDeviceManagerTest() {}

  void SetUp() override {
    WaylandTest::SetUp();

    Sync();

    data_device_manager_ = server_.data_device_manager();
    DCHECK(data_device_manager_);

    clipboard_client_ =
        std::make_unique<MockClipboardClient>(connection_.get());
  }

 protected:
  wl::TestDataDeviceManager* data_device_manager_;
  std::unique_ptr<MockClipboardClient> clipboard_client_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WaylandDataDeviceManagerTest);
};

TEST_P(WaylandDataDeviceManagerTest, WriteToClipboard) {
  // The client writes data to the clipboard ...
  clipboard_client_->SetData(wl::kSampleClipboardText, wl::kTextMimeTypeUtf8,
                             base::BindOnce([]() {}));
  Sync();

  // ... and the server reads it.
  base::RunLoop run_loop;
  auto callback = base::BindOnce(
      [](base::RunLoop* loop, std::vector<uint8_t>&& data) {
        std::string string_data(data.begin(), data.end());
        EXPECT_EQ(wl::kSampleClipboardText, string_data);
        loop->Quit();
      },
      &run_loop);
  data_device_manager_->data_source()->ReadData(wl::kTextMimeTypeUtf8,
                                                std::move(callback));
  run_loop.Run();
}

TEST_P(WaylandDataDeviceManagerTest, ReadFromClipboard) {
  // TODO(nickdiego): implement this in terms of an actual wl_surface that
  // gets focused and compositor sends data_device data to it.
  auto* data_offer = data_device_manager_->data_device()->OnDataOffer();
  data_offer->OnOffer(wl::kTextMimeTypeUtf8);
  data_device_manager_->data_device()->OnSelection(data_offer);
  Sync();

  // The client requests to reading clipboard data from the server.
  // The Server writes in some sample data, and we check it matches
  // expectation.
  auto callback =
      base::BindOnce([](const base::Optional<std::vector<uint8_t>>& data) {
        std::string string_data = std::string(data->begin(), data->end());
        EXPECT_EQ(wl::kSampleClipboardText, string_data);
      });
  clipboard_client_->ReadData(wl::kTextMimeTypeUtf8, std::move(callback));
  Sync();
}

TEST_P(WaylandDataDeviceManagerTest, ReadFromClipboardWithoutOffer) {
  // When no data offer is advertised and client requests clipboard data
  // from the server, the response callback should be gracefully called with
  // an empty string.
  auto callback =
      base::BindOnce([](const base::Optional<std::vector<uint8_t>>& data) {
        std::string string_data = std::string(data->begin(), data->end());
        EXPECT_EQ("", string_data);
      });
  clipboard_client_->ReadData(wl::kTextMimeTypeUtf8, std::move(callback));
}

TEST_P(WaylandDataDeviceManagerTest, IsSelectionOwner) {
  auto callback = base::BindOnce([]() {});
  clipboard_client_->SetData(wl::kSampleClipboardText, wl::kTextMimeTypeUtf8,
                             std::move(callback));
  Sync();
  ASSERT_TRUE(clipboard_client_->IsSelectionOwner());

  // The compositor sends OnCancelled whenever another application
  // on the system sets a new selection. It means we are not the application
  // that owns the current selection data.
  data_device_manager_->data_source()->OnCancelled();
  Sync();

  ASSERT_FALSE(clipboard_client_->IsSelectionOwner());
}

TEST_P(WaylandDataDeviceManagerTest, StartDrag) {
  bool restored_focus = window_->has_pointer_focus();
  window_->set_pointer_focus(true);

  // The client starts dragging.
  OSExchangeData os_exchange_data;
  int operation = DragDropTypes::DRAG_COPY | DragDropTypes::DRAG_MOVE;
  connection_->StartDrag(os_exchange_data, operation);

  WaylandDataSource::DragDataMap data;
  data[wl::kTextMimeTypeUtf8] = wl::kSampleTextForDragAndDrop;
  connection_->drag_data_source()->SetDragData(data);
  Sync();

  // The server reads the data and the callback gets it.
  base::RunLoop run_loop;
  auto callback = base::BindOnce(
      [](base::RunLoop* loop, std::vector<uint8_t>&& data) {
        std::string result(data.begin(), data.end());
        EXPECT_EQ(wl::kSampleTextForDragAndDrop, result);
        loop->Quit();
      },
      &run_loop);
  data_device_manager_->data_source()->ReadData(wl::kTextMimeTypeUtf8,
                                                std::move(callback));
  run_loop.Run();
  window_->set_pointer_focus(restored_focus);
}

TEST_P(WaylandDataDeviceManagerTest, StartDragWithWrongMimeType) {
  bool restored_focus = window_->has_pointer_focus();
  window_->set_pointer_focus(true);

  // The client starts dragging offering data with wl::kTextMimeTypeUtf8
  // mime type.
  OSExchangeData os_exchange_data;
  int operation = DragDropTypes::DRAG_COPY | DragDropTypes::DRAG_MOVE;
  connection_->StartDrag(os_exchange_data, operation);

  WaylandDataSource::DragDataMap data;
  data[wl::kTextMimeTypeUtf8] = wl::kSampleTextForDragAndDrop;
  connection_->drag_data_source()->SetDragData(data);
  Sync();

  // The server should get an empty data buffer in ReadData callback
  // when trying to read it.
  base::RunLoop run_loop;
  auto callback = base::BindOnce(
      [](base::RunLoop* loop, std::vector<uint8_t>&& data) {
        std::string result(data.begin(), data.end());
        EXPECT_EQ("", result);
        loop->Quit();
      },
      &run_loop);
  data_device_manager_->data_source()->ReadData(wl::kTextMimeTypeText,
                                                std::move(callback));
  run_loop.Run();
  window_->set_pointer_focus(restored_focus);
}

TEST_P(WaylandDataDeviceManagerTest, ReceiveDrag) {
  auto* data_offer = data_device_manager_->data_device()->OnDataOffer();
  data_offer->OnOffer(wl::kTextMimeTypeText);

  gfx::Point entered_point(10, 10);
  // The server sends an enter event.
  data_device_manager_->data_device()->OnEnter(
      1002, surface_->resource(), wl_fixed_from_int(entered_point.x()),
      wl_fixed_from_int(entered_point.y()), data_offer);

  int64_t time =
      (ui::EventTimeForNow() - base::TimeTicks()).InMilliseconds() & UINT32_MAX;
  gfx::Point motion_point(11, 11);

  // The server sends an motion event.
  data_device_manager_->data_device()->OnMotion(
      time, wl_fixed_from_int(motion_point.x()),
      wl_fixed_from_int(motion_point.y()));

  Sync();

  auto callback = base::BindOnce([](const std::string& contents) {
    EXPECT_EQ(wl::kSampleTextForDragAndDrop, contents);
  });

  // The client requests the data and gets callback with it.
  connection_->RequestDragData(wl::kTextMimeTypeText, std::move(callback));
  Sync();

  data_device_manager_->data_device()->OnLeave();
}

INSTANTIATE_TEST_SUITE_P(XdgVersionV5Test,
                         WaylandDataDeviceManagerTest,
                         ::testing::Values(kXdgShellV5));

INSTANTIATE_TEST_SUITE_P(XdgVersionV6Test,
                         WaylandDataDeviceManagerTest,
                         ::testing::Values(kXdgShellV6));

}  // namespace ui
