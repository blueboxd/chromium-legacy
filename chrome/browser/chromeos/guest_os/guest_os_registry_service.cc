// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/guest_os/guest_os_registry_service.h"

#include <utility>

#include "ash/public/cpp/app_list/app_list_config.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/crostini/crostini_features.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/guest_os/guest_os_pref_names.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "components/crx_file/id_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "ui/base/l10n/l10n_util.h"

using vm_tools::apps::App;

namespace guest_os {

namespace {

// This prefix is used when generating the crostini app list id.
constexpr char kCrostiniAppIdPrefix[] = "crostini:";

constexpr char kCrostiniIconFolder[] = "crostini.icons";

constexpr char kCrostiniAppsInstalledHistogram[] =
    "Crostini.AppsInstalledAtLogin";

std::string GenerateAppId(const std::string& desktop_file_id,
                          const std::string& vm_name,
                          const std::string& container_name) {
  // These can collide in theory because the user could choose VM and container
  // names which contain slashes, but this will only result in apps missing from
  // the launcher.
  return crx_file::id_util::GenerateId(kCrostiniAppIdPrefix + vm_name + "/" +
                                       container_name + "/" + desktop_file_id);
}

base::Value ProtoToDictionary(const App::LocaleString& locale_string) {
  base::Value result(base::Value::Type::DICTIONARY);
  for (const App::LocaleString::Entry& entry : locale_string.values()) {
    const std::string& locale = entry.locale();

    std::string locale_with_dashes(locale);
    std::replace(locale_with_dashes.begin(), locale_with_dashes.end(), '_',
                 '-');
    if (!locale.empty() && !l10n_util::IsValidLocaleSyntax(locale_with_dashes))
      continue;

    result.SetKey(locale, base::Value(entry.value()));
  }
  return result;
}

std::set<std::string> ListToStringSet(const base::Value* list) {
  std::set<std::string> result;
  if (!list)
    return result;
  for (const base::Value& value : list->GetList())
    result.insert(value.GetString());
  return result;
}

base::Value ProtoToList(
    const google::protobuf::RepeatedPtrField<std::string>& strings) {
  base::Value result(base::Value::Type::LIST);
  for (const std::string& string : strings)
    result.Append(string);
  return result;
}

base::Value LocaleStringsProtoToDictionary(
    const App::LocaleStrings& repeated_locale_string) {
  base::Value result(base::Value::Type::DICTIONARY);
  for (const auto& strings_with_locale : repeated_locale_string.values()) {
    const std::string& locale = strings_with_locale.locale();

    std::string locale_with_dashes(locale);
    std::replace(locale_with_dashes.begin(), locale_with_dashes.end(), '_',
                 '-');
    if (!locale.empty() && !l10n_util::IsValidLocaleSyntax(locale_with_dashes))
      continue;
    result.SetKey(locale, ProtoToList(strings_with_locale.value()));
  }
  return result;
}

// Construct a registration based on the given App proto.
// |name| should be |app.name()| in Dictionary form.
base::Value AppPrefRegistrationFromApp(
    const vm_tools::apps::App& app,
    GuestOsRegistryService::VmType vm_type,
    base::Value name,
    const vm_tools::apps::ApplicationList& app_list) {
  base::Value pref_registration(base::Value::Type::DICTIONARY);
  pref_registration.SetKey(guest_os::prefs::kAppDesktopFileIdKey,
                           base::Value(app.desktop_file_id()));
  pref_registration.SetIntKey(guest_os::prefs::kAppVmTypeKey,
                              static_cast<int>(vm_type));
  pref_registration.SetKey(guest_os::prefs::kAppVmNameKey,
                           base::Value(app_list.vm_name()));
  pref_registration.SetKey(guest_os::prefs::kAppContainerNameKey,
                           base::Value(app_list.container_name()));
  pref_registration.SetKey(guest_os::prefs::kAppNameKey, std::move(name));
  pref_registration.SetKey(guest_os::prefs::kAppCommentKey,
                           ProtoToDictionary(app.comment()));
  pref_registration.SetKey(guest_os::prefs::kAppExecutableFileNameKey,
                           base::Value(app.executable_file_name()));
  pref_registration.SetKey(guest_os::prefs::kAppExtensionsKey,
                           ProtoToList(app.extensions()));
  pref_registration.SetKey(guest_os::prefs::kAppMimeTypesKey,
                           ProtoToList(app.mime_types()));
  pref_registration.SetKey(guest_os::prefs::kAppKeywordsKey,
                           LocaleStringsProtoToDictionary(app.keywords()));
  pref_registration.SetKey(guest_os::prefs::kAppNoDisplayKey,
                           base::Value(app.no_display()));
  pref_registration.SetKey(guest_os::prefs::kAppStartupWMClassKey,
                           base::Value(app.startup_wm_class()));
  pref_registration.SetKey(guest_os::prefs::kAppStartupNotifyKey,
                           base::Value(app.startup_notify()));
  pref_registration.SetKey(guest_os::prefs::kAppPackageIdKey,
                           base::Value(app.package_id()));

  return pref_registration;
}

// This is the companion to GuestOsRegistryService::SetCurrentTime().
base::Time GetTime(const base::Value& pref, const char* key) {
  if (!pref.is_dict())
    return base::Time();

  const base::Value* value = pref.FindKeyOfType(key, base::Value::Type::STRING);
  int64_t time;
  if (!value || !base::StringToInt64(value->GetString(), &time))
    return base::Time();
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromMicroseconds(time));
}

bool EqualsExcludingTimestamps(const base::Value& left,
                               const base::Value& right) {
  auto left_items = left.DictItems();
  auto right_items = right.DictItems();
  auto left_iter = left_items.begin();
  auto right_iter = right_items.begin();
  while (left_iter != left_items.end() && right_iter != right_items.end()) {
    if (left_iter->first == guest_os::prefs::kAppInstallTimeKey ||
        left_iter->first == guest_os::prefs::kAppLastLaunchTimeKey) {
      ++left_iter;
      continue;
    }
    if (right_iter->first == guest_os::prefs::kAppInstallTimeKey ||
        right_iter->first == guest_os::prefs::kAppLastLaunchTimeKey) {
      ++right_iter;
      continue;
    }
    if (*left_iter != *right_iter)
      return false;
    ++left_iter;
    ++right_iter;
  }
  return left_iter == left_items.end() && right_iter == right_items.end();
}

bool InstallIconFromFileThread(const base::FilePath& icon_path,
                               const std::string& content_png) {
  DCHECK(!content_png.empty());

  base::CreateDirectory(icon_path.DirName());

  int wrote =
      base::WriteFile(icon_path, content_png.c_str(), content_png.size());
  if (wrote != static_cast<int>(content_png.size())) {
    VLOG(2) << "Failed to write Crostini icon file: "
            << icon_path.MaybeAsASCII();
    if (!base::DeleteFile(icon_path, false)) {
      VLOG(2) << "Couldn't delete broken icon file" << icon_path.MaybeAsASCII();
    }
    return false;
  }

  return true;
}

void DeleteIconFolderFromFileThread(const base::FilePath& path) {
  DCHECK(path.DirName().BaseName().MaybeAsASCII() == kCrostiniIconFolder &&
         (!base::PathExists(path) || base::DirectoryExists(path)));
  const bool deleted = base::DeleteFileRecursively(path);
  DCHECK(deleted);
}

template <typename List>
static std::string Join(const List& list);

static std::string ToString(bool b) {
  return b ? "true" : "false";
}

static std::string ToString(const std::string& string) {
  return '"' + string + '"';
}

static std::string ToString(
    const google::protobuf::RepeatedPtrField<std::string>& list) {
  return Join(list);
}

static std::string ToString(
    const vm_tools::apps::App_LocaleString_Entry& entry) {
  return "{locale: " + ToString(entry.locale()) +
         ", value: " + ToString(entry.value()) + "}";
}

static std::string ToString(
    const vm_tools::apps::App_LocaleStrings_StringsWithLocale&
        strings_with_locale) {
  return "{locale: " + ToString(strings_with_locale.locale()) +
         ", value: " + ToString(strings_with_locale.value()) + "}";
}

static std::string ToString(const vm_tools::apps::App_LocaleString& string) {
  return Join(string.values());
}

static std::string ToString(const vm_tools::apps::App_LocaleStrings& strings) {
  return Join(strings.values());
}

static std::string ToString(const vm_tools::apps::App& app) {
  return "{desktop_file_id: " + ToString(app.desktop_file_id()) +
         ", name: " + ToString(app.name()) +
         ", comment: " + ToString(app.comment()) +
         ", mime_types: " + ToString(app.mime_types()) +
         ", no_display: " + ToString(app.no_display()) +
         ", startup_wm_class: " + ToString(app.startup_wm_class()) +
         ", startup_notify: " + ToString(app.startup_notify()) +
         ", keywords: " + ToString(app.keywords()) +
         ", executable_file_name: " + ToString(app.executable_file_name()) +
         ", package_id: " + ToString(app.package_id()) +
         ", extensions: " + ToString(app.extensions()) + "}";
}

static std::string ToString(const vm_tools::apps::ApplicationList& list) {
  return "{apps: " + Join(list.apps()) +
         ", vm_name: " + ToString(list.vm_name()) +
         ", container_name: " + ToString(list.container_name()) +
         ", owner_id: " + ToString(list.owner_id()) + "}";
}

template <typename List>
static std::string Join(const List& list) {
  std::string joined = "[";
  const char* seperator = "";
  for (const auto& list_item : list) {
    joined += seperator + ToString(list_item);
    seperator = ", ";
  }
  joined += "]";
  return joined;
}

}  // namespace

GuestOsRegistryService::Registration::Registration(const base::Value* pref,
                                                   bool is_terminal_app)
    : is_terminal_app_(is_terminal_app) {
  DCHECK(pref || is_terminal_app);
  if (pref)
    pref_ = pref->Clone();
}

GuestOsRegistryService::Registration::~Registration() = default;

std::string GuestOsRegistryService::Registration::DesktopFileId() const {
  if (is_terminal_app_)
    return std::string();
  return pref_
      .FindKeyOfType(guest_os::prefs::kAppDesktopFileIdKey,
                     base::Value::Type::STRING)
      ->GetString();
}

GuestOsRegistryService::VmType GuestOsRegistryService::Registration::VmType()
    const {
  if (is_terminal_app_)
    return GuestOsRegistryService::VmType::ApplicationList_VmType_TERMINA;
  base::Optional<int> vm_type =
      pref_.FindIntKey(guest_os::prefs::kAppVmTypeKey);
  // The VmType field is new, existing Apps that do not include it must be
  // TERMINA Apps, as Plugin VM apps are not yet in production.
  if (!vm_type) {
    return GuestOsRegistryService::VmType::ApplicationList_VmType_TERMINA;
  }
  return static_cast<GuestOsRegistryService::VmType>(*vm_type);
}

std::string GuestOsRegistryService::Registration::VmName() const {
  if (is_terminal_app_)
    return crostini::kCrostiniDefaultVmName;
  return pref_
      .FindKeyOfType(guest_os::prefs::kAppVmNameKey, base::Value::Type::STRING)
      ->GetString();
}

std::string GuestOsRegistryService::Registration::ContainerName() const {
  if (is_terminal_app_)
    return crostini::kCrostiniDefaultContainerName;
  return pref_
      .FindKeyOfType(guest_os::prefs::kAppContainerNameKey,
                     base::Value::Type::STRING)
      ->GetString();
}

std::string GuestOsRegistryService::Registration::Name() const {
  if (is_terminal_app_)
    return l10n_util::GetStringUTF8(IDS_CROSTINI_TERMINAL_APP_NAME);
  return LocalizedString(guest_os::prefs::kAppNameKey);
}

std::string GuestOsRegistryService::Registration::Comment() const {
  return LocalizedString(guest_os::prefs::kAppCommentKey);
}

std::string GuestOsRegistryService::Registration::ExecutableFileName() const {
  if (pref_.is_none())
    return std::string();
  const base::Value* executable_file_name = pref_.FindKeyOfType(
      guest_os::prefs::kAppExecutableFileNameKey, base::Value::Type::STRING);
  if (!executable_file_name)
    return std::string();
  return executable_file_name->GetString();
}

std::set<std::string> GuestOsRegistryService::Registration::Extensions() const {
  if (pref_.is_none())
    return {};
  return ListToStringSet(pref_.FindKeyOfType(guest_os::prefs::kAppExtensionsKey,
                                             base::Value::Type::LIST));
}

std::set<std::string> GuestOsRegistryService::Registration::MimeTypes() const {
  if (pref_.is_none())
    return {};
  return ListToStringSet(pref_.FindKeyOfType(guest_os::prefs::kAppMimeTypesKey,
                                             base::Value::Type::LIST));
}

std::set<std::string> GuestOsRegistryService::Registration::Keywords() const {
  if (is_terminal_app_) {
    std::set<std::string> result = {"linux", "terminal", "crostini"};
    result.insert(
        l10n_util::GetStringUTF8(IDS_CROSTINI_TERMINAL_APP_SEARCH_TERMS));
    return result;
  }
  return LocalizedList(guest_os::prefs::kAppKeywordsKey);
}

bool GuestOsRegistryService::Registration::NoDisplay() const {
  if (pref_.is_none())
    return false;
  const base::Value* no_display = pref_.FindKeyOfType(
      guest_os::prefs::kAppNoDisplayKey, base::Value::Type::BOOLEAN);
  if (no_display)
    return no_display->GetBool();
  return false;
}

std::string GuestOsRegistryService::Registration::PackageId() const {
  if (is_terminal_app_)
    return std::string();
  if (pref_.is_none())
    return std::string();
  const base::Value* package_id = pref_.FindKeyOfType(
      guest_os::prefs::kAppPackageIdKey, base::Value::Type::STRING);
  if (!package_id)
    return std::string();
  return package_id->GetString();
}

bool GuestOsRegistryService::Registration::CanUninstall() const {
  if (pref_.is_none())
    return false;
  // We can uninstall if and only if there is a package that owns the
  // application. If no package owns the application, we don't know how to
  // uninstall the app.
  //
  // We don't check other things that might prevent us from uninstalling the
  // app. In particular, we don't check if there are other packages which
  // depend on the owning package. This should be rare for packages that have
  // desktop files, and it's better to show an error message (which the user can
  // then Google to learn more) than to just not have an uninstall option at
  // all.
  const base::Value* package_id = pref_.FindKeyOfType(
      guest_os::prefs::kAppPackageIdKey, base::Value::Type::STRING);
  if (package_id)
    return !package_id->GetString().empty();
  return false;
}

base::Time GuestOsRegistryService::Registration::InstallTime() const {
  return GetTime(pref_, guest_os::prefs::kAppInstallTimeKey);
}

base::Time GuestOsRegistryService::Registration::LastLaunchTime() const {
  return GetTime(pref_, guest_os::prefs::kAppLastLaunchTimeKey);
}

bool GuestOsRegistryService::Registration::IsScaled() const {
  if (pref_.is_none())
    return false;
  const base::Value* scaled = pref_.FindKeyOfType(
      guest_os::prefs::kAppScaledKey, base::Value::Type::BOOLEAN);
  if (!scaled)
    return false;
  return scaled->GetBool();
}

// We store in prefs all the localized values for given fields (formatted with
// undescores, e.g. 'fr' or 'en_US'), but users of the registry don't need to
// deal with this.
std::string GuestOsRegistryService::Registration::LocalizedString(
    base::StringPiece key) const {
  if (pref_.is_none())
    return std::string();
  const base::Value* dict =
      pref_.FindKeyOfType(key, base::Value::Type::DICTIONARY);
  if (!dict)
    return std::string();

  std::string current_locale =
      l10n_util::NormalizeLocale(g_browser_process->GetApplicationLocale());
  std::vector<std::string> locales;
  l10n_util::GetParentLocales(current_locale, &locales);
  // We use an empty locale as fallback.
  locales.push_back(std::string());

  for (const std::string& locale : locales) {
    const base::Value* value =
        dict->FindKeyOfType(locale, base::Value::Type::STRING);
    if (value)
      return value->GetString();
  }
  return std::string();
}

std::set<std::string> GuestOsRegistryService::Registration::LocalizedList(
    base::StringPiece key) const {
  if (pref_.is_none())
    return {};
  const base::Value* dict =
      pref_.FindKeyOfType(key, base::Value::Type::DICTIONARY);
  if (!dict)
    return {};

  std::string current_locale =
      l10n_util::NormalizeLocale(g_browser_process->GetApplicationLocale());
  std::vector<std::string> locales;
  l10n_util::GetParentLocales(current_locale, &locales);
  // We use an empty locale as fallback.
  locales.push_back(std::string());

  for (const std::string& locale : locales) {
    const base::Value* value =
        dict->FindKeyOfType(locale, base::Value::Type::LIST);
    if (value)
      return ListToStringSet(value);
  }
  return {};
}

GuestOsRegistryService::GuestOsRegistryService(Profile* profile)
    : profile_(profile),
      prefs_(profile->GetPrefs()),
      base_icon_path_(profile->GetPath().AppendASCII(kCrostiniIconFolder)),
      clock_(base::DefaultClock::GetInstance()) {
  RecordStartupMetrics();
  MigrateTerminal();
}

GuestOsRegistryService::~GuestOsRegistryService() = default;

base::WeakPtr<GuestOsRegistryService> GuestOsRegistryService::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

std::map<std::string, GuestOsRegistryService::Registration>
GuestOsRegistryService::GetAllRegisteredApps() const {
  const base::DictionaryValue* apps =
      prefs_->GetDictionary(guest_os::prefs::kGuestOsRegistry);
  std::map<std::string, GuestOsRegistryService::Registration> result;
  for (const auto& item : apps->DictItems()) {
    result.emplace(item.first,
                   Registration(&item.second, /*is_terminal_app=*/item.first ==
                                                  crostini::GetTerminalId()));
  }
  // TODO(crbug.com/1028898): Register Terminal as a System App rather than a
  // crostini app.
  if (!apps->FindKey(crostini::GetTerminalId())) {
    result.emplace(crostini::GetTerminalId(),
                   Registration(nullptr, /*is_terminal_app=*/true));
  }
  return result;
}

std::map<std::string, GuestOsRegistryService::Registration>
GuestOsRegistryService::GetRegisteredApps(VmType vm_type) const {
  auto apps = GetAllRegisteredApps();
  for (auto it = apps.cbegin(); it != apps.cend();) {
    if (it->second.VmType() == vm_type) {
      ++it;
    } else {
      it = apps.erase(it);
    }
  }
  return apps;
}

base::Optional<GuestOsRegistryService::Registration>
GuestOsRegistryService::GetRegistration(const std::string& app_id) const {
  const base::DictionaryValue* apps =
      prefs_->GetDictionary(guest_os::prefs::kGuestOsRegistry);
  const base::Value* pref_registration =
      apps->FindKeyOfType(app_id, base::Value::Type::DICTIONARY);

  if (app_id == crostini::GetTerminalId())
    return base::make_optional<Registration>(pref_registration, true);

  if (!pref_registration)
    return base::nullopt;
  return base::make_optional<Registration>(pref_registration, false);
}

void GuestOsRegistryService::RecordStartupMetrics() {
  const base::DictionaryValue* apps =
      prefs_->GetDictionary(guest_os::prefs::kGuestOsRegistry);

  if (!crostini::CrostiniFeatures::Get()->IsEnabled(profile_))
    return;

  size_t num_apps = 0;

  for (const auto& item : apps->DictItems()) {
    if (item.first == crostini::GetTerminalId())
      continue;

    const base::Value* no_display = item.second.FindKeyOfType(
        guest_os::prefs::kAppNoDisplayKey, base::Value::Type::BOOLEAN);
    if (no_display && no_display->GetBool())
      continue;

    num_apps++;
  }
  UMA_HISTOGRAM_COUNTS_1000(kCrostiniAppsInstalledHistogram, num_apps);
}

base::FilePath GuestOsRegistryService::GetAppPath(
    const std::string& app_id) const {
  return base_icon_path_.AppendASCII(app_id);
}

base::FilePath GuestOsRegistryService::GetIconPath(
    const std::string& app_id,
    ui::ScaleFactor scale_factor) const {
  const base::FilePath app_path = GetAppPath(app_id);
  switch (scale_factor) {
    case ui::SCALE_FACTOR_100P:
      return app_path.AppendASCII("icon_100p.png");
    case ui::SCALE_FACTOR_125P:
      return app_path.AppendASCII("icon_125p.png");
    case ui::SCALE_FACTOR_133P:
      return app_path.AppendASCII("icon_133p.png");
    case ui::SCALE_FACTOR_140P:
      return app_path.AppendASCII("icon_140p.png");
    case ui::SCALE_FACTOR_150P:
      return app_path.AppendASCII("icon_150p.png");
    case ui::SCALE_FACTOR_180P:
      return app_path.AppendASCII("icon_180p.png");
    case ui::SCALE_FACTOR_200P:
      return app_path.AppendASCII("icon_200p.png");
    case ui::SCALE_FACTOR_250P:
      return app_path.AppendASCII("icon_250p.png");
    case ui::SCALE_FACTOR_300P:
      return app_path.AppendASCII("icon_300p.png");
    default:
      NOTREACHED();
      return base::FilePath();
  }
}

void GuestOsRegistryService::MaybeRequestIcon(const std::string& app_id,
                                              ui::ScaleFactor scale_factor) {
  // First check to see if this request is already in process or not.
  const auto active_iter = active_icon_requests_.find(app_id);
  if (active_iter != active_icon_requests_.end()) {
    if (active_iter->second & (1 << scale_factor)) {
      // Icon request already in progress.
      return;
    }
  }
  const auto retry_iter = retry_icon_requests_.find(app_id);
  if (retry_iter != retry_icon_requests_.end()) {
    if (retry_iter->second & (1 << scale_factor)) {
      // Icon request already setup to be retried when we are active.
      return;
    }
  }
  RequestIcon(app_id, scale_factor);
}

void GuestOsRegistryService::ClearApplicationList(
    const std::string& vm_name,
    const std::string& container_name) {
  std::vector<std::string> removed_apps;
  // The DictionaryPrefUpdate should be destructed before calling the observer.
  {
    DictionaryPrefUpdate update(prefs_, guest_os::prefs::kGuestOsRegistry);
    base::DictionaryValue* apps = update.Get();

    for (const auto& item : apps->DictItems()) {
      if (item.first == crostini::GetTerminalId())
        continue;
      if (item.second.FindKey(guest_os::prefs::kAppVmNameKey)->GetString() ==
              vm_name &&
          (container_name.empty() ||
           item.second.FindKey(guest_os::prefs::kAppContainerNameKey)
                   ->GetString() == container_name))
        removed_apps.push_back(item.first);
    }
    for (const std::string& removed_app : removed_apps) {
      RemoveAppData(removed_app);
      apps->RemoveKey(removed_app);
    }
  }

  if (removed_apps.empty())
    return;
  std::vector<std::string> updated_apps;
  std::vector<std::string> inserted_apps;
  for (Observer& obs : observers_)
    obs.OnRegistryUpdated(this, updated_apps, removed_apps, inserted_apps);
}

void GuestOsRegistryService::UpdateApplicationList(
    const vm_tools::apps::ApplicationList& app_list) {
  VLOG(1) << "Received ApplicationList : " << ToString(app_list);

  if (app_list.vm_name().empty()) {
    LOG(WARNING) << "Received app list with missing VM name";
    return;
  }
  if (app_list.container_name().empty()) {
    LOG(WARNING) << "Received app list with missing container name";
    return;
  }

  // We need to compute the diff between the new list of apps and the old list
  // of apps (with matching vm/container names). We keep a set of the new app
  // ids so that we can compute these and update the Dictionary directly.
  std::set<std::string> new_app_ids;
  std::vector<std::string> updated_apps;
  std::vector<std::string> removed_apps;
  std::vector<std::string> inserted_apps;

  // The DictionaryPrefUpdate should be destructed before calling the observer.
  {
    DictionaryPrefUpdate update(prefs_, guest_os::prefs::kGuestOsRegistry);
    base::DictionaryValue* apps = update.Get();
    for (const App& app : app_list.apps()) {
      if (app.desktop_file_id().empty()) {
        LOG(WARNING) << "Received app with missing desktop file id";
        continue;
      }

      base::Value name = ProtoToDictionary(app.name());
      if (name.FindKey(base::StringPiece()) == nullptr) {
        LOG(WARNING) << "Received app '" << app.desktop_file_id()
                     << "' with missing unlocalized name";
        continue;
      }

      std::string app_id = GenerateAppId(
          app.desktop_file_id(), app_list.vm_name(), app_list.container_name());
      new_app_ids.insert(app_id);

      base::Value pref_registration = AppPrefRegistrationFromApp(
          app, app_list.vm_type(), std::move(name), app_list);

      base::Value* old_app = apps->FindKey(app_id);
      if (old_app && EqualsExcludingTimestamps(pref_registration, *old_app))
        continue;

      base::Value* old_install_time = nullptr;
      base::Value* old_last_launch_time = nullptr;
      if (old_app) {
        updated_apps.push_back(app_id);
        old_install_time =
            old_app->FindKey(guest_os::prefs::kAppInstallTimeKey);
        old_last_launch_time =
            old_app->FindKey(guest_os::prefs::kAppLastLaunchTimeKey);
      } else {
        inserted_apps.push_back(app_id);
      }

      if (old_install_time)
        pref_registration.SetKey(guest_os::prefs::kAppInstallTimeKey,
                                 old_install_time->Clone());
      else
        SetCurrentTime(&pref_registration, guest_os::prefs::kAppInstallTimeKey);

      if (old_last_launch_time) {
        pref_registration.SetKey(guest_os::prefs::kAppLastLaunchTimeKey,
                                 old_last_launch_time->Clone());
      }

      apps->SetKey(app_id, std::move(pref_registration));
    }

    for (const auto& item : apps->DictItems()) {
      if (item.first == crostini::GetTerminalId())
        continue;
      if (item.second.FindKey(guest_os::prefs::kAppVmNameKey)->GetString() ==
              app_list.vm_name() &&
          item.second.FindKey(guest_os::prefs::kAppContainerNameKey)
                  ->GetString() == app_list.container_name() &&
          new_app_ids.find(item.first) == new_app_ids.end()) {
        removed_apps.push_back(item.first);
      }
    }

    for (const std::string& removed_app : removed_apps) {
      RemoveAppData(removed_app);
      apps->RemoveKey(removed_app);
    }
  }

  // When we receive notification of the application list then the container
  // *should* be online and we can retry all of our icon requests that failed
  // due to the container being offline.
  for (auto retry_iter = retry_icon_requests_.begin();
       retry_iter != retry_icon_requests_.end(); ++retry_iter) {
    for (ui::ScaleFactor scale_factor : ui::GetSupportedScaleFactors()) {
      if (retry_iter->second & (1 << scale_factor)) {
        RequestIcon(retry_iter->first, scale_factor);
      }
    }
  }
  retry_icon_requests_.clear();

  if (!updated_apps.empty() || !removed_apps.empty() ||
      !inserted_apps.empty()) {
    for (Observer& obs : observers_)
      obs.OnRegistryUpdated(this, updated_apps, removed_apps, inserted_apps);
  }
}

void GuestOsRegistryService::RemoveAppData(const std::string& app_id) {
  // Remove any pending requests we have for this icon.
  active_icon_requests_.erase(app_id);
  retry_icon_requests_.erase(app_id);

  // Remove local data on filesystem for the icons.
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&DeleteIconFolderFromFileThread, GetAppPath(app_id)));
}

void GuestOsRegistryService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void GuestOsRegistryService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void GuestOsRegistryService::AppLaunched(const std::string& app_id) {
  DictionaryPrefUpdate update(prefs_, guest_os::prefs::kGuestOsRegistry);
  base::DictionaryValue* apps = update.Get();

  base::Value* app = apps->FindKey(app_id);
  if (!app) {
    DCHECK_EQ(app_id, crostini::GetTerminalId());
    base::Value pref(base::Value::Type::DICTIONARY);
    SetCurrentTime(&pref, guest_os::prefs::kAppLastLaunchTimeKey);
    apps->SetKey(app_id, std::move(pref));
    return;
  }

  SetCurrentTime(app, guest_os::prefs::kAppLastLaunchTimeKey);
}

void GuestOsRegistryService::SetCurrentTime(base::Value* dictionary,
                                            const char* key) const {
  DCHECK(dictionary);
  int64_t time = clock_->Now().ToDeltaSinceWindowsEpoch().InMicroseconds();
  dictionary->SetKey(key, base::Value(base::NumberToString(time)));
}

void GuestOsRegistryService::SetAppScaled(const std::string& app_id,
                                          bool scaled) {
  DCHECK_NE(app_id, crostini::GetTerminalId());

  DictionaryPrefUpdate update(prefs_, guest_os::prefs::kGuestOsRegistry);
  base::DictionaryValue* apps = update.Get();

  base::Value* app = apps->FindKey(app_id);
  if (!app) {
    LOG(ERROR)
        << "Tried to set display scaled property on the app with this app_id "
        << app_id << " that doesn't exist in the registry.";
    return;
  }
  app->SetKey(guest_os::prefs::kAppScaledKey, base::Value(scaled));
}

void GuestOsRegistryService::RequestIcon(const std::string& app_id,
                                         ui::ScaleFactor scale_factor) {
  // Ignore requests for app_id that isn't registered.
  base::Optional<GuestOsRegistryService::Registration> registration =
      GetRegistration(app_id);
  if (!registration) {
    VLOG(2) << "Request to load icon for non-registered app: " << app_id;
    return;
  }

  // Mark that we're doing a request for this icon so we don't duplicate
  // requests.
  active_icon_requests_[app_id] |= (1 << scale_factor);

  // Now make the call to request the actual icon.
  std::vector<std::string> desktop_file_ids{registration->DesktopFileId()};
  // We can only send integer scale factors to Crostini, so if we have a
  // non-integral scale factor we need round the scale factor. We do not expect
  // Crostini to give us back exactly what we ask for and we deal with that in
  // the CrostiniAppIcon class and may rescale the result in there to match our
  // needs.
  uint32_t icon_scale = 1;
  switch (scale_factor) {
    case ui::SCALE_FACTOR_180P:  // Close enough to 2, so use 2.
    case ui::SCALE_FACTOR_200P:
    case ui::SCALE_FACTOR_250P:  // Rounding scale factor down is better.
      icon_scale = 2;
      break;
    case ui::SCALE_FACTOR_300P:
      icon_scale = 3;
      break;
    default:
      break;
  }

  crostini::CrostiniManager::GetForProfile(profile_)->GetContainerAppIcons(
      registration->VmName(), registration->ContainerName(), desktop_file_ids,
      ash::AppListConfig::instance().grid_icon_dimension(), icon_scale,
      base::BindOnce(&GuestOsRegistryService::OnContainerAppIcon,
                     weak_ptr_factory_.GetWeakPtr(), app_id, scale_factor));
}

void GuestOsRegistryService::OnContainerAppIcon(
    const std::string& app_id,
    ui::ScaleFactor scale_factor,
    bool success,
    const std::vector<crostini::Icon>& icons) {
  if (!success) {
    // Add this to the list of retryable icon requests so we redo this when
    // we get feedback from the container that it's available.
    retry_icon_requests_[app_id] |= (1 << scale_factor);
    return;
  }
  if (icons.empty())
    return;
  // Now install the icon that we received.
  const base::FilePath icon_path = GetIconPath(app_id, scale_factor);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&InstallIconFromFileThread, icon_path, icons[0].content),
      base::BindOnce(&GuestOsRegistryService::OnIconInstalled,
                     weak_ptr_factory_.GetWeakPtr(), app_id, scale_factor));
}

void GuestOsRegistryService::OnIconInstalled(const std::string& app_id,
                                             ui::ScaleFactor scale_factor,
                                             bool success) {
  if (!success)
    return;

  for (Observer& obs : observers_)
    obs.OnAppIconUpdated(app_id, scale_factor);
}

void GuestOsRegistryService::MigrateTerminal() const {
  // Remove the old terminal from the registry.
  DictionaryPrefUpdate update(profile_->GetPrefs(),
                              guest_os::prefs::kGuestOsRegistry);
  base::DictionaryValue* apps = update.Get();
  apps->RemoveKey(crostini::GetDeletedTerminalId());

  // Transfer item attributes from old terminal to new, and delete old terminal
  // once AppListSyncableService is initialized.
  auto* app_list_syncable_service =
      app_list::AppListSyncableServiceFactory::GetForProfile(profile_);
  if (!app_list_syncable_service) {
    return;
  }
  app_list_syncable_service->on_initialized().Post(
      FROM_HERE,
      base::BindOnce(
          [](app_list::AppListSyncableService* service) {
            if (service->GetSyncItem(crostini::GetDeletedTerminalId())) {
              service->TransferItemAttributes(crostini::GetDeletedTerminalId(),
                                              crostini::GetTerminalId());
              service->RemoveItem(crostini::GetDeletedTerminalId());
            }
          },
          base::Unretained(app_list_syncable_service)));
}

}  // namespace guest_os
