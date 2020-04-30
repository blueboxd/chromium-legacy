// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_X_XPROTO_INTERNAL_H_
#define UI_GFX_X_XPROTO_INTERNAL_H_

#ifndef IS_X11_IMPL
#error "This file should only be included by generated xprotos"
#endif

#include <X11/Xlib-xcb.h>
#include <stdint.h>
#include <string.h>
#include <xcb/xcb.h>
#include <xcb/xcbext.h>

#include <bitset>
#include <limits>
#include <type_traits>

#include "base/component_export.h"
#include "base/optional.h"
#include "ui/gfx/x/xproto_types.h"

namespace x11 {

template <class Reply>
class Future;

using WriteBuffer = std::vector<uint8_t>;

struct ReadBuffer {
  const uint8_t* data = nullptr;
  size_t offset = 0;
};

template <typename T>
void Write(const T* t, WriteBuffer* buf) {
  static_assert(std::is_trivially_copyable<T>::value, "");
  // On the wire, X11 types are always aligned to their size.  This is a sanity
  // check to ensure padding etc are working properly.
  DCHECK_EQ(buf->size() % sizeof(*t), 0UL);
  const uint8_t* start = reinterpret_cast<const uint8_t*>(t);
  std::copy(start, start + sizeof(*t), std::back_inserter(*buf));
}

template <typename T>
void Read(T* t, ReadBuffer* buf) {
  static_assert(std::is_trivially_copyable<T>::value, "");
  // On the wire, X11 types are always aligned to their size.  This is a sanity
  // check to ensure padding etc are working properly.
  DCHECK_EQ(buf->offset % sizeof(*t), 0UL);
  memcpy(t, buf->data + buf->offset, sizeof(*t));
  buf->offset += sizeof(*t);
}

inline void Pad(WriteBuffer* buf, size_t amount) {
  buf->resize(buf->size() + amount, '\0');
}

inline void Pad(ReadBuffer* buf, size_t amount) {
  buf->offset += amount;
}

inline void Align(WriteBuffer* buf, size_t align) {
  Pad(buf, (align - (buf->size() % align)) % align);
}

inline void Align(ReadBuffer* buf, size_t align) {
  Pad(buf, (align - (buf->offset % align)) % align);
}

template <typename Reply>
Future<Reply> SendRequest(XDisplay* display, WriteBuffer* buf) {
  // Clang crashes when the value of |is_void| is inlined below,
  // so keep this variable outside of |xpr|.
  constexpr bool is_void = std::is_void<Reply>::value;
  xcb_protocol_request_t xpr{
      .count = 1,
      .ext = nullptr,
      .isvoid = is_void,
  };

  struct RequestHeader {
    uint8_t major_opcode;
    uint8_t minor_opcode;
    uint16_t length;
  };

  auto* header = reinterpret_cast<RequestHeader*>(buf->data());
  // Requests are always a multiple of 4 bytes on the wire.  Because of this,
  // the length field represents the size in chunks of 4 bytes.
  DCHECK_EQ(buf->size() % 4, 0UL);
  DCHECK_LE(buf->size() / 4, std::numeric_limits<uint16_t>::max());
  header->length = buf->size() / 4;

  struct iovec io[3];
  io[2].iov_base = buf->data();
  io[2].iov_len = buf->size();
  auto flags = XCB_REQUEST_CHECKED | XCB_REQUEST_RAW;

  xcb_connection_t* conn = XGetXCBConnection(display);
  auto sequence = xcb_send_request(conn, flags, &io[2], &xpr);
  if (xcb_connection_has_error(conn))
    return {nullptr, base::nullopt};
  return {display, sequence};
}

// Helper function for xcbproto popcount.  Given an integral type, returns the
// number of 1 bits present.
template <typename T>
size_t PopCount(T t) {
  return std::bitset<sizeof(T) * 8>(t).count();
}

// Helper function for xcbproto sumof.  Given a function |f| and a container
// |t|, maps the elements uisng |f| and reduces by summing the results.
template <typename F, typename T>
auto SumOf(F&& f, T& t) {
  decltype(f(t[0])) sum = 0;
  for (auto& v : t)
    sum += f(v);
  return sum;
}

// Helper function for xcbproto case.  Checks for equality between |t| and |s|.
template <typename T, typename S>
bool CaseEq(T t, S s) {
  return t == static_cast<decltype(t)>(s);
}

// Helper function for xcbproto bitcase.  Checks if the bitmasks |t| and |s|
// have any intersection.
template <typename T, typename S>
bool CaseAnd(T t, S s) {
  return t & static_cast<decltype(t)>(s);
}

}  // namespace x11

#endif  //  UI_GFX_X_XPROTO_INTERNAL_H_
