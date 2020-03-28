// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/print_management/print_job_info_mojom_conversions.h"

#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/printing/history/print_job_info.pb.h"
#include "url/gurl.h"

namespace chromeos {
namespace printing {
namespace print_management {
namespace {

mojom::PrintJobCompletionStatus PrintJobStatusProtoToMojom(
    proto::PrintJobInfo_PrintJobStatus print_job_status_proto) {
  switch (print_job_status_proto) {
    case proto::PrintJobInfo_PrintJobStatus_FAILED:
      return mojom::PrintJobCompletionStatus::kFailed;
    case proto::PrintJobInfo_PrintJobStatus_CANCELED:
      return mojom::PrintJobCompletionStatus::kCanceled;
    case proto::PrintJobInfo_PrintJobStatus_PRINTED:
      return mojom::PrintJobCompletionStatus::kPrinted;
    case proto::
        PrintJobInfo_PrintJobStatus_PrintJobInfo_PrintJobStatus_INT_MIN_SENTINEL_DO_NOT_USE_:
    case proto::
        PrintJobInfo_PrintJobStatus_PrintJobInfo_PrintJobStatus_INT_MAX_SENTINEL_DO_NOT_USE_:
      NOTREACHED();
      return mojom::PrintJobCompletionStatus::kFailed;
  }
  return mojom::PrintJobCompletionStatus::kFailed;
}

}  // namespace

mojom::PrintJobInfoPtr PrintJobProtoToMojom(
    const proto::PrintJobInfo& print_job_info_proto) {
  mojom::PrintJobInfoPtr print_job_mojom = mojom::PrintJobInfo::New();

  print_job_mojom->id = print_job_info_proto.id();
  print_job_mojom->title = base::UTF8ToUTF16(print_job_info_proto.title());
  print_job_mojom->completion_status =
      PrintJobStatusProtoToMojom(print_job_info_proto.status());
  print_job_mojom->creation_time =
      base::Time::FromJsTime(print_job_info_proto.creation_time());
  print_job_mojom->number_of_pages = print_job_info_proto.number_of_pages();
  print_job_mojom->printer_name =
      base::UTF8ToUTF16(print_job_info_proto.printer().name());
  print_job_mojom->printer_uri = GURL(print_job_info_proto.printer().uri());
  return print_job_mojom;
}

}  // namespace print_management
}  // namespace printing
}  // namespace chromeos
