// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_MAC_PERMISSION_WIZARD_H_
#define REMOTING_HOST_MAC_PERMISSION_WIZARD_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace remoting {
namespace mac {

// This class implements a wizard-style UI which guides the user to granting all
// needed MacOS permissions for the host process.
class PermissionWizard final {
 public:
  PermissionWizard();
  ~PermissionWizard();

  void Start(scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);

 private:
  class Impl;

  // Private implementation, to hide the Objective-C and Cocoa objects from C++
  // callers.
  std::unique_ptr<Impl> impl_;

  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(PermissionWizard);
};

}  // namespace mac
}  // namespace remoting

#endif  // REMOTING_HOST_MAC_PERMISSION_WIZARD_H_
