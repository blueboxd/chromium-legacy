// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/providers/openscreen/platform/chrome_tls_client_connection.h"

#include <algorithm>
#include <utility>

#include "third_party/openscreen/src/platform/base/error.h"

namespace media_router {

VectorIOBuffer::VectorIOBuffer() : buffer_(kMaxTlsPacketBufferSize) {
  IOBuffer::data_ = reinterpret_cast<char*>(buffer_.data());
}

VectorIOBuffer::~VectorIOBuffer() {
  IOBuffer::data_ = nullptr;
}

std::vector<uint8_t> VectorIOBuffer::TakeResult() {
  std::vector<uint8_t> result(kMaxTlsPacketBufferSize);
  result.swap(buffer_);
  IOBuffer::data_ = reinterpret_cast<char*>(buffer_.data());
  return result;
}

ChromeTlsClientConnection::ChromeTlsClientConnection(
    openscreen::platform::TaskRunner* task_runner,
    openscreen::IPEndpoint local_address,
    openscreen::IPEndpoint remote_address,
    std::unique_ptr<NetworkDataPump> data_pump,
    mojo::Remote<network::mojom::TCPConnectedSocket> tcp_socket,
    mojo::Remote<network::mojom::TLSClientSocket> tls_socket)
    : task_runner_(task_runner),
      local_address_(std::move(local_address)),
      remote_address_(std::move(remote_address)),
      data_pump_(std::move(data_pump)),
      read_buffer_(base::MakeRefCounted<VectorIOBuffer>()),
      write_buffer_(base::MakeRefCounted<net::GrowableIOBuffer>()),
      tcp_socket_(std::move(tcp_socket)),
      tls_socket_(std::move(tls_socket)) {
  DCHECK(task_runner_);
  RequestRead();
}

ChromeTlsClientConnection::~ChromeTlsClientConnection() = default;

void ChromeTlsClientConnection::SetClient(Client* client) {
  client_ = client;
}

void ChromeTlsClientConnection::Write(const void* data, size_t len) {
  // TODO(miu): move all implementation out of TlsConnection interface.
  client_->OnWriteBlocked(this);

  // TODO(jophba): add support for multiple pending writes at a time.
  if (data_pump_->HasPendingWrite() && !write_blocked_) {
    write_blocked_ = true;
    return;
  }

  const bool buffer_much_too_large =
      static_cast<double>(write_buffer_->capacity()) > (len * 1.5f);
  const bool buffer_too_small =
      write_buffer_->capacity() < static_cast<int>(len);
  if (buffer_much_too_large || buffer_too_small) {
    write_buffer_->SetCapacity(len);
  }

  const uint8_t* safe_data = reinterpret_cast<const uint8_t*>(data);
  std::copy(safe_data, safe_data + len, write_buffer_->data());
  data_pump_->Write(write_buffer_.get(), len,
                    base::BindOnce(&ChromeTlsClientConnection::OnWriteComplete,
                                   weak_factory_.GetWeakPtr()));
}

openscreen::IPEndpoint ChromeTlsClientConnection::GetLocalEndpoint() const {
  return local_address_;
}

openscreen::IPEndpoint ChromeTlsClientConnection::GetRemoteEndpoint() const {
  return remote_address_;
}

void ChromeTlsClientConnection::OnReadComplete(int32_t size_or_error) {
  if (size_or_error <= net::ERR_FAILED) {
    client_->OnError(
        this, openscreen::Error(openscreen::Error::Code::kSocketReadFailure,
                                net::ErrorToString(size_or_error)));
    return;
  }

  // Open Screen expects a moved in std::vector of data, so we swap our internal
  // vector when it is done.
  client_->OnRead(this, read_buffer_->TakeResult());
  RequestRead();
}

void ChromeTlsClientConnection::OnWriteComplete(int32_t size_or_error) {
  client_->OnWriteUnblocked(this);
  if (write_blocked_) {
    write_blocked_ = false;
  }

  if (size_or_error <= net::ERR_FAILED) {
    client_->OnError(
        this, openscreen::Error(openscreen::Error::Code::kSocketSendFailure,
                                net::ErrorToString(size_or_error)));
  }
}

void ChromeTlsClientConnection::RequestRead() {
  data_pump_->Read(read_buffer_, read_buffer_->size(),
                   base::BindOnce(&ChromeTlsClientConnection::OnReadComplete,
                                  weak_factory_.GetWeakPtr()));
}

}  // namespace media_router
