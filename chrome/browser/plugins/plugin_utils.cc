// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/plugin_utils.h"

#include "base/values.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/common/plugin_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "content/public/common/webplugininfo.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/info_map.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/mime_types_handler.h"
#endif

namespace {

const char kFlashPluginID[] = "adobe-flash-player";

void GetPluginContentSettingInternal(
    const HostContentSettingsMap* host_content_settings_map,
    bool use_javascript_setting,
    const url::Origin& main_frame_origin,
    const GURL& plugin_url,
    const std::string& resource,
    ContentSetting* setting,
    bool* is_default,
    bool* is_managed) {
  GURL main_frame_url = main_frame_origin.GetURL();
  std::unique_ptr<base::Value> value;
  content_settings::SettingInfo info;
  bool uses_plugin_specific_setting = false;
  if (use_javascript_setting) {
    value = host_content_settings_map->GetWebsiteSetting(
        main_frame_url, main_frame_url, CONTENT_SETTINGS_TYPE_JAVASCRIPT,
        std::string(), &info);
  } else {
    content_settings::SettingInfo specific_info;
    std::unique_ptr<base::Value> specific_setting =
        host_content_settings_map->GetWebsiteSetting(
            main_frame_url, plugin_url, CONTENT_SETTINGS_TYPE_PLUGINS, resource,
            &specific_info);
    content_settings::SettingInfo general_info;
    std::unique_ptr<base::Value> general_setting =
        host_content_settings_map->GetWebsiteSetting(
            main_frame_url, plugin_url, CONTENT_SETTINGS_TYPE_PLUGINS,
            std::string(), &general_info);
    // If there is a plugin-specific setting, we use it, unless the general
    // setting was set by policy, in which case it takes precedence.
    uses_plugin_specific_setting =
        specific_setting &&
        general_info.source != content_settings::SETTING_SOURCE_POLICY;
    if (uses_plugin_specific_setting) {
      value = std::move(specific_setting);
      info = specific_info;
    } else {
      value = std::move(general_setting);
      info = general_info;
    }
  }
  *setting = content_settings::ValueToContentSetting(value.get());

  bool uses_default_content_setting =
      !uses_plugin_specific_setting &&
      info.primary_pattern == ContentSettingsPattern::Wildcard() &&
      info.secondary_pattern == ContentSettingsPattern::Wildcard();

  if (is_default)
    *is_default = uses_default_content_setting;
  if (is_managed)
    *is_managed = info.source == content_settings::SETTING_SOURCE_POLICY;

  // Special behavior for non-JavaScript treated plugins (Flash):
  if (!use_javascript_setting) {
    // ALLOW-by-default is obsolete and should be treated as DETECT.
    if (*setting == CONTENT_SETTING_ALLOW && uses_default_content_setting)
      *setting = CONTENT_SETTING_DETECT_IMPORTANT_CONTENT;

    // Unless the setting is explicitly ALLOW, return BLOCK for any scheme that
    // is not HTTP, HTTPS, FILE, or chrome-extension.
    if (*setting != CONTENT_SETTING_ALLOW &&
        !main_frame_url.SchemeIsHTTPOrHTTPS() &&
        !main_frame_url.SchemeIsFile() &&
        !main_frame_url.SchemeIs(extensions::kExtensionScheme)) {
      *setting = CONTENT_SETTING_BLOCK;
    }
  }

  // For Plugins, ASK is obsolete. Show as DETECT_IMPORTANT_CONTENT to reflect
  // actual behavior.
  if (*setting == ContentSetting::CONTENT_SETTING_ASK)
    *setting = ContentSetting::CONTENT_SETTING_DETECT_IMPORTANT_CONTENT;
}

// Helper function to get the mime type to extension map using data from either
// ProfileIOData or Profile.
#if BUILDFLAG(ENABLE_EXTENSIONS)
base::flat_map<std::string, std::string> GetMimeTypeToExtensionIdMapInternal(
    bool profile_is_off_the_record,
    bool always_open_pdf_externally,
    base::RepeatingCallback<const extensions::Extension*(const std::string&)>
        get_extension,
    base::RepeatingCallback<bool(const std::string&)> is_incognito_enabled) {
  base::flat_map<std::string, std::string> mime_type_to_extension_id_map;
  std::vector<std::string> whitelist = MimeTypesHandler::GetMIMETypeWhitelist();
  // Go through the white-listed extensions and try to use them to intercept
  // the URL request.
  for (const std::string& extension_id : whitelist) {
    const extensions::Extension* extension = get_extension.Run(extension_id);
    // The white-listed extension may not be installed, so we have to NULL check
    // |extension|.
    if (!extension || (profile_is_off_the_record &&
                       !is_incognito_enabled.Run(extension_id))) {
      continue;
    }

    if (extension_id == extension_misc::kPdfExtensionId &&
        always_open_pdf_externally) {
      continue;
    }

    if (MimeTypesHandler* handler = MimeTypesHandler::GetHandler(extension)) {
      for (const auto& supported_mime_type : handler->mime_type_set()) {
        DCHECK(!base::Contains(mime_type_to_extension_id_map,
                               supported_mime_type));
        mime_type_to_extension_id_map[supported_mime_type] = extension_id;
      }
    }
  }
  return mime_type_to_extension_id_map;
}
#endif

}  // namespace

// static
void PluginUtils::GetPluginContentSetting(
    const HostContentSettingsMap* host_content_settings_map,
    const content::WebPluginInfo& plugin,
    const url::Origin& main_frame_origin,
    const GURL& plugin_url,
    const std::string& resource,
    ContentSetting* setting,
    bool* uses_default_content_setting,
    bool* is_managed) {
  GetPluginContentSettingInternal(
      host_content_settings_map, ShouldUseJavaScriptSettingForPlugin(plugin),
      main_frame_origin, plugin_url, resource, setting,
      uses_default_content_setting, is_managed);
}

// static
ContentSetting PluginUtils::GetFlashPluginContentSetting(
    const HostContentSettingsMap* host_content_settings_map,
    const url::Origin& main_frame_origin,
    const GURL& plugin_url,
    bool* is_managed) {
  ContentSetting plugin_setting = CONTENT_SETTING_DEFAULT;
  GetPluginContentSettingInternal(host_content_settings_map,
                                  false /* use_javascript_setting */,
                                  main_frame_origin, plugin_url, kFlashPluginID,
                                  &plugin_setting, nullptr, is_managed);
  return plugin_setting;
}

// static
ContentSetting PluginUtils::UnsafeGetRawDefaultFlashContentSetting(
    const HostContentSettingsMap* host_content_settings_map,
    bool* is_managed) {
  std::string provider_id;
  ContentSetting plugin_setting =
      host_content_settings_map->GetDefaultContentSetting(
          CONTENT_SETTINGS_TYPE_PLUGINS, &provider_id);

  if (is_managed) {
    *is_managed = HostContentSettingsMap::GetProviderTypeFromSource(
                      provider_id) == HostContentSettingsMap::POLICY_PROVIDER;
  }

  return plugin_setting;
}

// static
void PluginUtils::RememberFlashChangedForSite(
    HostContentSettingsMap* host_content_settings_map,
    const GURL& top_level_url) {
  // A |base::DictionaryValue| is set here but for now, clients only check this
  // is a non-nullptr value.
  auto dict = std::make_unique<base::DictionaryValue>();
  constexpr char kFlagKey[] = "flashPreviouslyChanged";
  dict->SetKey(kFlagKey, base::Value(true));
  host_content_settings_map->SetWebsiteSettingDefaultScope(
      top_level_url, top_level_url, CONTENT_SETTINGS_TYPE_PLUGINS_DATA,
      std::string(), std::move(dict));
}

// static
std::string PluginUtils::GetExtensionIdForMimeType(
    content::ResourceContext* resource_context,
    const std::string& mime_type) {
  auto map = GetMimeTypeToExtensionIdMap(resource_context);
  auto it = map.find(mime_type);
  if (it != map.end())
    return it->second;
  return std::string();
}

// static
std::string PluginUtils::GetExtensionIdForMimeType(
    content::BrowserContext* browser_context,
    const std::string& mime_type) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  Profile* profile = Profile::FromBrowserContext(browser_context);
  auto map = GetMimeTypeToExtensionIdMapInternal(
      profile->IsOffTheRecord(),
      profile->GetPrefs()->GetBoolean(prefs::kPluginsAlwaysOpenPdfExternally),
      base::BindRepeating(
          [](content::BrowserContext* context,
             const std::string& extension_id) {
            return extensions::ExtensionRegistry::Get(context)
                ->enabled_extensions()
                .GetByID(extension_id);
          },
          browser_context),
      base::BindRepeating(
          [](content::BrowserContext* context,
             const std::string& extension_id) {
            return extensions::util::IsIncognitoEnabled(extension_id, context);
          },
          browser_context));
  auto it = map.find(mime_type);
  if (it != map.end())
    return it->second;
#endif
  return std::string();
}

base::flat_map<std::string, std::string>
PluginUtils::GetMimeTypeToExtensionIdMap(
    content::ResourceContext* resource_context) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  ProfileIOData* io_data = ProfileIOData::FromResourceContext(resource_context);
  scoped_refptr<extensions::InfoMap> extension_info_map(
      io_data->GetExtensionInfoMap());
  return GetMimeTypeToExtensionIdMapInternal(
      io_data->IsOffTheRecord(),
      io_data->always_open_pdf_externally()->GetValue(),
      base::BindRepeating(
          [](const scoped_refptr<extensions::InfoMap>& info_map,
             const std::string& extension_id) {
            return info_map->extensions().GetByID(extension_id);
          },
          extension_info_map),
      base::BindRepeating(&extensions::InfoMap::IsIncognitoEnabled,
                          extension_info_map));
#else
  return {};
#endif
}
