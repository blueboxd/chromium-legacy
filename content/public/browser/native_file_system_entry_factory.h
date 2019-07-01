// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NATIVE_FILE_SYSTEM_ENTRY_FACTORY_H_
#define CONTENT_PUBLIC_BROWSER_NATIVE_FILE_SYSTEM_ENTRY_FACTORY_H_

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_directory_handle.mojom-forward.h"
#include "url/origin.h"

namespace content {

// Exposes methods for creating NativeFileSystemEntries.
class CONTENT_EXPORT NativeFileSystemEntryFactory
    : public base::RefCountedThreadSafe<NativeFileSystemEntryFactory,
                                        BrowserThread::DeleteOnIOThread> {
 public:
  struct CONTENT_EXPORT BindingContext {
    BindingContext(const url::Origin& origin, int process_id, int frame_id)
        : origin(origin), process_id(process_id), frame_id(frame_id) {}
    url::Origin origin;
    int process_id;
    int frame_id;
  };

  // Creates a new NativeFileSystemEntryPtr from the path to a file. Assumes the
  // passed in path is valid and represents a file.
  virtual blink::mojom::NativeFileSystemEntryPtr CreateFileEntryFromPath(
      const BindingContext& binding_context,
      const base::FilePath& file_path) = 0;

  // Creates a new NativeFileSystemEntryPtr from the path to a directory.
  // Assumes the passed in path is valid and represents a directory.
  virtual blink::mojom::NativeFileSystemEntryPtr CreateDirectoryEntryFromPath(
      const BindingContext& binding_context,
      const base::FilePath& directory_path) = 0;

 protected:
  friend struct BrowserThread::DeleteOnThread<BrowserThread::IO>;
  friend class base::DeleteHelper<NativeFileSystemEntryFactory>;
  virtual ~NativeFileSystemEntryFactory() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NATIVE_FILE_SYSTEM_ENTRY_FACTORY_H_
