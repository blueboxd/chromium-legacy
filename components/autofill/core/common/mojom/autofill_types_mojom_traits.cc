// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/mojom/autofill_types_mojom_traits.h"

#include "base/i18n/rtl.h"
#include "mojo/public/cpp/base/string16_mojom_traits.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "ui/gfx/geometry/mojo/geometry_struct_traits.h"
#include "url/mojom/origin_mojom_traits.h"
#include "url/mojom/url_gurl_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<
    autofill::mojom::FormFieldDataDataView,
    autofill::FormFieldData>::Read(autofill::mojom::FormFieldDataDataView data,
                                   autofill::FormFieldData* out) {
  if (!data.ReadLabel(&out->label))
    return false;
  if (!data.ReadName(&out->name))
    return false;
  if (!data.ReadIdAttribute(&out->id_attribute))
    return false;
  if (!data.ReadNameAttribute(&out->name_attribute))
    return false;
  if (!data.ReadValue(&out->value))
    return false;

  if (!data.ReadFormControlType(&out->form_control_type))
    return false;
  if (!data.ReadAutocompleteAttribute(&out->autocomplete_attribute))
    return false;

  if (!data.ReadPlaceholder(&out->placeholder))
    return false;

  if (!data.ReadCssClasses(&out->css_classes))
    return false;

  if (!data.ReadAriaLabel(&out->aria_label))
    return false;

  if (!data.ReadAriaDescription(&out->aria_description))
    return false;

  if (!data.ReadSection(&out->section))
    return false;

  out->properties_mask = data.properties_mask();
  out->unique_renderer_id = data.unique_renderer_id();
  out->form_control_ax_id = data.form_control_ax_id();
  out->max_length = data.max_length();
  out->is_autofilled = data.is_autofilled();

  if (!data.ReadCheckStatus(&out->check_status))
    return false;

  out->is_focusable = data.is_focusable();
  out->should_autocomplete = data.should_autocomplete();

  if (!data.ReadRole(&out->role))
    return false;

  if (!data.ReadTextDirection(&out->text_direction))
    return false;

  out->is_enabled = data.is_enabled();
  out->is_readonly = data.is_readonly();
  if (!data.ReadTypedValue(&out->typed_value))
    return false;

  if (!data.ReadOptionValues(&out->option_values))
    return false;
  if (!data.ReadOptionContents(&out->option_contents))
    return false;

  if (!data.ReadLabelSource(&out->label_source))
    return false;

  return true;
}

// static
bool StructTraits<autofill::mojom::ButtonTitleInfoDataView,
                  autofill::ButtonTitleInfo>::
    Read(autofill::mojom::ButtonTitleInfoDataView data,
         autofill::ButtonTitleInfo* out) {
  return data.ReadTitle(&out->first) && data.ReadType(&out->second);
}

// static
bool StructTraits<autofill::mojom::FormDataDataView, autofill::FormData>::Read(
    autofill::mojom::FormDataDataView data,
    autofill::FormData* out) {
  if (!data.ReadIdAttribute(&out->id_attribute))
    return false;
  if (!data.ReadNameAttribute(&out->name_attribute))
    return false;
  if (!data.ReadName(&out->name))
    return false;
  if (!data.ReadButtonTitles(&out->button_titles))
    return false;
  if (!data.ReadUrl(&out->url))
    return false;
  if (!data.ReadAction(&out->action))
    return false;
  if (!data.ReadMainFrameOrigin(&out->main_frame_origin))
    return false;

  out->is_form_tag = data.is_form_tag();
  out->is_formless_checkout = data.is_formless_checkout();
  out->unique_renderer_id = data.unique_renderer_id();

  if (!data.ReadSubmissionEvent(&out->submission_event))
    return false;

  if (!data.ReadFields(&out->fields))
    return false;

  if (!data.ReadUsernamePredictions(&out->username_predictions))
    return false;

  out->is_gaia_with_skip_save_password_form =
      data.is_gaia_with_skip_save_password_form();

  return true;
}

// static
bool StructTraits<autofill::mojom::FormFieldDataPredictionsDataView,
                  autofill::FormFieldDataPredictions>::
    Read(autofill::mojom::FormFieldDataPredictionsDataView data,
         autofill::FormFieldDataPredictions* out) {
  if (!data.ReadField(&out->field))
    return false;
  if (!data.ReadSignature(&out->signature))
    return false;
  if (!data.ReadHeuristicType(&out->heuristic_type))
    return false;
  if (!data.ReadServerType(&out->server_type))
    return false;
  if (!data.ReadOverallType(&out->overall_type))
    return false;
  if (!data.ReadParseableName(&out->parseable_name))
    return false;
  if (!data.ReadSection(&out->section))
    return false;

  return true;
}

// static
bool StructTraits<autofill::mojom::FormDataPredictionsDataView,
                  autofill::FormDataPredictions>::
    Read(autofill::mojom::FormDataPredictionsDataView data,
         autofill::FormDataPredictions* out) {
  if (!data.ReadData(&out->data))
    return false;
  if (!data.ReadSignature(&out->signature))
    return false;
  if (!data.ReadFields(&out->fields))
    return false;

  return true;
}

// static
bool StructTraits<autofill::mojom::PasswordAndRealmDataView,
                  autofill::PasswordAndRealm>::
    Read(autofill::mojom::PasswordAndRealmDataView data,
         autofill::PasswordAndRealm* out) {
  if (!data.ReadPassword(&out->password))
    return false;
  if (!data.ReadRealm(&out->realm))
    return false;

  return true;
}

// static
bool StructTraits<autofill::mojom::PasswordFormFillDataDataView,
                  autofill::PasswordFormFillData>::
    Read(autofill::mojom::PasswordFormFillDataDataView data,
         autofill::PasswordFormFillData* out) {
  if (!data.ReadOrigin(&out->origin) || !data.ReadAction(&out->action) ||
      !data.ReadUsernameField(&out->username_field) ||
      !data.ReadPasswordField(&out->password_field) ||
      !data.ReadPreferredRealm(&out->preferred_realm) ||
      !data.ReadAdditionalLogins(&out->additional_logins))
    return false;

  out->form_renderer_id = data.form_renderer_id();
  out->wait_for_username = data.wait_for_username();
  out->has_renderer_ids = data.has_renderer_ids();
  out->username_may_use_prefilled_placeholder =
      data.username_may_use_prefilled_placeholder();

  return true;
}

// static
bool StructTraits<autofill::mojom::PasswordFormGenerationDataDataView,
                  autofill::PasswordFormGenerationData>::
    Read(autofill::mojom::PasswordFormGenerationDataDataView data,
         autofill::PasswordFormGenerationData* out) {
  out->new_password_renderer_id = data.new_password_renderer_id();
  out->confirmation_password_renderer_id =
      data.confirmation_password_renderer_id();
  return true;
}

// static
bool StructTraits<autofill::mojom::PasswordGenerationUIDataDataView,
                  autofill::password_generation::PasswordGenerationUIData>::
    Read(autofill::mojom::PasswordGenerationUIDataDataView data,
         autofill::password_generation::PasswordGenerationUIData* out) {
  if (!data.ReadBounds(&out->bounds))
    return false;

  out->max_length = data.max_length();
  out->generation_element_id = data.generation_element_id();

  if (!data.ReadGenerationElement(&out->generation_element) ||
      !data.ReadTextDirection(&out->text_direction) ||
      !data.ReadPasswordForm(&out->password_form))
    return false;

  return true;
}

// static
bool StructTraits<
    autofill::mojom::PasswordFormDataView,
    autofill::PasswordForm>::Read(autofill::mojom::PasswordFormDataView data,
                                  autofill::PasswordForm* out) {
  if (!data.ReadScheme(&out->scheme) ||
      !data.ReadSignonRealm(&out->signon_realm) ||
      !data.ReadOriginWithPath(&out->origin) ||
      !data.ReadAction(&out->action) ||
      !data.ReadAffiliatedWebRealm(&out->affiliated_web_realm) ||
      !data.ReadSubmitElement(&out->submit_element) ||
      !data.ReadUsernameElement(&out->username_element) ||
      !data.ReadSubmissionEvent(&out->submission_event))
    return false;

  out->username_marked_by_site = data.username_marked_by_site();

  if (!data.ReadUsernameValue(&out->username_value) ||
      !data.ReadAllPossibleUsernames(&out->all_possible_usernames) ||
      !data.ReadAllPossiblePasswords(&out->all_possible_passwords) ||
      !data.ReadPasswordElement(&out->password_element) ||
      !data.ReadPasswordValue(&out->password_value))
    return false;

  out->form_has_autofilled_value = data.form_has_autofilled_value();

  if (!data.ReadNewPasswordElement(&out->new_password_element) ||
      !data.ReadNewPasswordValue(&out->new_password_value))
    return false;

  out->new_password_marked_by_site = data.new_password_marked_by_site();

  if (!data.ReadConfirmationPasswordElement(
          &out->confirmation_password_element))
    return false;

  out->preferred = data.preferred();

  if (!data.ReadDateCreated(&out->date_created) ||
      !data.ReadDateSynced(&out->date_synced))
    return false;

  out->blacklisted_by_user = data.blacklisted_by_user();

  if (!data.ReadType(&out->type))
    return false;

  out->times_used = data.times_used();

  if (!data.ReadFormData(&out->form_data) ||
      !data.ReadGenerationUploadStatus(&out->generation_upload_status) ||
      !data.ReadDisplayName(&out->display_name) ||
      !data.ReadIconUrl(&out->icon_url) ||
      !data.ReadFederationOrigin(&out->federation_origin))
    return false;

  out->skip_zero_click = data.skip_zero_click();

  out->was_parsed_using_autofill_predictions =
      data.was_parsed_using_autofill_predictions();
  out->is_public_suffix_match = data.is_public_suffix_match();
  out->is_affiliation_based_match = data.is_affiliation_based_match();
  out->only_for_fallback = data.only_for_fallback();
  return true;
}

// static
std::vector<autofill::FormFieldData>
StructTraits<autofill::mojom::PasswordFormFieldPredictionMapDataView,
             autofill::PasswordFormFieldPredictionMap>::
    keys(const autofill::PasswordFormFieldPredictionMap& r) {
  std::vector<autofill::FormFieldData> data;
  for (const auto& i : r)
    data.push_back(i.first);
  return data;
}

// static
std::vector<autofill::mojom::PasswordFormFieldPredictionType>
StructTraits<autofill::mojom::PasswordFormFieldPredictionMapDataView,
             autofill::PasswordFormFieldPredictionMap>::
    values(const autofill::PasswordFormFieldPredictionMap& r) {
  std::vector<autofill::mojom::PasswordFormFieldPredictionType> types;
  for (const auto& i : r)
    types.push_back(i.second);
  return types;
}

// static
bool StructTraits<autofill::mojom::PasswordFormFieldPredictionMapDataView,
                  autofill::PasswordFormFieldPredictionMap>::
    Read(autofill::mojom::PasswordFormFieldPredictionMapDataView data,
         autofill::PasswordFormFieldPredictionMap* out) {
  // Combines keys vector and values vector to the map.
  std::vector<autofill::FormFieldData> keys;
  if (!data.ReadKeys(&keys))
    return false;
  std::vector<autofill::mojom::PasswordFormFieldPredictionType> values;
  if (!data.ReadValues(&values))
    return false;
  if (keys.size() != values.size())
    return false;
  out->clear();
  for (size_t i = 0; i < keys.size(); ++i)
    out->insert({keys[i], values[i]});

  return true;
}

// static
std::vector<autofill::FormData> StructTraits<
    autofill::mojom::FormsPredictionsMapDataView,
    autofill::FormsPredictionsMap>::keys(const autofill::FormsPredictionsMap&
                                             r) {
  std::vector<autofill::FormData> data;
  for (const auto& i : r)
    data.push_back(i.first);
  return data;
}

// static
std::vector<autofill::PasswordFormFieldPredictionMap> StructTraits<
    autofill::mojom::FormsPredictionsMapDataView,
    autofill::FormsPredictionsMap>::values(const autofill::FormsPredictionsMap&
                                               r) {
  std::vector<autofill::PasswordFormFieldPredictionMap> maps;
  for (const auto& i : r)
    maps.push_back(i.second);
  return maps;
}

// static
bool StructTraits<autofill::mojom::FormsPredictionsMapDataView,
                  autofill::FormsPredictionsMap>::
    Read(autofill::mojom::FormsPredictionsMapDataView data,
         autofill::FormsPredictionsMap* out) {
  // Combines keys vector and values vector to the map.
  std::vector<autofill::FormData> keys;
  if (!data.ReadKeys(&keys))
    return false;
  std::vector<autofill::PasswordFormFieldPredictionMap> values;
  if (!data.ReadValues(&values))
    return false;
  if (keys.size() != values.size())
    return false;
  out->clear();
  for (size_t i = 0; i < keys.size(); ++i)
    out->insert({keys[i], values[i]});

  return true;
}

// static
bool StructTraits<autofill::mojom::ValueElementPairDataView,
                  autofill::ValueElementPair>::
    Read(autofill::mojom::ValueElementPairDataView data,
         autofill::ValueElementPair* out) {
  if (!data.ReadValue(&out->first) || !data.ReadFieldName(&out->second))
    return false;

  return true;
}

}  // namespace mojo
