// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/tts/tts_service.h"

#include <dlfcn.h>

#include "base/files/file_util.h"
#include "chromeos/services/tts/constants.h"

namespace chromeos {
namespace tts {

// Simple helper to bridge logging in the shared library to Chrome's logging.
void HandleLibraryLogging(int severity, const char* message) {
  switch (severity) {
    case logging::LOG_INFO:
      // Suppressed.
      break;
    case logging::LOG_WARNING:
      LOG(WARNING) << message;
      break;
    case logging::LOG_ERROR:
      LOG(ERROR) << message;
      break;
    default:
      break;
  }
}

// TtsService is mostly glue code that adapts the TtsStream interface into a
// form needed by libchrometts.so. As is convention with shared objects, the
// lifetime of all arguments passed to the library is scoped to the function.
//
// To keep the library interface stable and prevent name mangling, all library
// methods utilize C features only.

TtsService::TtsService(mojo::PendingReceiver<mojom::TtsService> receiver)
    : service_receiver_(this, std::move(receiver)), stream_receiver_(this) {
  bool loaded = libchrometts_.Load(kLibchromettsPath);
  if (!loaded)
    LOG(ERROR) << "Unable to load libchrometts.so: " << dlerror();
  else
    libchrometts_.GoogleTtsSetLogger(HandleLibraryLogging);
}

TtsService::~TtsService() = default;

void TtsService::BindTtsStream(
    mojo::PendingReceiver<mojom::TtsStream> receiver) {
  stream_receiver_.Bind(std::move(receiver));
}

void TtsService::InstallVoice(const std::string& voice_name,
                              const std::vector<uint8_t>& voice_bytes,
                              InstallVoiceCallback callback) {
  // Create a directory to place extracted voice data.
  base::FilePath voice_data_path(kTempDataDirectory);
  voice_data_path = voice_data_path.Append(voice_name);
  if (base::DirectoryExists(voice_data_path)) {
    std::move(callback).Run(true);
    return;
  }

  if (!base::CreateDirectoryAndGetError(voice_data_path, nullptr)) {
    std::move(callback).Run(false);
    return;
  }

  std::move(callback).Run(libchrometts_.GoogleTtsInstallVoice(
      voice_data_path.value().c_str(), (char*)&voice_bytes[0],
      voice_bytes.size()));
}

void TtsService::SelectVoice(const std::string& voice_name,
                             SelectVoiceCallback callback) {
  base::FilePath path_prefix =
      base::FilePath(kTempDataDirectory).Append(voice_name);
  base::FilePath pipeline_path = path_prefix.Append("pipeline");
  std::move(callback).Run(libchrometts_.GoogleTtsInit(
      pipeline_path.value().c_str(), path_prefix.value().c_str()));
}

void TtsService::Init(const std::vector<uint8_t>& text_jspb,
                      InitCallback callback) {
  std::move(callback).Run(libchrometts_.GoogleTtsInitBuffered(
      (char*)&text_jspb[0], text_jspb.size()));
}

void TtsService::Read(ReadCallback callback) {
  int32_t status = libchrometts_.GoogleTtsReadBuffered();
  if (status == -1) {
    std::move(callback).Run(mojom::TtsStreamItem::New(
        std::vector<uint8_t>(), true, std::vector<mojom::TimepointPtr>()));
    return;
  }

  char* event = libchrometts_.GoogleTtsGetEventBufferPtr();
  std::vector<uint8_t> send_event(libchrometts_.GoogleTtsGetEventBufferLen());
  for (size_t i = 0; i < send_event.size(); i++)
    send_event[i] = event[i];

  std::vector<mojom::TimepointPtr> timepoints(
      libchrometts_.GoogleTtsGetTimepointsCount());
  for (size_t i = 0; i < timepoints.size(); i++) {
    timepoints[i] = mojom::Timepoint::New(
        libchrometts_.GoogleTtsGetTimepointsTimeInSecsAtIndex(i),
        libchrometts_.GoogleTtsGetTimepointsCharIndexAtIndex(i));
  }

  std::move(callback).Run(mojom::TtsStreamItem::New(send_event, status == 0,
                                                    std::move(timepoints)));
}

void TtsService::Finalize() {
  libchrometts_.GoogleTtsFinalizeBuffered();
}

}  // namespace tts
}  // namespace chromeos
