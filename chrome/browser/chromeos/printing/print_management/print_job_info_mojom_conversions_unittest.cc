// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/print_management/print_job_info_mojom_conversions.h"

#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/printing/history/print_job_info.pb.h"
#include "chromeos/components/print_management/mojom/printing_manager.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace proto = printing::proto;
namespace mojom = printing::printing_manager::mojom;

namespace {

constexpr char kName[] = "name";
constexpr char kUri[] = "ipp://192.168.1.5";
constexpr char kTitle[] = "title";
constexpr char kId[] = "id";
constexpr int64_t kJobCreationTime = 0;
constexpr uint32_t kPagesNumber = 3;

proto::PrintJobInfo CreatePrintJobInfoProto() {
  // Create Printer proto.
  proto::Printer printer;
  printer.set_name(kName);
  printer.set_uri(kUri);

  // Create PrintJobInfo proto.
  proto::PrintJobInfo print_job_info;

  print_job_info.set_id(kId);
  print_job_info.set_title(kTitle);
  print_job_info.set_status(
      printing::proto::PrintJobInfo_PrintJobStatus_PRINTED);
  print_job_info.set_creation_time(
      static_cast<int64_t>(base::Time::UnixEpoch().ToJsTime()));
  print_job_info.set_number_of_pages(kPagesNumber);
  *print_job_info.mutable_printer() = printer;

  return print_job_info;
}

}  // namespace

TEST(PrintJobInfoMojomConversionsTest, PrintJobProtoToMojom) {
  mojom::PrintJobInfoPtr print_job_mojo =
      printing::print_management::PrintJobProtoToMojom(
          CreatePrintJobInfoProto());

  EXPECT_EQ(kId, print_job_mojo->id);
  EXPECT_EQ(base::UTF8ToUTF16(kTitle), print_job_mojo->title);
  EXPECT_EQ(mojom::PrintJobCompletionStatus::kPrinted,
            print_job_mojo->completion_status);
  EXPECT_EQ(base::Time::FromJsTime(kJobCreationTime),
            print_job_mojo->creation_time);
  EXPECT_EQ(base::UTF8ToUTF16(kName), print_job_mojo->printer_name);
  EXPECT_EQ(kUri, print_job_mojo->printer_uri.spec());
  EXPECT_EQ(kPagesNumber, print_job_mojo->number_of_pages);
}

}  // namespace chromeos
