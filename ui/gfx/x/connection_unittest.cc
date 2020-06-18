// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/xproto.h"

#undef Bool

#include <xcb/xcb.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace x11 {

namespace {

Window CreateWindow(Connection* connection) {
  Window window = connection->GenerateId<Window>();
  auto create_window_future = connection->CreateWindow({
      .depth = connection->default_root_depth().depth,
      .wid = window,
      .parent = connection->default_screen().root,
      .width = 1,
      .height = 1,
      .override_redirect = Bool32(true),
  });
  auto create_window_response = create_window_future.Sync();
  EXPECT_FALSE(create_window_response.error);
  return window;
}

}  // namespace

// Connection setup and teardown.
TEST(X11ConnectionTest, Basic) {
  Connection connection;
  ASSERT_TRUE(connection.XcbConnection());
  EXPECT_FALSE(xcb_connection_has_error(connection.XcbConnection()));
}

TEST(X11ConnectionTest, Request) {
  Connection connection;
  ASSERT_TRUE(connection.XcbConnection());
  EXPECT_FALSE(xcb_connection_has_error(connection.XcbConnection()));

  Window window = CreateWindow(&connection);

  auto attributes = connection.GetWindowAttributes({window}).Sync();
  ASSERT_TRUE(attributes);
  EXPECT_EQ(attributes->map_state, MapState::Unmapped);
  EXPECT_TRUE(attributes->override_redirect);

  auto geometry = connection.GetGeometry({window}).Sync();
  ASSERT_TRUE(geometry);
  EXPECT_EQ(geometry->x, 0);
  EXPECT_EQ(geometry->y, 0);
  EXPECT_EQ(geometry->width, 1u);
  EXPECT_EQ(geometry->height, 1u);
}

TEST(X11ConnectionTest, Event) {
  Connection connection;
  ASSERT_TRUE(connection.XcbConnection());
  EXPECT_FALSE(xcb_connection_has_error(connection.XcbConnection()));

  Window window = CreateWindow(&connection);

  auto cwa_future = connection.ChangeWindowAttributes({
      .window = window,
      .event_mask = EventMask::PropertyChange,
  });
  EXPECT_FALSE(cwa_future.Sync().error);

  auto prop_future = connection.ChangeProperty({
      .window = static_cast<x11::Window>(window),
      .property = x11::Atom::WM_NAME,
      .type = x11::Atom::STRING,
      .format = CHAR_BIT,
      .data_len = 1,
      .data = std::vector<uint8_t>{0},
  });
  EXPECT_FALSE(prop_future.Sync().error);

  connection.ReadResponses();
  ASSERT_EQ(connection.events().size(), 1u);
  XEvent& event = connection.events().front().xlib_event();
  auto property_notify_opcode = PropertyNotifyEvent::opcode;
  EXPECT_EQ(event.type, property_notify_opcode);
  EXPECT_EQ(event.xproperty.atom, static_cast<uint32_t>(x11::Atom::WM_NAME));
  EXPECT_EQ(event.xproperty.state, static_cast<int>(Property::NewValue));
}

TEST(X11ConnectionTest, Error) {
  Connection connection;
  ASSERT_TRUE(connection.XcbConnection());
  EXPECT_FALSE(xcb_connection_has_error(connection.XcbConnection()));

  Window invalid_window = connection.GenerateId<Window>();

  auto geometry = connection.GetGeometry({invalid_window}).Sync();
  ASSERT_FALSE(geometry);
  xcb_generic_error_t* error = geometry.error.get();
  EXPECT_EQ(error->error_code, XCB_DRAWABLE);
  EXPECT_EQ(error->resource_id, static_cast<uint32_t>(invalid_window));
}

}  // namespace x11
