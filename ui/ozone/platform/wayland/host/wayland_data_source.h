// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_DATA_SOURCE_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_DATA_SOURCE_H_

#include <wayland-client.h>

#include <cstdint>
#include <map>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/host/wayland_data_source_base.h"
#include "ui/ozone/public/platform_clipboard.h"

namespace ui {

class OSExchangeData;
class WaylandConnection;

// WaylandDataSource represents the source side of a WaylandDataOffer. It is
// created by the source client in a data transfer and provides a way to
// describe the offered data (wl_data_source_offer) and a way to respond to
// requests to transfer the data (OnSend listener).
class WaylandDataSource : public WaylandDataSourceBase {
 public:
  using DragDataMap = std::map<std::string, std::string>;

  // DragDelegate is responsible for handling the wl_data_source events during
  // drag and drop sessions.
  class DragDelegate {
   public:
    virtual void OnDragSourceFinish(bool completed) = 0;
    virtual void OnDragSourceSend(const std::string& mime_type,
                                  std::string* contents) = 0;

   protected:
    virtual ~DragDelegate() = default;
  };

  // Takes ownership of data_source.
  explicit WaylandDataSource(wl_data_source* data_source,
                             WaylandConnection* connection);
  ~WaylandDataSource() override;

  void set_connection(WaylandConnection* connection) {
    DCHECK(connection);
    connection_ = connection;
  }

  void WriteToClipboard(const PlatformClipboard::DataMap& data_map) override;
  void Offer(const ui::OSExchangeData& data, DragDelegate* drag_delegate);
  void SetAction(int operation);

  wl_data_source* data_source() const { return data_source_.get(); }
  uint32_t dnd_action() const { return dnd_action_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(WaylandDataDragControllerTest, StartDrag);
  FRIEND_TEST_ALL_PREFIXES(WaylandDataDragControllerTest,
                           StartDragWithWrongMimeType);

  static void OnTarget(void* data,
                       wl_data_source* source,
                       const char* mime_type);
  static void OnSend(void* data,
                     wl_data_source* source,
                     const char* mime_type,
                     int32_t fd);
  static void OnCancel(void* data, wl_data_source* source);
  static void OnDnDDropPerformed(void* data, wl_data_source* source);
  static void OnDnDFinished(void* data, wl_data_source* source);
  static void OnAction(void* data, wl_data_source* source, uint32_t dnd_action);

  void GetDragData(const std::string& mime_type, std::string* contents);

  wl::Object<wl_data_source> data_source_;
  WaylandConnection* connection_ = nullptr;

  // Set when this used in DND sessions initiated from Chromium.
  DragDelegate* drag_delegate_ = nullptr;

  DragDataMap drag_data_map_;

  // Action selected by the compositor
  uint32_t dnd_action_;

  DISALLOW_COPY_AND_ASSIGN(WaylandDataSource);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_DATA_SOURCE_H_
