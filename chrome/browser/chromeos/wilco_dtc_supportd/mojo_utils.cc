// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/wilco_dtc_supportd/mojo_utils.h"

#include <cstdint>
#include <cstring>
#include <utility>

#include "base/file_descriptor_posix.h"
#include "base/files/file.h"
#include "base/memory/shared_memory_handle.h"
#include "base/unguessable_token.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace chromeos {

base::StringPiece GetStringPieceFromMojoHandle(
    mojo::ScopedHandle handle,
    std::unique_ptr<base::SharedMemory>* shared_memory) {
  base::PlatformFile platform_file;
  auto result = mojo::UnwrapPlatformFile(std::move(handle), &platform_file);
  shared_memory->reset();
  if (result != MOJO_RESULT_OK) {
    return base::StringPiece();
  }
  base::UnguessableToken guid = base::UnguessableToken::Create();
  *shared_memory = std::make_unique<base::SharedMemory>(
      base::SharedMemoryHandle(
          base::FileDescriptor(platform_file, true /* iauto_close */), 0u,
          guid),
      true /* read_only */);

  base::SharedMemoryHandle dup_shared_memory_handle =
      base::SharedMemory::DuplicateHandle((*shared_memory)->handle());
  const int64_t file_size =
      base::File(dup_shared_memory_handle.GetHandle()).GetLength();
  if (file_size <= 0) {
    shared_memory->reset();
    return base::StringPiece();
  }
  if (!(*shared_memory)->Map(file_size)) {
    shared_memory->reset();
    return base::StringPiece();
  }
  return base::StringPiece(static_cast<const char*>((*shared_memory)->memory()),
                           (*shared_memory)->mapped_size());
}

mojo::ScopedHandle CreateReadOnlySharedMemoryMojoHandle(
    const std::string& content) {
  if (content.empty()) {
    return mojo::ScopedHandle();
  }

  base::SharedMemory shared_memory;
  base::SharedMemoryCreateOptions options;
  options.size = content.length();
  options.share_read_only = true;
  if (!shared_memory.Create(base::SharedMemoryCreateOptions(options)) ||
      !shared_memory.Map(content.length())) {
    return mojo::ScopedHandle();
  }
  memcpy(shared_memory.memory(), content.data(), content.length());
  return mojo::WrapPlatformFile(shared_memory.TakeHandle().GetHandle());
}

}  // namespace chromeos
