// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIARECORDER_MEDIA_RECORDER_HANDLER_CLIENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIARECORDER_MEDIA_RECORDER_HANDLER_CLIENT_H_

namespace WTF {
class String;
}

namespace blink {

// Interface used by a MediaRecorder to get errors and recorded data delivered.
class MediaRecorderHandlerClient {
 public:
  virtual void WriteData(const char* data,
                         size_t length,
                         bool last_inslice,
                         double timecode) = 0;

  virtual void OnError(const String& message) = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIARECORDER_MEDIA_RECORDER_HANDLER_CLIENT_H_
