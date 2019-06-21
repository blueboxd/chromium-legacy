// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NATIVE_FILE_SYSTEM_PERMISSION_GRANT_H_
#define CONTENT_PUBLIC_BROWSER_NATIVE_FILE_SYSTEM_PERMISSION_GRANT_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom-forward.h"

namespace content {

// A ref-counted permission grant. This is needed so the implementation of
// this class can keep track of references to permissions, and clean up state
// when no more references remain. Multiple NativeFileSystemHandle instances
// can share the same permission grant. For example a directory and all its
// children will use the same grant.
//
// Instances of this class can be retrieved via a
// NativeFileSystemPermissionContext instance.
//
// NativeFileSystemPermissionGrant instances are not thread safe, and should
// only be used (and referenced) on the same sequence as the PermssionContext
// that created them.
class NativeFileSystemPermissionGrant
    : public base::RefCounted<NativeFileSystemPermissionGrant> {
 public:
  using PermissionStatus = blink::mojom::PermissionStatus;

  virtual PermissionStatus GetStatus() const = 0;

  // Call this method to request permission for this grant. The |callback|
  // should be called after the status of this grant has been updated with
  // the outcome of the request.
  virtual void RequestPermission(int process_id,
                                 int frame_id,
                                 base::OnceClosure callback) = 0;

 protected:
  friend class base::RefCounted<NativeFileSystemPermissionGrant>;
  virtual ~NativeFileSystemPermissionGrant() = default;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NATIVE_FILE_SYSTEM_PERMISSION_GRANT_H_
