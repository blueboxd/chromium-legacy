// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/location_bar_model_impl.h"

#include "base/check.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/dom_distiller/core/url_utils.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/buildflags.h"
#include "components/omnibox/browser/location_bar_model_delegate.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url_service.h"
#include "components/security_state/core/security_state.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/common/origin_util.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/vector_icon_types.h"

#if (!defined(OS_ANDROID) || BUILDFLAG(ENABLE_VR)) && !defined(OS_IOS)
#include "components/omnibox/browser/vector_icons.h"  // nogncheck
#include "components/vector_icons/vector_icons.h"     // nogncheck
#endif

using metrics::OmniboxEventProto;

LocationBarModelImpl::LocationBarModelImpl(LocationBarModelDelegate* delegate,
                                           size_t max_url_display_chars)
    : delegate_(delegate), max_url_display_chars_(max_url_display_chars) {
  DCHECK(delegate_);
}

LocationBarModelImpl::~LocationBarModelImpl() {}

// LocationBarModelImpl Implementation.
base::string16 LocationBarModelImpl::GetFormattedFullURL() const {
  return GetFormattedURL(url_formatter::kFormatUrlOmitDefaults);
}

base::string16 LocationBarModelImpl::GetURLForDisplay() const {
  url_formatter::FormatUrlTypes format_types =
      url_formatter::kFormatUrlOmitDefaults;
  if (delegate_->ShouldTrimDisplayUrlAfterHostName()) {
    format_types |= url_formatter::kFormatUrlTrimAfterHost;
  }

#if defined(OS_IOS)
  format_types |= url_formatter::kFormatUrlTrimAfterHost;
#endif

  if (OmniboxFieldTrial::IsHideSteadyStateUrlSchemeEnabled())
    format_types |= url_formatter::kFormatUrlOmitHTTPS;

  if (OmniboxFieldTrial::IsHideSteadyStateUrlTrivialSubdomainsEnabled())
    format_types |= url_formatter::kFormatUrlOmitTrivialSubdomains;

  if (base::FeatureList::IsEnabled(omnibox::kHideFileUrlScheme))
    format_types |= url_formatter::kFormatUrlOmitFileScheme;

  if (dom_distiller::url_utils::IsDistilledPage(GetURL())) {
    // We explicitly elide the scheme here to ensure that HTTPS and HTTP will
    // be removed for display: Reader mode pages should not display a scheme,
    // and should only run on HTTP/HTTPS pages.
    // Users will be able to see the scheme when the URL is focused or being
    // edited in the omnibox.
    format_types |= url_formatter::kFormatUrlOmitHTTP;
    format_types |= url_formatter::kFormatUrlOmitHTTPS;
  }

  return GetFormattedURL(format_types);
}

base::string16 LocationBarModelImpl::GetFormattedURL(
    url_formatter::FormatUrlTypes format_types) const {
  if (!ShouldDisplayURL())
    return base::string16{};

  // Reset |format_types| to prevent elision of URLs when relevant extension or
  // pref is enabled.
  if (delegate_->ShouldPreventElision()) {
    format_types = url_formatter::kFormatUrlOmitDefaults &
                   ~url_formatter::kFormatUrlOmitHTTP;
  }

  GURL url(GetURL());
  // Special handling for dom-distiller:. Instead of showing internal reader
  // mode URLs, show the original article URL in the omnibox.
  // Note that this does not disallow the user from seeing the distilled page
  // URL in the view-source url or devtools. Note that this also impacts
  // GetFormattedFullURL which uses GetFormattedURL as a helper.
  // Virtual URLs were not a good solution for Reader Mode URLs because some
  // security UI is based off of the virtual URL rather than the original URL,
  // and Reader Mode has its own security chip. In addition virtual URLs would
  // add a lot of complexity around passing necessary URL parameters to the
  // Reader Mode pages.
  // Note: if the URL begins with dom-distiller:// but is invalid we display it
  // as-is because it cannot be transformed into an article URL.
  if (dom_distiller::url_utils::IsDistilledPage(url))
    url = dom_distiller::url_utils::GetOriginalUrlFromDistillerUrl(url);

  // Note that we can't unescape spaces here, because if the user copies this
  // and pastes it into another program, that program may think the URL ends at
  // the space.
  const base::string16 formatted_text =
      delegate_->FormattedStringWithEquivalentMeaning(
          url,
          url_formatter::FormatUrl(url, format_types, net::UnescapeRule::NORMAL,
                                   nullptr, nullptr, nullptr));

  // Truncating the URL breaks editing and then pressing enter, but hopefully
  // people won't try to do much with such enormous URLs anyway. If this becomes
  // a real problem, we could perhaps try to keep some sort of different "elided
  // visible URL" where editing affects and reloads the "real underlying URL",
  // but this seems very tricky for little gain.
  return gfx::TruncateString(formatted_text, max_url_display_chars_,
                             gfx::CHARACTER_BREAK);
}

GURL LocationBarModelImpl::GetURL() const {
  GURL url;
  return (ShouldDisplayURL() && delegate_->GetURL(&url))
             ? url
             : GURL(url::kAboutBlankURL);
}

security_state::SecurityLevel LocationBarModelImpl::GetSecurityLevel() const {
  // When empty, assume no security style.
  if (!ShouldDisplayURL())
    return security_state::NONE;

  return delegate_->GetSecurityLevel();
}

bool LocationBarModelImpl::GetDisplaySearchTerms(base::string16* search_terms) {
  if (!base::FeatureList::IsEnabled(omnibox::kQueryInOmnibox) ||
      delegate_->ShouldPreventElision())
    return false;

  // Only show the search terms if the site is secure. However, make an
  // exception before the security state is initialized to prevent a UI flicker.
  std::unique_ptr<security_state::VisibleSecurityState> visible_security_state =
      delegate_->GetVisibleSecurityState();
  security_state::SecurityLevel security_level = delegate_->GetSecurityLevel();
  if (visible_security_state->connection_info_initialized &&
      security_level != security_state::SecurityLevel::SECURE &&
      security_level != security_state::SecurityLevel::EV_SECURE) {
    return false;
  }

  base::string16 extracted_search_terms = ExtractSearchTermsInternal(GetURL());
  if (extracted_search_terms.empty())
    return false;

  if (search_terms)
    *search_terms = extracted_search_terms;

  return true;
}

OmniboxEventProto::PageClassification
LocationBarModelImpl::GetPageClassification(OmniboxFocusSource focus_source) {
  // We may be unable to fetch the current URL during startup or shutdown when
  // the omnibox exists but there is no attached page.
  GURL gurl;
  if (!delegate_->GetURL(&gurl))
    return OmniboxEventProto::OTHER;

  if (focus_source == OmniboxFocusSource::SEARCH_BUTTON)
    return OmniboxEventProto::SEARCH_BUTTON_AS_STARTING_FOCUS;
  if (delegate_->IsInstantNTP()) {
    // Note that we treat OMNIBOX as the source if focus_source_ is INVALID,
    // i.e., if input isn't actually in progress.
    return (focus_source == OmniboxFocusSource::FAKEBOX)
               ? OmniboxEventProto::INSTANT_NTP_WITH_FAKEBOX_AS_STARTING_FOCUS
               : OmniboxEventProto::INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS;
  }
  if (!gurl.is_valid())
    return OmniboxEventProto::INVALID_SPEC;
  if (delegate_->IsNewTabPage(gurl))
    return OmniboxEventProto::NTP;
  if (gurl.spec() == url::kAboutBlankURL)
    return OmniboxEventProto::BLANK;
  if (delegate_->IsHomePage(gurl))
    return OmniboxEventProto::HOME_PAGE;

  TemplateURLService* template_url_service = delegate_->GetTemplateURLService();
  if (template_url_service &&
      template_url_service->IsSearchResultsPageFromDefaultSearchProvider(
          gurl)) {
    return GetDisplaySearchTerms(nullptr)
               ? OmniboxEventProto::
                     SEARCH_RESULT_PAGE_DOING_SEARCH_TERM_REPLACEMENT
               : OmniboxEventProto::
                     SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT;
  }

  return OmniboxEventProto::OTHER;
}

const gfx::VectorIcon& LocationBarModelImpl::GetVectorIcon() const {
#if (!defined(OS_ANDROID) || BUILDFLAG(ENABLE_VR)) && !defined(OS_IOS)
  auto* const icon_override = delegate_->GetVectorIconOverride();
  if (icon_override)
    return *icon_override;

  if (IsOfflinePage())
    return omnibox::kOfflinePinIcon;

  security_state::SecurityLevel security_level = GetSecurityLevel();
  switch (security_level) {
    case security_state::NONE:
      return omnibox::kHttpIcon;
    case security_state::WARNING:
      // When kMarkHttpAsParameterDangerWarning is enabled, show a danger
      // triangle icon.
      if (security_state::ShouldShowDangerTriangleForWarningLevel()) {
        return omnibox::kNotSecureWarningIcon;
      }
      return omnibox::kHttpIcon;
    case security_state::EV_SECURE:
    case security_state::SECURE:
      return omnibox::kHttpsValidIcon;
    case security_state::SECURE_WITH_POLICY_INSTALLED_CERT:
      return vector_icons::kBusinessIcon;
    case security_state::DANGEROUS:
      return omnibox::kNotSecureWarningIcon;
    case security_state::SECURITY_LEVEL_COUNT:
      NOTREACHED();
      return omnibox::kHttpIcon;
  }
  NOTREACHED();
  return omnibox::kHttpIcon;
#else
  NOTREACHED();
  static const gfx::VectorIcon dummy = {};
  return dummy;
#endif
}

base::string16 LocationBarModelImpl::GetSecureDisplayText() const {
  // Note that display text will be implicitly used as the accessibility text.
  // GetSecureAccessibilityText() handles special cases when no display text is set.

  if (IsOfflinePage())
    return l10n_util::GetStringUTF16(IDS_OFFLINE_VERBOSE_STATE);

  switch (GetSecurityLevel()) {
    case security_state::WARNING:
      return l10n_util::GetStringUTF16(IDS_NOT_SECURE_VERBOSE_STATE);
    case security_state::EV_SECURE:
    case security_state::SECURE:
      return base::string16();
    case security_state::DANGEROUS: {
      std::unique_ptr<security_state::VisibleSecurityState>
          visible_security_state = delegate_->GetVisibleSecurityState();

      // Don't show any text in the security indicator for sites on the billing
      // interstitial list.
      if (visible_security_state->malicious_content_status ==
          security_state::MALICIOUS_CONTENT_STATUS_BILLING) {
#if defined(OS_IOS)
        // On iOS, we never expect this status, because there are no billing
        // interstitials.
        NOTREACHED();
#endif
        return base::string16();
      }

      bool fails_malware_check =
          visible_security_state->malicious_content_status !=
          security_state::MALICIOUS_CONTENT_STATUS_NONE;
      return l10n_util::GetStringUTF16(fails_malware_check
                                           ? IDS_DANGEROUS_VERBOSE_STATE
                                           : IDS_NOT_SECURE_VERBOSE_STATE);
    }
    default:
      return base::string16();
  }
}

base::string16 LocationBarModelImpl::GetSecureAccessibilityText() const {
  auto display_text = GetSecureDisplayText();
  if (!display_text.empty())
    return display_text;

  switch (GetSecurityLevel()) {
    case security_state::EV_SECURE:
    case security_state::SECURE:
      return l10n_util::GetStringUTF16(IDS_SECURE_VERBOSE_STATE);
    default:
      return base::string16();
  }
}

bool LocationBarModelImpl::ShouldDisplayURL() const {
  return delegate_->ShouldDisplayURL();
}

bool LocationBarModelImpl::IsOfflinePage() const {
  return delegate_->IsOfflinePage();
}

base::string16 LocationBarModelImpl::ExtractSearchTermsInternal(
    const GURL& url) {
  AutocompleteClassifier* autocomplete_classifier =
      delegate_->GetAutocompleteClassifier();
  TemplateURLService* template_url_service = delegate_->GetTemplateURLService();
  if (!autocomplete_classifier || !template_url_service)
    return base::string16();

  if (url.is_empty())
    return base::string16();

  // Because we cache keyed by URL, if the user changes the default search
  // provider, we will continue to extract the search terms from the cached URL
  // (even if it's no longer from the default search provider) until the user
  // changes tabs or navigates the tab. That is intentional, as it would be
  // weird otherwise if the omnibox text changed without any user gesture.
  if (url != cached_url_) {
    cached_url_ = url;
    cached_search_terms_.clear();

    const TemplateURL* default_provider =
        template_url_service->GetDefaultSearchProvider();
    if (default_provider) {
      // If |url| doesn't match the default search provider,
      // |cached_search_terms_| will remain empty.
      default_provider->ExtractSearchTermsFromURL(
          url, template_url_service->search_terms_data(),
          &cached_search_terms_);

      // Clear out the search terms if it looks like a URL.
      AutocompleteMatch match;
      autocomplete_classifier->Classify(
          cached_search_terms_, false, false,
          metrics::OmniboxEventProto::INVALID_SPEC, &match, nullptr);
      if (!AutocompleteMatch::IsSearchType(match.type))
        cached_search_terms_.clear();
    }
  }

  return cached_search_terms_;
}
