// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/serial/serial_device_enumerator.h"

#include <utility>

#include "base/unguessable_token.h"
#include "build/build_config.h"

#if defined(OS_LINUX)
#include "services/device/serial/serial_device_enumerator_linux.h"
#elif defined(OS_MACOSX)
#include "services/device/serial/serial_device_enumerator_mac.h"
#elif defined(OS_WIN)
#include "services/device/serial/serial_device_enumerator_win.h"
#endif

namespace device {

// static
std::unique_ptr<SerialDeviceEnumerator> SerialDeviceEnumerator::Create(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
#if defined(OS_LINUX)
  return std::make_unique<SerialDeviceEnumeratorLinux>();
#elif defined(OS_MACOSX)
  return std::make_unique<SerialDeviceEnumeratorMac>();
#elif defined(OS_WIN)
  return std::make_unique<SerialDeviceEnumeratorWin>(std::move(ui_task_runner));
#else
#error "No implementation of SerialDeviceEnumerator on this platform."
#endif
}

SerialDeviceEnumerator::SerialDeviceEnumerator() = default;

SerialDeviceEnumerator::~SerialDeviceEnumerator() = default;

base::Optional<base::FilePath> SerialDeviceEnumerator::GetPathFromToken(
    const base::UnguessableToken& token) {
  auto it = token_path_map_.find(token);
  if (it == token_path_map_.end())
    return base::nullopt;

  return it->second;
}

const base::UnguessableToken& SerialDeviceEnumerator::GetTokenFromPath(
    const base::FilePath& path) {
  for (const auto& pair : token_path_map_) {
    if (pair.second == path)
      return pair.first;
  }
  // A new serial path.
  return token_path_map_
      .insert(std::make_pair(base::UnguessableToken::Create(), path))
      .first->first;
}

}  // namespace device
