// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_API_BINDINGS_SCOPED_API_BINDING_H_
#define COMPONENTS_CAST_API_BINDINGS_SCOPED_API_BINDING_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/check.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "components/cast/cast_component_export.h"
#include "third_party/blink/public/common/messaging/web_message_port.h"

namespace cast_api_bindings {

class Manager;

// Manages the registration of bindings Javascript and establishment of
// communication channels, as well as unregistration on object teardown, using
// RAII semantics.
class CAST_COMPONENT_EXPORT ScopedApiBinding
    : public blink::WebMessagePort::MessageReceiver {
 public:
  // Methods for handling message I/O with bindings scripts.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Expresses the name for which MessagePort connection requests should be
    // routed to the Delegate implementation.
    virtual base::StringPiece GetPortName() const = 0;

    // Invoked when |message_port_| is connected. This allows the Delegate to do
    // work when the client first connects, e.g. sending it a message conveying
    // some initial state.
    virtual void OnConnected() {}

    // Invoked for messages received over |message_port_|.
    virtual bool OnMessage(base::StringPiece message) = 0;

    // Invoked when |message_port_| is disconnected.
    // Allows the delegate to cleanup if the client unexpectedly disconnects.
    virtual void OnDisconnected() {}
  };

  // |bindings_manager|: Specifies the Manager to which the binding will be
  //     published.
  // |delegate|: If set, provides the necessary identifier and
  //     method implementations for connecting script message I/O with the
  //     bindings backend.
  //     Must outlive |this|.
  //     Can be nullptr if the bindings do not require communication.
  // |js_bindings_id|: Id used for management of the |js_bindings| script.
  //     Must be unique.
  // |js_bindings|: script to inject.
  ScopedApiBinding(Manager* bindings_manager,
                   Delegate* delegate,
                   base::StringPiece js_bindings_id,
                   base::StringPiece js_bindings);
  ~ScopedApiBinding() final;

  ScopedApiBinding(const ScopedApiBinding&) = delete;
  ScopedApiBinding& operator=(const ScopedApiBinding&) = delete;

  // Sends a |message| to |message_port_|.
  // Returns true if the message was sent.
  bool SendMessage(base::StringPiece message);

 private:
  // Called when a port is received from the page.
  void OnPortConnected(blink::WebMessagePort port);

  // blink::WebMessagePort::MessageReceiver implementation:
  bool OnMessage(blink::WebMessagePort::Message message) final;
  void OnPipeError() final;

  Manager* const bindings_manager_;
  Delegate* const delegate_;
  const std::string js_bindings_id_;

  // The MessagePort used to receive messages from the receiver JS.
  blink::WebMessagePort message_port_;
};

}  // namespace cast_api_bindings

#endif  // COMPONENTS_CAST_API_BINDINGS_SCOPED_API_BINDING_H_
