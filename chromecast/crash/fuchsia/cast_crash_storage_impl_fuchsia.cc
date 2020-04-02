// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/crash/fuchsia/cast_crash_storage_impl_fuchsia.h"

#include <fuchsia/feedback/cpp/fidl.h>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/strings/string_util.h"
#include "chromecast/crash/cast_crash_keys.h"
#include "chromecast/crash/fuchsia/constants.h"

namespace chromecast {
namespace {

void ConvertToFuchsiaKey(base::StringPiece key, std::string* out) {
  base::ReplaceChars(key.as_string(), "_", "-", out);
}

fuchsia::feedback::Annotation MakeAnnotation(base::StringPiece key,
                                             base::StringPiece value) {
  fuchsia::feedback::Annotation annotation;
  ConvertToFuchsiaKey(key, &annotation.key);
  annotation.value = value.as_string();
  return annotation;
}

}  // namespace

CastCrashStorageImplFuchsia::CastCrashStorageImplFuchsia(
    const sys::ServiceDirectory* incoming_directory)
    : incoming_directory_(incoming_directory) {
  DCHECK(incoming_directory_);
}

CastCrashStorageImplFuchsia::~CastCrashStorageImplFuchsia() = default;

void CastCrashStorageImplFuchsia::SetLastLaunchedApp(base::StringPiece app_id) {
  UpsertAnnotations({MakeAnnotation(crash_keys::kLastApp, app_id)});
}

void CastCrashStorageImplFuchsia::ClearLastLaunchedApp() {
  UpsertAnnotations(
      {MakeAnnotation(crash_keys::kLastApp, base::StringPiece())});
}

void CastCrashStorageImplFuchsia::SetCurrentApp(base::StringPiece app_id) {
  UpsertAnnotations({MakeAnnotation(crash_keys::kCurrentApp, app_id)});
}

void CastCrashStorageImplFuchsia::ClearCurrentApp() {
  UpsertAnnotations(
      {MakeAnnotation(crash_keys::kCurrentApp, base::StringPiece())});
}

void CastCrashStorageImplFuchsia::SetPreviousApp(base::StringPiece app_id) {
  UpsertAnnotations({MakeAnnotation(crash_keys::kPreviousApp, app_id)});
}

void CastCrashStorageImplFuchsia::ClearPreviousApp() {
  UpsertAnnotations(
      {MakeAnnotation(crash_keys::kPreviousApp, base::StringPiece())});
}

void CastCrashStorageImplFuchsia::SetStadiaSessionId(
    base::StringPiece session_id) {
  UpsertAnnotations({MakeAnnotation(crash_keys::kStadiaSessionId, session_id)});
}

void CastCrashStorageImplFuchsia::ClearStadiaSessionId() {
  UpsertAnnotations(
      {MakeAnnotation(crash_keys::kStadiaSessionId, base::StringPiece())});
}

void CastCrashStorageImplFuchsia::UpsertAnnotations(
    std::vector<fuchsia::feedback::Annotation> annotations) {
  fuchsia::feedback::ComponentDataRegisterPtr component_data_register;
  incoming_directory_->Connect<fuchsia::feedback::ComponentDataRegister>(
      component_data_register.NewRequest());
  component_data_register.set_error_handler([](zx_status_t status) {
    ZX_CHECK(status == ZX_OK, status)
        << "Unable to connect to Feedback service.";
  });

  fuchsia::feedback::ComponentData component_data;
  component_data.set_namespace_(crash::kCastNamespace);
  component_data.set_annotations(std::move(annotations));
  component_data_register->Upsert(std::move(component_data), []() {});
}

}  // namespace chromecast
