// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/printing/print_job_submitter.h"

#include <cstring>
#include <utility>

#include "base/bind.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/chromeos/extensions/printing/print_job_controller.h"
#include "chrome/browser/chromeos/extensions/printing/printer_capabilities_provider.h"
#include "chrome/browser/chromeos/extensions/printing/printing_api_utils.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager.h"
#include "chrome/browser/printing/printing_service.h"
#include "chrome/services/printing/public/mojom/pdf_flattener.mojom.h"
#include "chromeos/printing/printer_configuration.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/blob_reader.h"
#include "printing/backend/print_backend.h"
#include "printing/metafile_skia.h"
#include "printing/print_settings.h"

namespace extensions {

namespace {

constexpr char kPdfMimeType[] = "application/pdf";

// PDF document format identifier.
constexpr char kPdfMagicBytes[] = "%PDF";

constexpr char kUnsupportedContentType[] = "Unsupported content type";
constexpr char kInvalidTicket[] = "Invalid ticket";
constexpr char kInvalidPrinterId[] = "Invalid printer ID";
constexpr char kPrinterUnavailable[] = "Printer is unavailable at the moment";
constexpr char kUnsupportedTicket[] =
    "Ticket is unsupported on the given printer";
constexpr char kInvalidData[] = "Invalid document";

}  // namespace

PrintJobSubmitter::PrintJobSubmitter(
    content::BrowserContext* browser_context,
    chromeos::CupsPrintersManager* printers_manager,
    PrinterCapabilitiesProvider* printer_capabilities_provider,
    PrintJobController* print_job_controller,
    mojo::Remote<printing::mojom::PdfFlattener>* pdf_flattener,
    const std::string& extension_id,
    api::printing::SubmitJobRequest request)
    : browser_context_(browser_context),
      printers_manager_(printers_manager),
      printer_capabilities_provider_(printer_capabilities_provider),
      print_job_controller_(print_job_controller),
      pdf_flattener_(pdf_flattener),
      extension_id_(extension_id),
      request_(std::move(request)) {}

PrintJobSubmitter::~PrintJobSubmitter() = default;

void PrintJobSubmitter::Start(SubmitJobCallback callback) {
  callback_ = std::move(callback);
  if (!CheckContentType()) {
    FireErrorCallback(kUnsupportedContentType);
    return;
  }
  if (!CheckPrintTicket()) {
    FireErrorCallback(kInvalidTicket);
    return;
  }
  CheckPrinter();
}

bool PrintJobSubmitter::CheckContentType() const {
  return request_.job.content_type == kPdfMimeType;
}

bool PrintJobSubmitter::CheckPrintTicket() {
  settings_ = ParsePrintTicket(
      base::Value::FromUniquePtrValue(request_.job.ticket.ToValue()));
  if (!settings_)
    return false;
  settings_->set_title(base::UTF8ToUTF16(request_.job.title));
  settings_->set_device_name(base::UTF8ToUTF16(request_.job.printer_id));
  return true;
}

void PrintJobSubmitter::CheckPrinter() {
  base::Optional<chromeos::Printer> printer =
      printers_manager_->GetPrinter(request_.job.printer_id);
  if (!printer) {
    FireErrorCallback(kInvalidPrinterId);
    return;
  }
  printer_capabilities_provider_->GetPrinterCapabilities(
      request_.job.printer_id,
      base::BindOnce(&PrintJobSubmitter::CheckCapabilitiesCompatibility,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PrintJobSubmitter::CheckCapabilitiesCompatibility(
    base::Optional<printing::PrinterSemanticCapsAndDefaults> capabilities) {
  if (!capabilities) {
    FireErrorCallback(kPrinterUnavailable);
    return;
  }
  if (!CheckSettingsAndCapabilitiesCompatibility(*settings_, *capabilities)) {
    FireErrorCallback(kUnsupportedTicket);
    return;
  }
  ReadDocumentData();
}

void PrintJobSubmitter::ReadDocumentData() {
  DCHECK(request_.document_blob_uuid);
  BlobReader::Read(browser_context_, *request_.document_blob_uuid,
                   base::BindOnce(&PrintJobSubmitter::OnDocumentDataRead,
                                  weak_ptr_factory_.GetWeakPtr()));
}

void PrintJobSubmitter::OnDocumentDataRead(std::unique_ptr<std::string> data,
                                           int64_t total_blob_length) {
  if (!data ||
      !base::StartsWith(*data, kPdfMagicBytes, base::CompareCase::SENSITIVE)) {
    FireErrorCallback(kInvalidData);
    return;
  }

  base::MappedReadOnlyRegion memory =
      base::ReadOnlySharedMemoryRegion::Create(data->length());
  if (!memory.IsValid()) {
    FireErrorCallback(kInvalidData);
    return;
  }
  memcpy(memory.mapping.memory(), data->data(), data->length());

  if (!pdf_flattener_->is_bound()) {
    GetPrintingService()->BindPdfFlattener(
        pdf_flattener_->BindNewPipeAndPassReceiver());
    pdf_flattener_->set_disconnect_handler(
        base::BindOnce(&PrintJobSubmitter::OnPdfFlattenerDisconnected,
                       weak_ptr_factory_.GetWeakPtr()));
  }
  (*pdf_flattener_)
      ->FlattenPdf(std::move(memory.region),
                   base::BindOnce(&PrintJobSubmitter::OnPdfFlattened,
                                  weak_ptr_factory_.GetWeakPtr()));
}

void PrintJobSubmitter::OnPdfFlattened(
    base::ReadOnlySharedMemoryRegion flattened_pdf) {
  const auto mapping = flattened_pdf.Map();
  if (!mapping.IsValid()) {
    FireErrorCallback(kInvalidData);
    return;
  }

  auto metafile = std::make_unique<printing::MetafileSkia>();
  CHECK(metafile->InitFromData(mapping.memory(), mapping.size()));

  print_job_controller_->StartPrintJob(
      extension_id_, std::move(metafile), std::move(settings_),
      base::BindOnce(&PrintJobSubmitter::OnPrintJobSubmitted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PrintJobSubmitter::OnPdfFlattenerDisconnected() {
  FireErrorCallback(kInvalidData);
}

void PrintJobSubmitter::OnPrintJobSubmitted(
    std::unique_ptr<std::string> job_id) {
  DCHECK(job_id);
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback_), api::printing::SUBMIT_JOB_STATUS_OK,
                     std::move(job_id), base::nullopt));
}

void PrintJobSubmitter::FireErrorCallback(const std::string& error) {
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback_), base::nullopt, nullptr, error));
}

}  // namespace extensions
