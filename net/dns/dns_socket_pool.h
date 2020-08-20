// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DNS_DNS_SOCKET_POOL_H_
#define NET_DNS_DNS_SOCKET_POOL_H_

#include <memory>
#include <vector>

#include "net/base/net_export.h"
#include "net/base/rand_callback.h"

namespace net {

class ClientSocketFactory;
class DatagramClientSocket;
class IPEndPoint;
class NetLog;
struct NetLogSource;
class StreamSocket;

// A DnsSocketPool is an abstraction layer around a ClientSocketFactory that
// allows preallocation, reuse, or other strategies to manage sockets connected
// to DNS servers.
//
// TODO(crbug.com/1116579): Rename since this doesn't do any pooling.
class NET_EXPORT_PRIVATE DnsSocketPool {
 public:
  DnsSocketPool(ClientSocketFactory* factory,
                std::vector<IPEndPoint> nameservers,
                NetLog* net_log);
  ~DnsSocketPool();

  DnsSocketPool(const DnsSocketPool&) = delete;
  DnsSocketPool& operator=(const DnsSocketPool&) = delete;

  // Creates a UDP client socket that is already connected to the nameserver
  // referenced by |server_index|. Returns null on error connecting the socket.
  std::unique_ptr<DatagramClientSocket> CreateConnectedUdpSocket(
      size_t server_index);

  // Creates a StreamSocket for TCP to the nameserver referenced by
  // |server_index|. Does not connect the seocket.
  std::unique_ptr<StreamSocket> CreateTcpSocket(size_t server_index,
                                                const NetLogSource& source);

 private:
  ClientSocketFactory* const socket_factory_;
  NetLog* const net_log_;
  const std::vector<IPEndPoint> nameservers_;
};

} // namespace net

#endif // NET_DNS_DNS_SOCKET_POOL_H_
