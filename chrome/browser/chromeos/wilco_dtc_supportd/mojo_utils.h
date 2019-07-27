// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_WILCO_DTC_SUPPORTD_MOJO_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_WILCO_DTC_SUPPORTD_MOJO_UTILS_H_

#include <memory>
#include <string>

#include "base/memory/shared_memory.h"
#include "base/strings/string_piece.h"
#include "mojo/public/cpp/system/handle.h"

namespace chromeos {

// Allows to get access to the buffer in read only shared memory. It converts
// mojo::Handle to base::SharedMemory and returns a string content.
//
// |handle| must be a valid mojo handle of the non-empty buffer in the shared
// memory.
// |shared_memory| must be a valid allocated unique pointer.
//
// Returns an empty string and nullptr |shared_memory| if error.
base::StringPiece GetStringPieceFromMojoHandle(
    mojo::ScopedHandle handle,
    std::unique_ptr<base::SharedMemory>* shared_memory);

// Allocates buffer in shared memory, copies |content| to the buffer and
// converts shared buffer handle into |mojo::ScopedHandle|.
//
// Allocated shared memory is read only for another process.
//
// Returns invalid |mojo::ScopedHandle| if |content| is empty or error happened.
mojo::ScopedHandle CreateReadOnlySharedMemoryMojoHandle(
    const std::string& content);

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_WILCO_DTC_SUPPORTD_MOJO_UTILS_H_
