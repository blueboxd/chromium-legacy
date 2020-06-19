// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_PEER_CONNECTION_CONTROLS_H_
#define REMOTING_PROTOCOL_PEER_CONNECTION_CONTROLS_H_

#include "base/optional.h"

namespace remoting {
namespace protocol {

// Interface for changing peer connection parameters after the connection is
// established.
class PeerConnectionControls {
 public:
  virtual ~PeerConnectionControls() = default;

  // Sets preferred min and max bitrates for the peer connection. nullopt means
  // no preference.
  virtual void SetPreferredBitrates(base::Optional<int> min_bitrate_bps,
                                    base::Optional<int> max_bitrate_bps) = 0;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_PEER_CONNECTION_CONTROLS_H_
