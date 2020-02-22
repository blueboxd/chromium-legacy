// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_WIN_MF_HELPERS_H_
#define MEDIA_BASE_WIN_MF_HELPERS_H_

#include <mfapi.h>
#include <stdint.h>
#include <wrl/client.h>

#include "base/logging.h"
#include "base/macros.h"
#include "media/base/win/mf_initializer_export.h"

namespace media {

// Macros that contain return statements can make code harder to read. Only use
// these when necessary, e.g. in places where we deal with a lot of Windows API
// calls, for each of which we have to check the returned HRESULT.
// See discussion thread at:
// https://groups.google.com/a/chromium.org/d/msg/cxx/zw5Xmcs--S4/r7Fwb-TsCAAJ

#define RETURN_IF_FAILED(expr)                                          \
  do {                                                                  \
    HRESULT hresult = (expr);                                           \
    if (FAILED(hresult)) {                                              \
      DLOG(ERROR) << __func__ << ": failed with \""                     \
                  << logging::SystemErrorCodeToString(hresult) << "\""; \
      return hresult;                                                   \
    }                                                                   \
  } while (0)

#define RETURN_ON_FAILURE(success, log, ret) \
  do {                                       \
    if (!(success)) {                        \
      DLOG(ERROR) << log;                    \
      return ret;                            \
    }                                        \
  } while (0)

#define RETURN_ON_HR_FAILURE(hresult, log, ret)                               \
  RETURN_ON_FAILURE(SUCCEEDED(hresult),                                       \
                    log << ", " << logging::SystemErrorCodeToString(hresult), \
                    ret);

// Creates a Media Foundation sample with one buffer of length |buffer_length|
// on a |align|-byte boundary. Alignment must be a perfect power of 2 or 0.
MF_INITIALIZER_EXPORT Microsoft::WRL::ComPtr<IMFSample>
CreateEmptySampleWithBuffer(uint32_t buffer_length, int align);

// Provides scoped access to the underlying buffer in an IMFMediaBuffer
// instance.
class MF_INITIALIZER_EXPORT MediaBufferScopedPointer {
 public:
  MediaBufferScopedPointer(IMFMediaBuffer* media_buffer);
  ~MediaBufferScopedPointer();

  uint8_t* get() { return buffer_; }
  DWORD current_length() const { return current_length_; }

 private:
  Microsoft::WRL::ComPtr<IMFMediaBuffer> media_buffer_;
  uint8_t* buffer_;
  DWORD max_length_;
  DWORD current_length_;

  DISALLOW_COPY_AND_ASSIGN(MediaBufferScopedPointer);
};

// Wrap around the usage of device handle from |device_manager|.
class MF_INITIALIZER_EXPORT DXGIDeviceScopedHandle {
 public:
  explicit DXGIDeviceScopedHandle(IMFDXGIDeviceManager* device_manager);

  ~DXGIDeviceScopedHandle();

  HRESULT LockDevice(REFIID riid, void** device_out);

 private:
  Microsoft::WRL::ComPtr<IMFDXGIDeviceManager> device_manager_;

  HANDLE device_handle_ = INVALID_HANDLE_VALUE;
};

}  // namespace media

#endif  // MEDIA_BASE_WIN_MF_HELPERS_H_
