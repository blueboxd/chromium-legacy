// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/x/xproto_internal.h"

// XCB used to send requests with FDs by sending each FD individually with
// xcb_send_fd(), then the request with xcb_send_request().  However, there's a
// race condition -- FDs can get mixed up if multiple threads are sending them
// at the same time.  xcb_send_request_with_fds() was introduced to atomically
// handle this case, however it's only available on newer systems.  In
// particular it's not available on Ubuntu Xenial (which has LTS until April
// 2024).  We want to use the bug-free version when it's available, and fallback
// to the buggy version otherwise.

// Declare the function in case this is a packager build on an older distro with
// use_sysroot=false.
unsigned int xcb_send_request_with_fds(xcb_connection_t* c,
                                       int flags,
                                       struct iovec* vector,
                                       const xcb_protocol_request_t* request,
                                       unsigned int num_fds,
                                       int* fds);

// Add the weak attribute to the symbol.  This prevents the dynamic linker from
// erroring out.  Instead, if the function is not found, then it's address is
// nullptr, so we can do a runtime check to test availability.
extern "C" __attribute__((weak)) decltype(
    xcb_send_request_with_fds) xcb_send_request_with_fds;

namespace x11 {

MallocedRefCountedMemory::MallocedRefCountedMemory(void* data)
    : data_(reinterpret_cast<uint8_t*>(data)) {}

const uint8_t* MallocedRefCountedMemory::front() const {
  return data_;
}

size_t MallocedRefCountedMemory::size() const {
  // There's no easy way to tell how large malloc'ed data is.
  NOTREACHED();
  return 0;
}

MallocedRefCountedMemory::~MallocedRefCountedMemory() {
  free(data_);
}

OffsetRefCountedMemory::OffsetRefCountedMemory(
    scoped_refptr<base::RefCountedMemory> memory,
    size_t offset,
    size_t size)
    : memory_(memory), offset_(offset), size_(size) {}

const uint8_t* OffsetRefCountedMemory::front() const {
  return memory_->front() + offset_;
}

size_t OffsetRefCountedMemory::size() const {
  return size_;
}

OffsetRefCountedMemory::~OffsetRefCountedMemory() = default;

UnretainedRefCountedMemory::UnretainedRefCountedMemory(const void* data)
    : data_(reinterpret_cast<const uint8_t*>(data)) {}

const uint8_t* UnretainedRefCountedMemory::front() const {
  return data_;
}

size_t UnretainedRefCountedMemory::size() const {
  // There's no easy way to tell how large malloc'ed data is.
  NOTREACHED();
  return 0;
}

UnretainedRefCountedMemory::~UnretainedRefCountedMemory() = default;

base::Optional<unsigned int> SendRequestImpl(x11::Connection* connection,
                                             WriteBuffer* buf,
                                             bool is_void,
                                             bool reply_has_fds) {
  xcb_protocol_request_t xpr{
      .ext = nullptr,
      .isvoid = is_void,
  };

  struct RequestHeader {
    uint8_t major_opcode;
    uint8_t minor_opcode;
    uint16_t length;
  };

  struct ExtendedRequestHeader {
    RequestHeader header;
    uint32_t long_length;
  };
  static_assert(sizeof(ExtendedRequestHeader) == 8, "");

  auto& first_buffer = buf->GetBuffers()[0];
  DCHECK_GE(first_buffer->size(), sizeof(RequestHeader));
  auto* old_header = reinterpret_cast<RequestHeader*>(
      const_cast<uint8_t*>(first_buffer->data()));
  ExtendedRequestHeader new_header{*old_header, 0};

  // Requests are always a multiple of 4 bytes on the wire.  Because of this,
  // the length field represents the size in chunks of 4 bytes.
  DCHECK_EQ(buf->offset() % 4, 0UL);
  size_t size32 = buf->offset() / 4;

  // XCB requires 2 iovecs for its own internal usage.
  std::vector<struct iovec> io{{nullptr, 0}, {nullptr, 0}};
  if (size32 < connection->setup().maximum_request_length) {
    // Regular request
    old_header->length = size32;
  } else if (size32 < connection->extended_max_request_length()) {
    // BigRequests extension request
    DCHECK_EQ(new_header.header.length, 0U);
    new_header.long_length = size32 + 1;

    io.push_back({&new_header, sizeof(ExtendedRequestHeader)});
    first_buffer = base::MakeRefCounted<OffsetRefCountedMemory>(
        first_buffer, sizeof(RequestHeader),
        first_buffer->size() - sizeof(RequestHeader));
  } else {
    LOG(ERROR) << "Cannot send request of length " << buf->offset();
    return base::nullopt;
  }

  for (auto& buffer : buf->GetBuffers())
    io.push_back({const_cast<uint8_t*>(buffer->data()), buffer->size()});
  xpr.count = io.size() - 2;

  xcb_connection_t* conn = connection->XcbConnection();
  auto flags = XCB_REQUEST_CHECKED | XCB_REQUEST_RAW;
  if (reply_has_fds)
    flags |= XCB_REQUEST_REPLY_FDS;
  base::Optional<unsigned int> sequence;
  if (xcb_send_request_with_fds) {
    // Atomically send the request with its FDs if we can.
    sequence = xcb_send_request_with_fds(conn, flags, &io[2], &xpr,
                                         buf->fds().size(), buf->fds().data());
  } else {
    // Otherwise manually lock and send the fds, then the request.
    XLockDisplay(connection->display());
    for (int fd : buf->fds())
      xcb_send_fd(conn, fd);
    sequence = xcb_send_request(conn, flags, &io[2], &xpr);
    XUnlockDisplay(connection->display());
  }
  if (xcb_connection_has_error(conn))
    return base::nullopt;
  return sequence;
}

}  // namespace x11
