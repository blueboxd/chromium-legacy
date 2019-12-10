// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/bookmark_update_preprocessing.h"

#include <array>
#include <string>

#include "base/guid.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "components/sync/base/hash_util.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/engine_impl/syncer_proto_util.h"
#include "components/sync/model/entity_data.h"
#include "components/sync/protocol/sync.pb.h"

namespace syncer {

namespace {

// Enumeration of possible values for the positioning schemes used in Sync
// entities. Used in UMA metrics. Do not re-order or delete these entries; they
// are used in a UMA histogram. Please edit SyncPositioningScheme in enums.xml
// if a value is added.
enum class SyncPositioningScheme {
  kUniquePosition = 0,
  kPositionInParent = 1,
  kInsertAfterItemId = 2,
  kMissing = 3,
  kMaxValue = kMissing
};

// Used in metric "Sync.BookmarkGUIDSource2". These values are persisted to
// logs. Entries should not be renumbered and numeric values should never be
// reused.
enum class BookmarkGuidSource {
  // GUID came from specifics.
  kSpecifics = 0,
  // GUID came from originator_client_item_id and is valid.
  kValidOCII = 1,
  // GUID not found in the specifics and originator_client_item_id is invalid,
  // so field left empty.
  kLeftEmpty = 2,
  kMaxValue = kLeftEmpty,
};

inline void LogGuidSource(BookmarkGuidSource source) {
  base::UmaHistogramEnumeration("Sync.BookmarkGUIDSource2", source);
}

}  // namespace

void AdaptUniquePositionForBookmark(const sync_pb::SyncEntity& update_entity,
                                    EntityData* data) {
  DCHECK(data);
  bool has_position_scheme = false;
  SyncPositioningScheme sync_positioning_scheme;
  if (update_entity.has_unique_position()) {
    data->unique_position = update_entity.unique_position();
    has_position_scheme = true;
    sync_positioning_scheme = SyncPositioningScheme::kUniquePosition;
  } else if (update_entity.has_position_in_parent() ||
             update_entity.has_insert_after_item_id()) {
    bool missing_originator_fields = false;
    if (!update_entity.has_originator_cache_guid() ||
        !update_entity.has_originator_client_item_id()) {
      DLOG(ERROR) << "Update is missing requirements for bookmark position.";
      missing_originator_fields = true;
    }

    std::string suffix = missing_originator_fields
                             ? UniquePosition::RandomSuffix()
                             : GenerateSyncableBookmarkHash(
                                   update_entity.originator_cache_guid(),
                                   update_entity.originator_client_item_id());

    if (update_entity.has_position_in_parent()) {
      data->unique_position =
          UniquePosition::FromInt64(update_entity.position_in_parent(), suffix)
              .ToProto();
      has_position_scheme = true;
      sync_positioning_scheme = SyncPositioningScheme::kPositionInParent;
    } else {
      // If update_entity has insert_after_item_id, use 0 index.
      DCHECK(update_entity.has_insert_after_item_id());
      data->unique_position = UniquePosition::FromInt64(0, suffix).ToProto();
      has_position_scheme = true;
      sync_positioning_scheme = SyncPositioningScheme::kInsertAfterItemId;
    }
  } else if (SyncerProtoUtil::ShouldMaintainPosition(update_entity) &&
             !update_entity.deleted()) {
    DLOG(ERROR) << "Missing required position information in update.";
    has_position_scheme = true;
    sync_positioning_scheme = SyncPositioningScheme::kMissing;
  }
  if (has_position_scheme) {
    UMA_HISTOGRAM_ENUMERATION("Sync.Entities.PositioningScheme",
                              sync_positioning_scheme);
  }
}

void AdaptTitleForBookmark(const sync_pb::SyncEntity& update_entity,
                           sync_pb::EntitySpecifics* specifics,
                           bool specifics_were_encrypted) {
  DCHECK(specifics);
  if (specifics_were_encrypted || update_entity.deleted()) {
    // If encrypted, the name field is never populated (unencrypted) for privacy
    // reasons. Encryption was also introduced after moving the name out of
    // SyncEntity so this hack is not needed at all.
    return;
  }
  // Legacy clients populate the name field in the SyncEntity instead of the
  // title field in the BookmarkSpecifics.
  if (!specifics->bookmark().has_title() && !update_entity.name().empty()) {
    specifics->mutable_bookmark()->set_title(update_entity.name());
  }
}

void AdaptGuidForBookmark(const sync_pb::SyncEntity& update_entity,
                          sync_pb::EntitySpecifics* specifics) {
  DCHECK(specifics);
  // Tombstones and permanent entities don't have a GUID.
  if (update_entity.deleted() ||
      !update_entity.server_defined_unique_tag().empty()) {
    return;
  }
  // Legacy clients don't populate the guid field in the BookmarkSpecifics, so
  // we use the originator_client_item_id instead, if it is a valid GUID.
  // Otherwise, we leave the field empty.
  if (specifics->bookmark().has_guid()) {
    LogGuidSource(BookmarkGuidSource::kSpecifics);
  } else if (base::IsValidGUID(update_entity.originator_client_item_id())) {
    specifics->mutable_bookmark()->set_guid(
        update_entity.originator_client_item_id());
    LogGuidSource(BookmarkGuidSource::kValidOCII);
  } else {
    LogGuidSource(BookmarkGuidSource::kLeftEmpty);
  }
}

}  // namespace syncer
