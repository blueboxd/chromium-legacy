// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/instant_service.h"

#include <stddef.h>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/scoped_observer.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/image_fetcher/image_decoder_impl.h"
#include "chrome/browser/ntp_tiles/chrome_most_visited_sites_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/background/ntp_background_service.h"
#include "chrome/browser/search/background/ntp_background_service_factory.h"
#include "chrome/browser/search/instant_io_context.h"
#include "chrome/browser/search/instant_service_observer.h"
#include "chrome/browser/search/local_ntp_source.h"
#include "chrome/browser/search/most_visited_iframe_source.h"
#include "chrome/browser/search/ntp_features.h"
#include "chrome/browser/search/ntp_icon_source.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/search.mojom.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/theme_resources.h"
#include "components/ntp_tiles/constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/search/search.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_observer.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/url_data_source.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/dark_mode_observer.h"

namespace {

const char kNtpCustomBackgroundURL[] = "background_url";
const char kNtpCustomBackgroundAttributionLine1[] = "attribution_line_1";
const char kNtpCustomBackgroundAttributionLine2[] = "attribution_line_2";
const char kNtpCustomBackgroundAttributionActionURL[] =
    "attribution_action_url";

const char kCustomBackgroundsUmaClientName[] = "NtpCustomBackgrounds";

base::DictionaryValue GetBackgroundInfoAsDict(
    const GURL& background_url,
    const std::string& attribution_line_1,
    const std::string& attribution_line_2,
    const GURL& action_url) {
  base::DictionaryValue background_info;
  background_info.SetKey(kNtpCustomBackgroundURL,
                         base::Value(background_url.spec()));
  background_info.SetKey(kNtpCustomBackgroundAttributionLine1,
                         base::Value(attribution_line_1));
  background_info.SetKey(kNtpCustomBackgroundAttributionLine2,
                         base::Value(attribution_line_2));
  background_info.SetKey(kNtpCustomBackgroundAttributionActionURL,
                         base::Value(action_url.spec()));

  return background_info;
}

// |GetBackgroundInfoWithColor| has to return new object so that updated version
// gets synced.
base::DictionaryValue GetBackgroundInfoWithColor(
    const base::DictionaryValue* background_info,
    const SkColor color) {
  base::DictionaryValue new_background_info;
  auto url = const_cast<base::Value&&>(
      *background_info->FindKey(kNtpCustomBackgroundURL));
  auto attribution_line_1 = const_cast<base::Value&&>(
      *background_info->FindKey(kNtpCustomBackgroundAttributionLine1));
  auto attribution_line_2 = const_cast<base::Value&&>(
      *background_info->FindKey(kNtpCustomBackgroundAttributionLine2));
  auto action_url = const_cast<base::Value&&>(
      *background_info->FindKey(kNtpCustomBackgroundAttributionActionURL));

  new_background_info.SetKey(kNtpCustomBackgroundURL, url.Clone());
  new_background_info.SetKey(kNtpCustomBackgroundAttributionLine1,
                             attribution_line_1.Clone());
  new_background_info.SetKey(kNtpCustomBackgroundAttributionLine2,
                             attribution_line_2.Clone());
  new_background_info.SetKey(kNtpCustomBackgroundAttributionActionURL,
                             action_url.Clone());
  new_background_info.SetKey(kNtpCustomBackgroundMainColor,
                             base::Value((int)color));
  return new_background_info;
}

base::Value NtpCustomBackgroundDefaults() {
  base::Value defaults(base::Value::Type::DICTIONARY);
  defaults.SetKey(kNtpCustomBackgroundURL,
                  base::Value(base::Value::Type::STRING));
  defaults.SetKey(kNtpCustomBackgroundAttributionLine1,
                  base::Value(base::Value::Type::STRING));
  defaults.SetKey(kNtpCustomBackgroundAttributionLine2,
                  base::Value(base::Value::Type::STRING));
  defaults.SetKey(kNtpCustomBackgroundAttributionActionURL,
                  base::Value(base::Value::Type::STRING));
  return defaults;
}

void CopyFileToProfilePath(const base::FilePath& from_path,
                           const base::FilePath& profile_path) {
  base::CopyFile(from_path,
                 profile_path.AppendASCII(
                     chrome::kChromeSearchLocalNtpBackgroundFilename));
}

void DoDeleteThumbnailDataIfExists(
    const base::FilePath& database_dir,
    base::Optional<base::OnceCallback<void(bool)>> callback) {
  bool result = false;
  if (base::PathExists(database_dir)) {
    base::DeleteFile(database_dir, true);
    result = true;
  }
  if (callback.has_value())
    std::move(*callback).Run(result);
}

// |GetBitmapMainColor| just wraps |CalculateKMeanColorOfBitmap|.
// As |CalculateKMeanColorOfBitmap| is overloaded, it cannot be bind for async
// call.
SkColor GetBitmapMainColor(const SkBitmap& bitmap) {
  return color_utils::CalculateKMeanColorOfBitmap(bitmap);
}

}  // namespace

const char kNtpCustomBackgroundMainColor[] = "background_main_color";

// Keeps track of any changes in search engine provider and notifies
// InstantService if a third-party search provider (i.e. a third-party NTP) is
// being used.
class InstantService::SearchProviderObserver
    : public TemplateURLServiceObserver {
 public:
  explicit SearchProviderObserver(TemplateURLService* service,
                                  base::RepeatingClosure callback)
      : service_(service),
        is_google_(search::DefaultSearchProviderIsGoogle(service_)),
        callback_(std::move(callback)) {
    DCHECK(service_);
    service_->AddObserver(this);
  }

  ~SearchProviderObserver() override {
    if (service_)
      service_->RemoveObserver(this);
  }

  bool is_google() { return is_google_; }

 private:
  void OnTemplateURLServiceChanged() override {
    is_google_ = search::DefaultSearchProviderIsGoogle(service_);
    callback_.Run();
  }

  void OnTemplateURLServiceShuttingDown() override {
    service_->RemoveObserver(this);
    service_ = nullptr;
  }

  TemplateURLService* service_;
  bool is_google_;
  base::RepeatingClosure callback_;
};

InstantService::InstantService(Profile* profile)
    : profile_(profile),
      most_visited_info_(std::make_unique<InstantMostVisitedInfo>()),
      pref_service_(profile_->GetPrefs()),
      weak_ptr_factory_(this) {
  // The initialization below depends on a typical set of browser threads. Skip
  // it if we are running in a unit test without the full suite.
  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI))
    return;

  // This depends on the existence of the typical browser threads. Therefore it
  // is only instantiated here (after the check for a UI thread above).
  instant_io_context_ = new InstantIOContext();

  registrar_.Add(this,
                 content::NOTIFICATION_RENDERER_PROCESS_CREATED,
                 content::NotificationService::AllSources());
  registrar_.Add(this,
                 content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
                 content::NotificationService::AllSources());

  most_visited_sites_ = ChromeMostVisitedSitesFactory::NewForProfile(profile_);
  if (most_visited_sites_) {
    // Determine if we are using a third-party NTP. Custom links should only be
    // enabled for the default NTP.
    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(profile_);
    if (template_url_service) {
      search_provider_observer_ = std::make_unique<SearchProviderObserver>(
          template_url_service,
          base::BindRepeating(&InstantService::OnSearchProviderChanged,
                              weak_ptr_factory_.GetWeakPtr()));
    }

    // 9 tiles are required for the custom links feature in order to balance the
    // Most Visited rows (this is due to an additional "Add" button).
    most_visited_sites_->SetMostVisitedURLsObserver(
        this, ntp_tiles::kMaxNumMostVisited);
    most_visited_sites_->EnableCustomLinks(IsCustomLinksEnabled());
  }

  most_visited_info_->use_most_visited = !IsCustomLinksEnabled();
  most_visited_info_->is_visible =
      pref_service_->GetBoolean(prefs::kNtpShortcutsVisible);

  if (profile_) {
    DeleteThumbnailDataIfExists(profile_->GetPath(), base::nullopt);

    if (profile_->GetResourceContext()) {
      base::PostTaskWithTraits(
          FROM_HERE, {content::BrowserThread::IO},
          base::BindOnce(&InstantIOContext::SetUserDataOnIO,
                         profile->GetResourceContext(), instant_io_context_));
    }
  }

  CreateDarkModeObserver(ui::NativeTheme::GetInstanceForNativeUi());

  background_service_ = NtpBackgroundServiceFactory::GetForProfile(profile_);

  // Listen for theme installation.
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
                 content::Source<ThemeService>(
                     ThemeServiceFactory::GetForProfile(profile_)));

  // Set up the data sources that Instant uses on the NTP.
  content::URLDataSource::Add(profile_,
                              std::make_unique<ThemeSource>(profile_));
  content::URLDataSource::Add(profile_,
                              std::make_unique<LocalNtpSource>(profile_));
  content::URLDataSource::Add(profile_,
                              std::make_unique<NtpIconSource>(profile_));
  content::URLDataSource::Add(profile_,
                              std::make_unique<FaviconSource>(profile_));
  content::URLDataSource::Add(profile_,
                              std::make_unique<MostVisitedIframeSource>());

  // Update theme info when the pref is changed via Sync.
  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      prefs::kNtpCustomBackgroundDict,
      base::BindRepeating(&InstantService::UpdateBackgroundFromSync,
                          weak_ptr_factory_.GetWeakPtr()));

  image_fetcher_ = std::make_unique<image_fetcher::ImageFetcherImpl>(
      std::make_unique<ImageDecoderImpl>(),
      content::BrowserContext::GetDefaultStoragePartition(profile_)
          ->GetURLLoaderFactoryForBrowserProcess());
}

InstantService::~InstantService() = default;

void InstantService::AddInstantProcess(int process_id) {
  process_ids_.insert(process_id);

  if (instant_io_context_.get()) {
    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::IO},
        base::BindOnce(&InstantIOContext::AddInstantProcessOnIO,
                       instant_io_context_, process_id));
  }
}

bool InstantService::IsInstantProcess(int process_id) const {
  return process_ids_.find(process_id) != process_ids_.end();
}

void InstantService::AddObserver(InstantServiceObserver* observer) {
  observers_.AddObserver(observer);
}

void InstantService::RemoveObserver(InstantServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

void InstantService::OnNewTabPageOpened() {
  if (most_visited_sites_) {
    most_visited_sites_->Refresh();
  }
}

void InstantService::DeleteMostVisitedItem(const GURL& url) {
  if (most_visited_sites_) {
    most_visited_sites_->AddOrRemoveBlacklistedUrl(url, true);
  }
}

void InstantService::UndoMostVisitedDeletion(const GURL& url) {
  if (most_visited_sites_) {
    most_visited_sites_->AddOrRemoveBlacklistedUrl(url, false);
  }
}

void InstantService::UndoAllMostVisitedDeletions() {
  if (most_visited_sites_) {
    most_visited_sites_->ClearBlacklistedUrls();
  }
}

bool InstantService::AddCustomLink(const GURL& url, const std::string& title) {
  return most_visited_sites_ &&
         most_visited_sites_->AddCustomLink(url, base::UTF8ToUTF16(title));
}

bool InstantService::UpdateCustomLink(const GURL& url,
                                      const GURL& new_url,
                                      const std::string& new_title) {
  return most_visited_sites_ && most_visited_sites_->UpdateCustomLink(
                                    url, new_url, base::UTF8ToUTF16(new_title));
}

bool InstantService::ReorderCustomLink(const GURL& url, int new_pos) {
  return most_visited_sites_ &&
         most_visited_sites_->ReorderCustomLink(url, new_pos);
}

bool InstantService::DeleteCustomLink(const GURL& url) {
  return most_visited_sites_ && most_visited_sites_->DeleteCustomLink(url);
}

bool InstantService::UndoCustomLinkAction() {
  // Non-Google NTPs are not supported.
  if (!most_visited_sites_ || !search_provider_observer_ ||
      !search_provider_observer_->is_google()) {
    return false;
  }
  most_visited_sites_->UndoCustomLinkAction();
  return true;
}

bool InstantService::ResetCustomLinks() {
  // Non-Google NTPs are not supported.
  if (!most_visited_sites_ || !search_provider_observer_ ||
      !search_provider_observer_->is_google()) {
    return false;
  }
  most_visited_sites_->UninitializeCustomLinks();
  return true;
}

bool InstantService::ToggleMostVisitedOrCustomLinks() {
  // Non-Google NTPs are not supported.
  if (!most_visited_sites_ || !search_provider_observer_ ||
      !search_provider_observer_->is_google()) {
    return false;
  }
  bool use_most_visited =
      !pref_service_->GetBoolean(prefs::kNtpUseMostVisitedTiles);
  pref_service_->SetBoolean(prefs::kNtpUseMostVisitedTiles, use_most_visited);
  most_visited_info_->use_most_visited = use_most_visited;

  // Custom links is enabled if Most Visited is disabled.
  most_visited_sites_->EnableCustomLinks(!use_most_visited);
  return true;
}

bool InstantService::ToggleShortcutsVisibility() {
  // Non-Google NTPs are not supported.
  if (!most_visited_sites_ || !search_provider_observer_ ||
      !search_provider_observer_->is_google()) {
    return false;
  }
  bool is_visible = !pref_service_->GetBoolean(prefs::kNtpShortcutsVisible);
  pref_service_->SetBoolean(prefs::kNtpShortcutsVisible, is_visible);
  most_visited_info_->is_visible = is_visible;

  NotifyAboutMostVisitedInfo();
  return true;
}

void InstantService::UpdateThemeInfo() {
  ApplyOrResetCustomBackgroundThemeInfo();

  NotifyAboutThemeInfo();
}

void InstantService::UpdateBackgroundFromSync() {
  // Any incoming change to synced background data should clear the local image.
  pref_service_->SetBoolean(prefs::kNtpCustomBackgroundLocalToDevice, false);
  UpdateThemeInfo();
}

void InstantService::UpdateMostVisitedInfo() {
  NotifyAboutMostVisitedInfo();
}

void InstantService::SendNewTabPageURLToRenderer(
    content::RenderProcessHost* rph) {
  if (auto* channel = rph->GetChannel()) {
    chrome::mojom::SearchBouncerAssociatedPtr client;
    channel->GetRemoteAssociatedInterface(&client);
    client->SetNewTabPageURL(search::GetNewTabPageURL(profile_));
  }
}

void InstantService::SetCustomBackgroundURL(const GURL& url) {
  SetCustomBackgroundURLWithAttributions(url, std::string(), std::string(),
                                         GURL());
}

void InstantService::SetCustomBackgroundURLWithAttributions(
    const GURL& background_url,
    const std::string& attribution_line_1,
    const std::string& attribution_line_2,
    const GURL& action_url) {
  bool is_backdrop_url =
      background_service_ &&
      background_service_->IsValidBackdropUrl(background_url);

  bool need_forced_refresh =
      pref_service_->GetBoolean(prefs::kNtpCustomBackgroundLocalToDevice) &&
      pref_service_->FindPreference(prefs::kNtpCustomBackgroundDict)
          ->IsDefaultValue();
  pref_service_->SetBoolean(prefs::kNtpCustomBackgroundLocalToDevice, false);
  RemoveLocalBackgroundImageCopy();

  if (background_url.is_valid() && is_backdrop_url) {
    const GURL& thumbnail_url =
        background_service_->GetThumbnailUrl(background_url);
    FetchCustomBackground(background_url, thumbnail_url.is_valid()
                                              ? thumbnail_url
                                              : background_url);

    base::DictionaryValue background_info = GetBackgroundInfoAsDict(
        background_url, attribution_line_1, attribution_line_2, action_url);
    pref_service_->Set(prefs::kNtpCustomBackgroundDict, background_info);
  } else {
    pref_service_->ClearPref(prefs::kNtpCustomBackgroundDict);

    // If this device was using a local image and did not have a non-local
    // background saved, UpdateBackgroundFromSync will not fire. Therefore, we
    // need to force a refresh here.
    if (need_forced_refresh) {
      UpdateThemeInfo();
    }
  }
}

void InstantService::SetBackgroundToLocalResource() {
  pref_service_->SetBoolean(prefs::kNtpCustomBackgroundLocalToDevice, true);
  UpdateThemeInfo();
}

void InstantService::SelectLocalBackgroundImage(const base::FilePath& path) {
  base::PostTaskWithTraitsAndReply(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(&CopyFileToProfilePath, path, profile_->GetPath()),
      base::BindOnce(&InstantService::SetBackgroundToLocalResource,
                     weak_ptr_factory_.GetWeakPtr()));
}

ThemeBackgroundInfo* InstantService::GetInitializedThemeInfo() {
  if (!theme_info_)
    BuildThemeInfo();
  return theme_info_.get();
}

void InstantService::SetDarkModeThemeForTesting(ui::NativeTheme* theme) {
  CreateDarkModeObserver(theme);
}

void InstantService::Shutdown() {
  process_ids_.clear();

  if (instant_io_context_.get()) {
    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::IO},
        base::BindOnce(&InstantIOContext::ClearInstantProcessesOnIO,
                       instant_io_context_));
  }

  if (most_visited_sites_) {
    most_visited_sites_.reset();
  }

  instant_io_context_ = NULL;
}

void InstantService::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_RENDERER_PROCESS_CREATED: {
      content::RenderProcessHost* rph =
          content::Source<content::RenderProcessHost>(source).ptr();
      Profile* renderer_profile =
          static_cast<Profile*>(rph->GetBrowserContext());
      if (profile_ == renderer_profile)
        SendNewTabPageURLToRenderer(rph);
      break;
    }
    case content::NOTIFICATION_RENDERER_PROCESS_TERMINATED: {
      content::RenderProcessHost* rph =
          content::Source<content::RenderProcessHost>(source).ptr();
      Profile* renderer_profile =
          static_cast<Profile*>(rph->GetBrowserContext());
      if (profile_ == renderer_profile)
        OnRendererProcessTerminated(rph->GetID());
      break;
    }
    case chrome::NOTIFICATION_BROWSER_THEME_CHANGED:
      theme_info_ = nullptr;
      UpdateThemeInfo();
      break;
    default:
      NOTREACHED() << "Unexpected notification type in InstantService.";
  }
}

void InstantService::OnRendererProcessTerminated(int process_id) {
  process_ids_.erase(process_id);

  if (instant_io_context_.get()) {
    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::IO},
        base::BindOnce(&InstantIOContext::RemoveInstantProcessOnIO,
                       instant_io_context_, process_id));
  }
}

void InstantService::OnSearchProviderChanged() {
  DCHECK(most_visited_sites_);
  most_visited_sites_->EnableCustomLinks(IsCustomLinksEnabled());
}

void InstantService::OnDarkModeChanged(bool dark_mode) {
  // Force theme information rebuild in order to update dark mode colors.
  BuildThemeInfo();
  UpdateThemeInfo();
}

void InstantService::OnURLsAvailable(
    const std::map<ntp_tiles::SectionType, ntp_tiles::NTPTilesVector>&
        sections) {
  DCHECK(most_visited_sites_);
  most_visited_info_->items.clear();
  // Use only personalized tiles for instant service.
  const ntp_tiles::NTPTilesVector& tiles =
      sections.at(ntp_tiles::SectionType::PERSONALIZED);
  for (const ntp_tiles::NTPTile& tile : tiles) {
    InstantMostVisitedItem item;
    item.url = tile.url;
    item.title = tile.title;
    item.favicon = tile.favicon_url;
    item.source = tile.source;
    item.title_source = tile.title_source;
    item.data_generation_time = tile.data_generation_time;
    most_visited_info_->items.push_back(item);
  }
  most_visited_info_->items_are_custom_links =
      (most_visited_sites_ && most_visited_sites_->IsCustomLinksInitialized());

  NotifyAboutMostVisitedInfo();
}

void InstantService::OnIconMadeAvailable(const GURL& site_url) {}

void InstantService::NotifyAboutMostVisitedInfo() {
  for (InstantServiceObserver& observer : observers_)
    observer.MostVisitedInfoChanged(*most_visited_info_);
}

void InstantService::NotifyAboutThemeInfo() {
  for (InstantServiceObserver& observer : observers_)
    observer.ThemeInfoChanged(*theme_info_);
}

bool InstantService::IsCustomLinksEnabled() {
  return search_provider_observer_ && search_provider_observer_->is_google() &&
         !pref_service_->GetBoolean(prefs::kNtpUseMostVisitedTiles);
}

void InstantService::BuildThemeInfo() {
  // Get theme information from theme service.
  theme_info_.reset(new ThemeBackgroundInfo());

  // Get if the current theme is the default theme.
  ThemeService* theme_service = ThemeServiceFactory::GetForProfile(profile_);
  theme_info_->using_default_theme = theme_service->UsingDefaultTheme();

  theme_info_->using_dark_mode = dark_mode_observer_->InDarkMode();

  // Get theme colors.
  const ui::ThemeProvider& theme_provider =
      ThemeService::GetThemeProviderForProfile(profile_);
  SkColor background_color =
      theme_provider.GetColor(ThemeProperties::COLOR_NTP_BACKGROUND);
  SkColor text_color = theme_provider.GetColor(ThemeProperties::COLOR_NTP_TEXT);
  SkColor text_color_light =
      theme_provider.GetColor(ThemeProperties::COLOR_NTP_TEXT_LIGHT);

  // Set colors.
  theme_info_->background_color = background_color;
  theme_info_->text_color = text_color;
  theme_info_->text_color_light = text_color_light;

  int logo_alternate =
      theme_provider.GetDisplayProperty(ThemeProperties::NTP_LOGO_ALTERNATE);
  theme_info_->logo_alternate = logo_alternate == 1;

  if (theme_provider.HasCustomImage(IDR_THEME_NTP_BACKGROUND)) {
    // Set theme id for theme background image url.
    theme_info_->theme_id = theme_service->GetThemeID();

    // Set theme background image horizontal alignment.
    int alignment = theme_provider.GetDisplayProperty(
        ThemeProperties::NTP_BACKGROUND_ALIGNMENT);
    if (alignment & ThemeProperties::ALIGN_LEFT)
      theme_info_->image_horizontal_alignment = THEME_BKGRND_IMAGE_ALIGN_LEFT;
    else if (alignment & ThemeProperties::ALIGN_RIGHT)
      theme_info_->image_horizontal_alignment = THEME_BKGRND_IMAGE_ALIGN_RIGHT;
    else
      theme_info_->image_horizontal_alignment = THEME_BKGRND_IMAGE_ALIGN_CENTER;

    // Set theme background image vertical alignment.
    if (alignment & ThemeProperties::ALIGN_TOP)
      theme_info_->image_vertical_alignment = THEME_BKGRND_IMAGE_ALIGN_TOP;
    else if (alignment & ThemeProperties::ALIGN_BOTTOM)
      theme_info_->image_vertical_alignment = THEME_BKGRND_IMAGE_ALIGN_BOTTOM;
    else
      theme_info_->image_vertical_alignment = THEME_BKGRND_IMAGE_ALIGN_CENTER;

    // Set theme background image tiling.
    int tiling = theme_provider.GetDisplayProperty(
        ThemeProperties::NTP_BACKGROUND_TILING);
    switch (tiling) {
      case ThemeProperties::NO_REPEAT:
        theme_info_->image_tiling = THEME_BKGRND_IMAGE_NO_REPEAT;
        break;
      case ThemeProperties::REPEAT_X:
        theme_info_->image_tiling = THEME_BKGRND_IMAGE_REPEAT_X;
        break;
      case ThemeProperties::REPEAT_Y:
        theme_info_->image_tiling = THEME_BKGRND_IMAGE_REPEAT_Y;
        break;
      case ThemeProperties::REPEAT:
        theme_info_->image_tiling = THEME_BKGRND_IMAGE_REPEAT;
        break;
    }

    theme_info_->has_attribution =
        theme_provider.HasCustomImage(IDR_THEME_NTP_ATTRIBUTION);
  }
}

void InstantService::ApplyOrResetCustomBackgroundThemeInfo() {
  // Custom backgrounds for non-Google search providers are not supported.
  if (!search::DefaultSearchProviderIsGoogle(profile_)) {
    ResetCustomBackgroundThemeInfo();
    return;
  }

  if (pref_service_->GetBoolean(prefs::kNtpCustomBackgroundLocalToDevice)) {
    // Add a timestamp to the url to prevent the browser from using a cached
    // version when "Upload an image" is used multiple times.
    std::string time_string = std::to_string(base::Time::Now().ToTimeT());
    std::string local_string(chrome::kChromeSearchLocalNtpBackgroundUrl);
    GURL timestamped_url(local_string + "?ts=" + time_string);
    GetInitializedThemeInfo()->custom_background_url = timestamped_url;
    GetInitializedThemeInfo()->custom_background_attribution_line_1 =
        std::string();
    GetInitializedThemeInfo()->custom_background_attribution_line_2 =
        std::string();
    GetInitializedThemeInfo()->custom_background_attribution_action_url =
        GURL();
    return;
  }

  // Attempt to get custom background URL from preferences.
  GURL custom_background_url;
  if (!IsCustomBackgroundPrefValid(custom_background_url)) {
    ResetCustomBackgroundThemeInfo();
    return;
  }

  ApplyCustomBackgroundThemeInfo();
}

void InstantService::ApplyCustomBackgroundThemeInfo() {
  const base::DictionaryValue* background_info =
      pref_service_->GetDictionary(prefs::kNtpCustomBackgroundDict);
  GURL custom_background_url(
      background_info->FindKey(kNtpCustomBackgroundURL)->GetString());

  // Set custom background information in theme info (attributions are
  // optional).
  const base::Value* attribution_line_1 =
      background_info->FindKey(kNtpCustomBackgroundAttributionLine1);
  const base::Value* attribution_line_2 =
      background_info->FindKey(kNtpCustomBackgroundAttributionLine2);
  const base::Value* attribution_action_url =
      background_info->FindKey(kNtpCustomBackgroundAttributionActionURL);
  ThemeBackgroundInfo* theme_info = GetInitializedThemeInfo();
  theme_info->custom_background_url = custom_background_url;

  if (attribution_line_1) {
    theme_info->custom_background_attribution_line_1 =
        background_info->FindKey(kNtpCustomBackgroundAttributionLine1)
            ->GetString();
  }
  if (attribution_line_2) {
    theme_info->custom_background_attribution_line_2 =
        background_info->FindKey(kNtpCustomBackgroundAttributionLine2)
            ->GetString();
  }
  if (attribution_action_url) {
    GURL action_url(
        background_info->FindKey(kNtpCustomBackgroundAttributionActionURL)
            ->GetString());

    if (!action_url.SchemeIsCryptographic()) {
      theme_info->custom_background_attribution_action_url = GURL();
    } else {
      theme_info->custom_background_attribution_action_url = action_url;
    }
  }
}

void InstantService::ResetCustomBackgroundThemeInfo() {
  pref_service_->ClearPref(prefs::kNtpCustomBackgroundDict);
  pref_service_->SetBoolean(prefs::kNtpCustomBackgroundLocalToDevice, false);
  RemoveLocalBackgroundImageCopy();
  FallbackToDefaultThemeInfo();
}

void InstantService::FallbackToDefaultThemeInfo() {
  ThemeBackgroundInfo* theme_info = GetInitializedThemeInfo();
  theme_info->custom_background_url = GURL();
  theme_info->custom_background_attribution_line_1 = std::string();
  theme_info->custom_background_attribution_line_2 = std::string();
  theme_info->custom_background_attribution_action_url = GURL();
}

bool InstantService::IsCustomBackgroundSet() {
  if (pref_service_->GetBoolean(prefs::kNtpCustomBackgroundLocalToDevice))
    return true;

  GURL custom_background_url;
  if (!IsCustomBackgroundPrefValid(custom_background_url))
    return false;

  return true;
}

void InstantService::ResetToDefault() {
  ResetCustomLinks();
  ResetCustomBackgroundThemeInfo();
  pref_service_->SetBoolean(prefs::kNtpUseMostVisitedTiles, false);
  pref_service_->SetBoolean(prefs::kNtpShortcutsVisible, true);
}

void InstantService::UpdateCustomBackgroundColorAsync(
    const GURL& image_url,
    const gfx::Image& fetched_image,
    const image_fetcher::RequestMetadata& metadata) {
  // Calculate the bitmap color asynchronously as it is slow (1-2 seconds for
  // the thumbnail). However, prefs should be updated on the main thread.
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&GetBitmapMainColor, *fetched_image.ToSkBitmap()),
      base::BindOnce(&InstantService::UpdateCustomBackgroundPrefsWithColor,
                     weak_ptr_factory_.GetWeakPtr(), image_url));
}

void InstantService::FetchCustomBackground(const GURL& image_url,
                                           const GURL& fetch_url) {
  DCHECK(!fetch_url.is_empty());

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("ntp_custom_background",
                                          R"(
    semantics {
      sender: "Desktop Chrome background fetcher"
      description:
        "Fetch New Tab Page custom background for color calculation."
      trigger:
        "User selects new background on the New Tab Page."
      data: "The only data sent is the path to an image"
      destination: GOOGLE_OWNED_SERVICE
    }
    policy {
      cookies_allowed: NO
      setting:
        "Users cannot disable this feature. The feature is enabled by "
        "default."
      policy_exception_justification: "Not implemented."
    })");

  image_fetcher::ImageFetcherParams params(traffic_annotation,
                                           kCustomBackgroundsUmaClientName);
  image_fetcher_->FetchImage(
      image_url,
      base::BindOnce(&InstantService::UpdateCustomBackgroundColorAsync,
                     weak_ptr_factory_.GetWeakPtr(), image_url),
      std::move(params));
}

bool InstantService::IsCustomBackgroundPrefValid(GURL& custom_background_url) {
  const base::DictionaryValue* background_info =
      profile_->GetPrefs()->GetDictionary(prefs::kNtpCustomBackgroundDict);
  if (!background_info)
    return false;

  const base::Value* background_url =
      background_info->FindKey(kNtpCustomBackgroundURL);
  if (!background_url)
    return false;

  custom_background_url = GURL(background_url->GetString());
  return custom_background_url.is_valid();
}

void InstantService::RemoveLocalBackgroundImageCopy() {
  base::FilePath path = profile_->GetPath().AppendASCII(
      chrome::kChromeSearchLocalNtpBackgroundFilename);
  base::PostTaskWithTraits(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(IgnoreResult(&base::DeleteFile), path, false));
}

void InstantService::DeleteThumbnailDataIfExists(
    const base::FilePath& profile_path,
    base::Optional<base::OnceCallback<void(bool)>> callback) {
  base::FilePath database_dir(
      profile_path.Append(FILE_PATH_LITERAL("Thumbnails")));
  base::PostTaskWithTraits(FROM_HERE,
                           {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
                           base::BindOnce(&DoDeleteThumbnailDataIfExists,
                                          database_dir, std::move(callback)));
}

void InstantService::AddValidBackdropUrlForTesting(const GURL& url) const {
  background_service_->AddValidBackdropUrlForTesting(url);
}

void InstantService::CreateDarkModeObserver(ui::NativeTheme* theme) {
  dark_mode_observer_ = std::make_unique<ui::DarkModeObserver>(
      theme, base::BindRepeating(&InstantService::OnDarkModeChanged,
                                 weak_ptr_factory_.GetWeakPtr()));
  dark_mode_observer_->Start();
}

// static
void InstantService::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(
      prefs::kNtpCustomBackgroundDict, NtpCustomBackgroundDefaults(),
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kNtpCustomBackgroundLocalToDevice,
                                false);
  registry->RegisterBooleanPref(prefs::kNtpUseMostVisitedTiles, false);
  registry->RegisterBooleanPref(prefs::kNtpShortcutsVisible, true);
}

void InstantService::UpdateCustomBackgroundPrefsWithColor(const GURL& image_url,
                                                          SkColor color) {
  // Update background color only if the selected background is still the same.
  const base::DictionaryValue* background_info =
      pref_service_->GetDictionary(prefs::kNtpCustomBackgroundDict);
  if (!background_info)
    return;

  GURL current_bg_url(
      background_info->FindKey(kNtpCustomBackgroundURL)->GetString());
  if (current_bg_url == image_url) {
    pref_service_->Set(prefs::kNtpCustomBackgroundDict,
                       GetBackgroundInfoWithColor(background_info, color));
  }
}

void InstantService::SetImageFetcherForTesting(
    image_fetcher::ImageFetcher* image_fetcher) {
  image_fetcher_ = base::WrapUnique(image_fetcher);
}
