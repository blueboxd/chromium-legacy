// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SERVICE_PROCESS_HOST_H_
#define CONTENT_PUBLIC_BROWSER_SERVICE_PROCESS_HOST_H_

#include <memory>
#include <tuple>
#include <utility>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "content/common/content_export.h"
#include "content/public/browser/sandbox_type.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace content {

// A ServiceProcessHost instance corresponds to a single service process in the
// system. ServiceProcessHost instances are not thread-safe, but they may be
// created on any thread.
class CONTENT_EXPORT ServiceProcessHost {
 public:
  struct CONTENT_EXPORT Options {
    Options();
    ~Options();

    Options(Options&&);

    Options& WithSandboxType(SandboxType type);
    Options& WithDisplayName(const base::string16& name);
    Options& WithDisplayName(int resource_id);

    SandboxType sandbox_type = service_manager::SANDBOX_TYPE_UTILITY;
    base::string16 display_name;
  };

  virtual ~ServiceProcessHost() {}

  // Launches a new service process for asks it to bind the given interface
  // receiver. |Interface| must be a service interface known to utility process
  // code. See content/utility/services.cc and/or
  // ContentUtilityClient::Run{Main,IO}ThreadService() methods.
  //
  // NOTE: The Interface type can be inferred from from the |receiver| argument.
  template <typename Interface>
  static std::unique_ptr<ServiceProcessHost> Launch(
      mojo::PendingReceiver<Interface> receiver,
      Options options = {}) {
    return Launch(Interface::Name_, receiver.PassPipe(), std::move(options));
  }

  // Same as above but expects |remote| to point to an unbound Remote. The
  // interface type is inferred from |remote| and upon return |*remote| is
  // bound. The process will launch with a receiver for that interface type.
  template <typename Interface>
  static std::unique_ptr<ServiceProcessHost> Launch(
      mojo::Remote<Interface>* unbound_remote,
      Options options = {}) {
    return Launch(unbound_remote->BindNewPipeAndPassReceiver(),
                  std::move(options));
  }

 protected:
  // Launches a new service process for asks it to bind a receiver for the
  // service interface named by |interface_name|. The receiving pipe must be
  // given in |pipe| and should be connected to a Remote of the same interface
  // type.
  static std::unique_ptr<ServiceProcessHost> Launch(
      base::StringPiece service_interface_name,
      mojo::ScopedMessagePipeHandle receiving_pipe,
      Options options = {});
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SERVICE_PROCESS_HOST_H_
