// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BINDINGS_BINDINGS_MANAGER_H_
#define CHROMECAST_BINDINGS_BINDINGS_MANAGER_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/strings/string_piece.h"
#include "third_party/blink/public/common/messaging/web_message_port.h"

namespace chromecast {
namespace bindings {

// Injects Cast Platform API scripts into pages' scripting context and
// establishes bidirectional communication with them across the JS/native
// boundary.
class BindingsManager {
 public:
  using MessagePortConnectedHandler =
      base::RepeatingCallback<void(blink::WebMessagePort)>;

  BindingsManager();

  // All handlers must be Unregistered() before |this| is destroyed.
  virtual ~BindingsManager();

  // Registers a |handler| which will receive MessagePorts originating from
  // the frame's web content. |port_name| is an alphanumeric string that is
  // consistent across JS and native code.
  void RegisterPortHandler(base::StringPiece port_name,
                           MessagePortConnectedHandler handler);

  // Unregisters a previously registered handler.
  // The owner of BindingsManager is responsible for ensuring that all handlers
  // are unregistered before |this| is deleted.
  void UnregisterPortHandler(base::StringPiece port_name);

  // Registers a |binding_script| for injection in the frame.
  // Replaces registered bindings with the same |binding_name|.
  virtual void AddBinding(base::StringPiece binding_name,
                          base::StringPiece binding_script) = 0;

 protected:
  // Called by platform-specific subclasses when the underlying transport has
  // delivered a port.
  void OnPortConnected(base::StringPiece port_name, blink::WebMessagePort port);

 private:
  base::flat_map<std::string, MessagePortConnectedHandler> port_handlers_;

  DISALLOW_COPY_AND_ASSIGN(BindingsManager);
};

}  // namespace bindings
}  // namespace chromecast

#endif  // CHROMECAST_BINDINGS_BINDINGS_MANAGER_H_
