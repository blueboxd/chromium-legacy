// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/guest_os_file_tasks.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/crostini/crostini_features.h"
#include "chrome/browser/chromeos/crostini/crostini_mime_types_service.h"
#include "chrome/browser/chromeos/crostini/crostini_mime_types_service_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/guest_os/guest_os_registry_service.h"
#include "chrome/browser/chromeos/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_files.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"
#include "extensions/browser/entry_info.h"
#include "storage/browser/file_system/file_system_url.h"
#include "ui/base/layout.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "url/gurl.h"

namespace file_manager {
namespace file_tasks {

namespace {

constexpr base::TimeDelta kIconLoadTimeout =
    base::TimeDelta::FromMilliseconds(100);
constexpr size_t kIconSizeInDip = 16;
// When MIME type detection is done; if we can't be properly determined then
// the detection system will end up guessing one of the 2 values below depending
// upon whether it "thinks" it is binary or text content.
constexpr char kUnknownBinaryMimeType[] = "application/octet-stream";
constexpr char kUnknownTextMimeType[] = "text/plain";

GURL GeneratePNGDataUrl(const SkBitmap& sk_bitmap) {
  std::vector<unsigned char> output;
  gfx::PNGCodec::EncodeBGRASkBitmap(sk_bitmap, false /* discard_transparency */,
                                    &output);
  std::string encoded;
  base::Base64Encode(
      base::StringPiece(reinterpret_cast<const char*>(output.data()),
                        output.size()),
      &encoded);
  return GURL("data:image/png;base64," + encoded);
}

void OnAppIconsLoaded(
    Profile* profile,
    const std::vector<std::string>& app_ids,
    const std::vector<std::string>& app_names,
    const std::vector<guest_os::GuestOsRegistryService::VmType>& vm_types,
    ui::ScaleFactor scale_factor,
    std::vector<FullTaskDescriptor>* result_list,
    base::OnceClosure completion_closure,
    const std::vector<gfx::ImageSkia>& icons) {
  DCHECK(!app_ids.empty());
  DCHECK_EQ(app_ids.size(), icons.size());

  float scale = ui::GetScaleForScaleFactor(scale_factor);

  std::vector<TaskType> task_types;
  task_types.reserve(vm_types.size());
  for (auto vm_type : vm_types) {
    switch (vm_type) {
      case guest_os::GuestOsRegistryService::VmType::
          ApplicationList_VmType_TERMINA:
        task_types.push_back(TASK_TYPE_CROSTINI_APP);
        break;
      case guest_os::GuestOsRegistryService::VmType::
          ApplicationList_VmType_PLUGIN_VM:
        task_types.push_back(TASK_TYPE_PLUGIN_VM_APP);
        break;
      default:
        LOG(ERROR) << "Unsupported VmType: " << static_cast<int>(vm_type);
        return;
    }
  }

  for (size_t i = 0; i < app_ids.size(); ++i) {
    result_list->push_back(FullTaskDescriptor(
        TaskDescriptor(app_ids[i], task_types[i], kGuestOsAppActionID),
        app_names[i],
        extensions::api::file_manager_private::Verb::VERB_OPEN_WITH,
        GeneratePNGDataUrl(icons[i].GetRepresentation(scale).GetBitmap()),
        false /* is_default */, false /* is_generic */,
        false /* is_file_extension_match */));
  }

  std::move(completion_closure).Run();
}

void OnTaskComplete(FileTaskFinishedCallback done,
                    bool success,
                    const std::string& failure_reason) {
  if (!success) {
    LOG(ERROR) << "Crostini task error: " << failure_reason;
  }
  std::move(done).Run(
      success ? extensions::api::file_manager_private::TASK_RESULT_MESSAGE_SENT
              : extensions::api::file_manager_private::TASK_RESULT_FAILED,
      failure_reason);
}

bool HasSupportedMimeType(
    const std::set<std::string>& supported_mime_types,
    const std::string& vm_name,
    const std::string& container_name,
    const crostini::CrostiniMimeTypesService& mime_types_service,
    const extensions::EntryInfo& entry) {
  if (supported_mime_types.find(entry.mime_type) !=
      supported_mime_types.end()) {
    return true;
  }

  // Allow files with type text/* to be opened with a text/plain application
  // as per xdg spec.
  // https://specifications.freedesktop.org/shared-mime-info-spec/shared-mime-info-spec-latest.html.
  // TODO(crbug.com/1032910): Add xdg mime support for aliases, subclasses.
  if (base::StartsWith(entry.mime_type, "text/",
                       base::CompareCase::SENSITIVE) &&
      supported_mime_types.find(kUnknownTextMimeType) !=
          supported_mime_types.end()) {
    return true;
  }

  // If we see either of these then we use the Linux container MIME type
  // mappings as alternates for finding an appropriate app since these are
  // the defaults when Chrome can't figure out the exact MIME type (but they
  // can also be the actual MIME type, so we don't exclude them above).
  if (entry.mime_type == kUnknownBinaryMimeType ||
      entry.mime_type == kUnknownTextMimeType) {
    const std::string& alternate_mime_type =
        mime_types_service.GetMimeType(entry.path, vm_name, container_name);
    if (supported_mime_types.find(alternate_mime_type) !=
        supported_mime_types.end()) {
      return true;
    }
  }

  return false;
}

bool HasSupportedExtension(const std::set<std::string>& supported_extensions,
                           const extensions::EntryInfo& entry) {
  const auto& extension = entry.path.Extension();
  if (extension.size() <= 1 || extension[0] != '.')
    return false;
  // Strip the leading period.
  return supported_extensions.find({extension.begin() + 1, extension.end()}) !=
         supported_extensions.end();
}

bool AppSupportsAllEntries(
    const crostini::CrostiniMimeTypesService& mime_types_service,
    const std::vector<extensions::EntryInfo>& entries,
    const guest_os::GuestOsRegistryService::Registration& app) {
  guest_os::GuestOsRegistryService::VmType vm_type = app.VmType();
  switch (vm_type) {
    case guest_os::GuestOsRegistryService::VmType::
        ApplicationList_VmType_TERMINA:
      return std::all_of(
          cbegin(entries), cend(entries),
          // Get fields once, as their getters are not cheap.
          [supported_mime_types = app.MimeTypes(), vm_name = app.VmName(),
           container_name = app.ContainerName(),
           &mime_types_service](const auto& entry) {
            return HasSupportedMimeType(supported_mime_types, vm_name,
                                        container_name, mime_types_service,
                                        entry);
          });
    case guest_os::GuestOsRegistryService::VmType::
        ApplicationList_VmType_PLUGIN_VM:
      return std::all_of(
          cbegin(entries), cend(entries),
          [supported_extensions = app.Extensions()](const auto& entry) {
            return HasSupportedExtension(supported_extensions, entry);
          });
    default:
      LOG(ERROR) << "Unsupported VmType: " << static_cast<int>(vm_type);
      return false;
  }
}

}  // namespace

void FindGuestOsApps(
    Profile* profile,
    const std::vector<extensions::EntryInfo>& entries,
    std::vector<std::string>* app_ids,
    std::vector<std::string>* app_names,
    std::vector<guest_os::GuestOsRegistryService::VmType>* vm_types) {
  auto* registry_service =
      guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile);
  crostini::CrostiniMimeTypesService* mime_types_service =
      crostini::CrostiniMimeTypesServiceFactory::GetForProfile(profile);
  for (const auto& pair : registry_service->GetAllRegisteredApps()) {
    const std::string& app_id = pair.first;
    const auto& registration = pair.second;

    if (!AppSupportsAllEntries(*mime_types_service, entries, registration))
      continue;

    app_ids->push_back(app_id);
    app_names->push_back(registration.Name());
    vm_types->push_back(registration.VmType());
  }
}

void FindGuestOsTasks(Profile* profile,
                      const std::vector<extensions::EntryInfo>& entries,
                      std::vector<FullTaskDescriptor>* result_list,
                      base::OnceClosure completion_closure) {
  if (!crostini::CrostiniFeatures::Get()->IsUIAllowed(profile) &&
      !plugin_vm::IsPluginVmEnabled(profile)) {
    std::move(completion_closure).Run();
    return;
  }

  std::vector<std::string> result_app_ids;
  std::vector<std::string> result_app_names;
  std::vector<guest_os::GuestOsRegistryService::VmType> result_vm_types;
  FindGuestOsApps(profile, entries, &result_app_ids, &result_app_names,
                  &result_vm_types);

  if (result_app_ids.empty()) {
    std::move(completion_closure).Run();
    return;
  }

  ui::ScaleFactor scale_factor = ui::GetSupportedScaleFactors().back();

  crostini::LoadIcons(
      profile, result_app_ids, kIconSizeInDip, scale_factor, kIconLoadTimeout,
      base::BindOnce(OnAppIconsLoaded, profile, result_app_ids,
                     std::move(result_app_names), std::move(result_vm_types),
                     scale_factor, result_list, std::move(completion_closure)));
}

void ExecuteGuestOsTask(
    Profile* profile,
    const TaskDescriptor& task,
    const std::vector<storage::FileSystemURL>& file_system_urls,
    FileTaskFinishedCallback done) {
  auto* registry_service =
      guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile);

  base::Optional<guest_os::GuestOsRegistryService::Registration> registration =
      registry_service->GetRegistration(task.app_id);
  if (!registration) {
    std::move(done).Run(
        extensions::api::file_manager_private::TASK_RESULT_FAILED,
        "Unknown app_id: " + task.app_id);
    return;
  }

  guest_os::GuestOsRegistryService::VmType vm_type = registration->VmType();
  switch (vm_type) {
    case guest_os::GuestOsRegistryService::VmType::
        ApplicationList_VmType_TERMINA:
      DCHECK(crostini::CrostiniFeatures::Get()->IsUIAllowed(profile));
      crostini::LaunchCrostiniApp(
          profile, task.app_id, display::kInvalidDisplayId, file_system_urls,
          base::BindOnce(OnTaskComplete, std::move(done)));
      return;
    case guest_os::GuestOsRegistryService::VmType::
        ApplicationList_VmType_PLUGIN_VM:
      DCHECK(plugin_vm::IsPluginVmEnabled(profile));
      plugin_vm::LaunchPluginVmApp(
          profile, task.app_id, file_system_urls,
          base::BindOnce(OnTaskComplete, std::move(done)));
      return;
    default:
      std::move(done).Run(
          extensions::api::file_manager_private::TASK_RESULT_FAILED,
          base::StringPrintf("Unsupported VmType: %d",
                             static_cast<int>(vm_type)));
  }
}

}  // namespace file_tasks
}  // namespace file_manager
