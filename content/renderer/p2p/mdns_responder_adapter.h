// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_P2P_MDNS_RESPONDER_ADAPTER_H_
#define CONTENT_RENDERER_P2P_MDNS_RESPONDER_ADAPTER_H_

#include "services/network/public/mojom/mdns_responder.mojom.h"
#include "third_party/webrtc/rtc_base/mdns_responder_interface.h"

namespace rtc {
class IPAddress;
}  // namespace rtc

namespace content {

// This class is created on the main thread but is used only on the WebRTC
// worker threads. The MdnsResponderAdapter implements the WebRTC mDNS responder
// interface via the MdnsResponder service in Chromium, and is used to register
// and resolve mDNS hostnames to conceal local IP addresses.
class MdnsResponderAdapter : public webrtc::MdnsResponderInterface {
 public:
  // The adapter should be created on the main thread to have access to the
  // connector to the service manager.
  MdnsResponderAdapter();
  ~MdnsResponderAdapter() override;

  // webrtc::MdnsResponderInterface implementation.
  void CreateNameForAddress(const rtc::IPAddress& addr,
                            NameCreatedCallback callback) override;
  void RemoveNameForAddress(const rtc::IPAddress& addr,
                            NameRemovedCallback callback) override;

 private:
  scoped_refptr<network::mojom::ThreadSafeMdnsResponderPtr> thread_safe_client_;

  DISALLOW_COPY_AND_ASSIGN(MdnsResponderAdapter);
};

}  // namespace content

#endif  // CONTENT_RENDERER_P2P_MDNS_RESPONDER_ADAPTER_H_
