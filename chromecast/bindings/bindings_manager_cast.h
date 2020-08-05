// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BINDINGS_BINDINGS_MANAGER_CAST_H_
#define CHROMECAST_BINDINGS_BINDINGS_MANAGER_CAST_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "chromecast/bindings/bindings_manager.h"
#include "chromecast/browser/cast_web_contents.h"
#include "mojo/public/cpp/bindings/connector.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace chromecast {
namespace bindings {

// Implements the CastOS BindingsManager.
class BindingsManagerCast : public BindingsManager,
                            public CastWebContents::Observer,
                            public blink::WebMessagePort::MessageReceiver {
 public:
  explicit BindingsManagerCast(chromecast::CastWebContents* cast_web_contents);
  ~BindingsManagerCast() override;

  // The document and its statically-declared subresources are loaded.
  // BindingsManagerCast will inject all registered bindings at this time.
  // BindingsManagerCast will post a message that conveys an end of MessagePort
  // to the loaded page, so that the NamedMessagePort binding could utilize the
  // port to communicate with the native part.
  void OnPageLoaded();

  // BindingsManager implementation.
  void AddBinding(base::StringPiece binding_name,
                  base::StringPiece binding_script) override;

 private:
  // CastWebContents::Observer implementation.
  void OnPageStateChanged(CastWebContents* cast_web_contents) override;

  // blink::WebMessagePort::MessageReceiver implementation:
  bool OnMessage(blink::WebMessagePort::Message message) override;
  void OnPipeError() override;

  chromecast::CastWebContents* cast_web_contents_;

  // Receives messages from JS.
  blink::WebMessagePort blink_port_;

  DISALLOW_COPY_AND_ASSIGN(BindingsManagerCast);
};

}  // namespace bindings
}  // namespace chromecast

#endif  // CHROMECAST_BINDINGS_BINDINGS_MANAGER_CAST_H_
