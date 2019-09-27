// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/password_form_conversion_utils.h"

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/html_based_username_detector.h"
#include "components/autofill/core/common/password_form.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_form_control_element.h"
#include "third_party/blink/public/web/web_input_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/gurl.h"

using blink::WebElement;
using blink::WebFormControlElement;
using blink::WebFormElement;
using blink::WebInputElement;
using blink::WebLocalFrame;
using blink::WebString;

namespace autofill {

namespace {

constexpr char kAutocompleteUsername[] = "username";
constexpr char kAutocompleteCurrentPassword[] = "current-password";
constexpr char kAutocompleteNewPassword[] = "new-password";
constexpr char kAutocompleteCreditCardPrefix[] = "cc-";

// Parses the string with the value of an autocomplete attribute. If any of the
// tokens "username", "current-password" or "new-password" are present, returns
// an appropriate enum value, picking an arbitrary one if more are applicable.
// Otherwise, it returns CREDIT_CARD if a token with a "cc-" prefix is found.
// Otherwise, returns NONE.
AutocompleteFlag ExtractAutocompleteFlag(const std::string& attribute) {
  std::vector<base::StringPiece> tokens =
      base::SplitStringPiece(attribute, base::kWhitespaceASCII,
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  bool cc_seen = false;
  for (base::StringPiece token : tokens) {
    if (base::LowerCaseEqualsASCII(token, kAutocompleteUsername))
      return AutocompleteFlag::USERNAME;
    if (base::LowerCaseEqualsASCII(token, kAutocompleteCurrentPassword))
      return AutocompleteFlag::CURRENT_PASSWORD;
    if (base::LowerCaseEqualsASCII(token, kAutocompleteNewPassword))
      return AutocompleteFlag::NEW_PASSWORD;

    if (!cc_seen) {
      cc_seen = base::StartsWith(token, kAutocompleteCreditCardPrefix,
                                 base::CompareCase::SENSITIVE);
    }
  }
  return cc_seen ? AutocompleteFlag::CREDIT_CARD : AutocompleteFlag::NONE;
}

// Helper to spare map::find boilerplate when caching field's autocomplete
// attributes.
class AutocompleteCache {
 public:
  AutocompleteCache();

  ~AutocompleteCache();

  // Computes and stores the AutocompleteFlag for |field| based on its
  // autocomplete attribute. Note that this cannot be done on-demand during
  // RetrieveFor, because the cache spares space and look-up time by not storing
  // AutocompleteFlag::NONE values, hence for all elements without an
  // autocomplete attribute, every retrieval would result in a new computation.
  void Store(const FormFieldData* field);

  // Retrieves the value previously stored for |field|.
  AutocompleteFlag RetrieveFor(const FormFieldData* field) const;

 private:
  std::map<const FormFieldData*, AutocompleteFlag> cache_;

  DISALLOW_COPY_AND_ASSIGN(AutocompleteCache);
};

AutocompleteCache::AutocompleteCache() = default;

AutocompleteCache::~AutocompleteCache() = default;

void AutocompleteCache::Store(const FormFieldData* field) {
  const AutocompleteFlag flag =
      ExtractAutocompleteFlag(field->autocomplete_attribute);
  // Only store non-trivial flags. Most of the elements will have the NONE
  // value, so spare storage and lookup time by assuming anything not stored in
  // |cache_| has the NONE flag.
  if (flag != AutocompleteFlag::NONE)
    cache_[field] = flag;
}

AutocompleteFlag AutocompleteCache::RetrieveFor(
    const FormFieldData* field) const {
  auto it = cache_.find(field);
  if (it == cache_.end())
    return AutocompleteFlag::NONE;
  return it->second;
}

const char kPasswordSiteUrlRegex[] =
    "passwords(?:-[a-z-]+\\.corp)?\\.google\\.com";

struct PasswordSiteUrlLazyInstanceTraits
    : public base::internal::DestructorAtExitLazyInstanceTraits<re2::RE2> {
  static re2::RE2* New(void* instance) {
    return CreateMatcher(instance, kPasswordSiteUrlRegex);
  }
};

base::LazyInstance<re2::RE2, PasswordSiteUrlLazyInstanceTraits>
    g_password_site_matcher = LAZY_INSTANCE_INITIALIZER;

// Extracts the username predictions. |control_elements| should be all the DOM
// elements of the form, |form_data| should be the already extracted FormData
// representation of that form. |username_detector_cache| is optional, and can
// be used to spare recomputation if called multiple times for the same form.
std::vector<uint32_t> GetUsernamePredictions(
    const std::vector<WebFormControlElement>& control_elements,
    const FormData& form_data,
    UsernameDetectorCache* username_detector_cache) {
  std::vector<uint32_t> username_predictions;
  // Dummy cache stores the predictions in case no real cache was passed to
  // here.
  UsernameDetectorCache dummy_cache;
  if (!username_detector_cache)
    username_detector_cache = &dummy_cache;

  return GetPredictionsFieldBasedOnHtmlAttributes(control_elements, form_data,
                                                  username_detector_cache);
}

bool HasGaiaSchemeAndHost(const WebFormElement& form) {
  GURL form_url = form.GetDocument().Url();
  GURL gaia_url = GaiaUrls::GetInstance()->gaia_url();
  return form_url.scheme() == gaia_url.scheme() &&
         form_url.host() == gaia_url.host();
}

}  // namespace

AutocompleteFlag AutocompleteFlagForElement(const WebInputElement& element) {
  static const base::NoDestructor<WebString> kAutocomplete(("autocomplete"));
  return ExtractAutocompleteFlag(
      element.GetAttribute(*kAutocomplete)
          .Utf8(WebString::UTF8ConversionMode::kStrictReplacingErrorsWithFFFD));
}

re2::RE2* CreateMatcher(void* instance, const char* pattern) {
  re2::RE2::Options options;
  options.set_case_sensitive(false);
  // Use placement new to initialize the instance in the preallocated space.
  // The "(instance)" is very important to force POD type initialization.
  re2::RE2* matcher = new (instance) re2::RE2(pattern, options);
  DCHECK(matcher->ok());
  return matcher;
}

bool IsGaiaReauthenticationForm(const blink::WebFormElement& form) {
  if (!HasGaiaSchemeAndHost(form))
    return false;

  bool has_rart_field = false;
  bool has_continue_field = false;

  blink::WebVector<WebFormControlElement> web_control_elements;
  form.GetFormControlElements(web_control_elements);

  for (const WebFormControlElement& element : web_control_elements) {
    // We're only interested in the presence
    // of <input type="hidden" /> elements.
    static base::NoDestructor<WebString> kHidden("hidden");
    const blink::WebInputElement* input = blink::ToWebInputElement(&element);
    if (!input || input->FormControlTypeForAutofill() != *kHidden)
      continue;

    // There must be a hidden input named "rart".
    if (input->FormControlName() == "rart")
      has_rart_field = true;

    // There must be a hidden input named "continue", whose value points
    // to a password (or password testing) site.
    if (input->FormControlName() == "continue" &&
        re2::RE2::PartialMatch(input->Value().Utf8(),
                               g_password_site_matcher.Get())) {
      has_continue_field = true;
    }
  }
  return has_rart_field && has_continue_field;
}

bool IsGaiaWithSkipSavePasswordForm(const blink::WebFormElement& form) {
  if (!HasGaiaSchemeAndHost(form))
    return false;

  GURL url(form.GetDocument().Url());
  std::string should_skip_password;
  if (!net::GetValueForKeyInQuery(url, "ssp", &should_skip_password))
    return false;
  return should_skip_password == "1";
}

std::unique_ptr<PasswordForm> CreateSimplifiedPasswordFormFromWebForm(
    const WebFormElement& web_form,
    const FieldDataManager* field_data_manager,
    UsernameDetectorCache* username_detector_cache) {
  if (web_form.IsNull())
    return nullptr;

  auto password_form = std::make_unique<PasswordForm>();
  password_form->origin =
      form_util::GetCanonicalOriginForDocument(web_form.GetDocument());
  password_form->signon_realm = GetSignOnRealm(password_form->origin);

  password_form->form_data.is_gaia_with_skip_save_password_form =
      IsGaiaWithSkipSavePasswordForm(web_form) ||
      IsGaiaReauthenticationForm(web_form);

  blink::WebVector<WebFormControlElement> control_elements;
  web_form.GetFormControlElements(control_elements);
  if (control_elements.empty())
    return nullptr;

  if (!WebFormElementToFormData(web_form, WebFormControlElement(),
                                field_data_manager, form_util::EXTRACT_VALUE,
                                &password_form->form_data,
                                nullptr /* FormFieldData */)) {
    return nullptr;
  }
  password_form->form_data.username_predictions =
      GetUsernamePredictions(control_elements.ReleaseVector(),
                             password_form->form_data, username_detector_cache);

  return password_form;
}

std::unique_ptr<PasswordForm>
CreateSimplifiedPasswordFormFromUnownedInputElements(
    const WebLocalFrame& frame,
    const FieldDataManager* field_data_manager,
    UsernameDetectorCache* username_detector_cache) {
  std::vector<WebElement> fieldsets;
  std::vector<WebFormControlElement> control_elements =
      form_util::GetUnownedFormFieldElements(frame.GetDocument().All(),
                                             &fieldsets);
  if (control_elements.empty())
    return nullptr;

  auto password_form = std::make_unique<PasswordForm>();
  if (!UnownedPasswordFormElementsAndFieldSetsToFormData(
          fieldsets, control_elements, nullptr, frame.GetDocument(),
          field_data_manager, form_util::EXTRACT_VALUE,
          &password_form->form_data, nullptr /* FormFieldData */)) {
    return nullptr;
  }

  password_form->origin =
      form_util::GetCanonicalOriginForDocument(frame.GetDocument());
  password_form->signon_realm = GetSignOnRealm(password_form->origin);
  password_form->form_data.username_predictions = GetUsernamePredictions(
      control_elements, password_form->form_data, username_detector_cache);
  return password_form;
}

std::string GetSignOnRealm(const GURL& origin) {
  GURL::Replacements rep;
  rep.SetPathStr("");
  return origin.ReplaceComponents(rep).spec();
}

}  // namespace autofill
