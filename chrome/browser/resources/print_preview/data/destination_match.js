// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';
  /**
   * Printer types for capabilities and printer list requests.
   * Must match PrinterType in printing/print_job_constants.h
   * @enum {number}
   */
  const PrinterType = {
    PRIVET_PRINTER: 0,
    EXTENSION_PRINTER: 1,
    PDF_PRINTER: 2,
    LOCAL_PRINTER: 3,
    CLOUD_PRINTER: 4
  };

  /**
   * Converts DestinationOrigin to PrinterType.
   * @param {!print_preview.DestinationOrigin} origin The printer's
   *     destination origin.
   * return {!print_preview.PrinterType} The corresponding PrinterType.
   */
  const originToType = function(origin) {
    if (origin === print_preview.DestinationOrigin.LOCAL ||
        origin === print_preview.DestinationOrigin.CROS) {
      return print_preview.PrinterType.LOCAL_PRINTER;
    }
    if (origin === print_preview.DestinationOrigin.PRIVET) {
      return print_preview.PrinterType.PRIVET_PRINTER;
    }
    if (origin === print_preview.DestinationOrigin.EXTENSION) {
      return print_preview.PrinterType.EXTENSION_PRINTER;
    }
    assert(print_preview.CloudOrigins.includes(origin));
    return print_preview.PrinterType.CLOUD_PRINTER;
  };

  /**
   * @param {!print_preview.Destination} destination The destination to figure
   *     out the printer type of.
   * @return {!print_preview.PrinterType} Map the destination to a PrinterType.
   */
  function getPrinterTypeForDestination(destination) {
    if (destination.id ==
        print_preview.Destination.GooglePromotedId.SAVE_AS_PDF) {
      return print_preview.PrinterType.PDF_PRINTER;
    }
    return print_preview.originToType(destination.origin);
  }

  class DestinationMatch {
    /**
     * A set of key parameters describing a destination used to determine
     * if two destinations are the same.
     * @param {!Array<!print_preview.DestinationOrigin>} origins Match
     *     destinations from these origins.
     * @param {RegExp} idRegExp Match destination's id.
     * @param {RegExp} displayNameRegExp Match destination's displayName.
     * @param {boolean} skipVirtualDestinations Whether to ignore virtual
     *     destinations, for example, Save as PDF.
     */
    constructor(origins, idRegExp, displayNameRegExp, skipVirtualDestinations) {
      /** @private {!Array<!print_preview.DestinationOrigin>} */
      this.origins_ = origins;

      /** @private {RegExp} */
      this.idRegExp_ = idRegExp;

      /** @private {RegExp} */
      this.displayNameRegExp_ = displayNameRegExp;

      /** @private {boolean} */
      this.skipVirtualDestinations_ = skipVirtualDestinations;
    }

    /**
     * @param {!print_preview.DestinationOrigin} origin Origin to match.
     * @return {boolean} Whether the origin is one of the {@code origins_}.
     */
    matchOrigin(origin) {
      return this.origins_.includes(origin);
    }

    /**
     * @param {string} id Id of the destination.
     * @param {!print_preview.DestinationOrigin} origin Origin of the
     *     destination.
     * @return {boolean} Whether destination is the same as initial.
     */
    matchIdAndOrigin(id, origin) {
      return this.matchOrigin(origin) && !!this.idRegExp_ &&
          this.idRegExp_.test(id);
    }

    /**
     * @param {!print_preview.Destination} destination Destination to match.
     * @return {boolean} Whether {@code destination} matches the last user
     *     selected one.
     */
    match(destination) {
      if (!this.matchOrigin(destination.origin)) {
        return false;
      }
      if (this.idRegExp_ && !this.idRegExp_.test(destination.id)) {
        return false;
      }
      if (this.displayNameRegExp_ &&
          !this.displayNameRegExp_.test(destination.displayName)) {
        return false;
      }
      if (this.skipVirtualDestinations_ &&
          this.isVirtualDestination_(destination)) {
        return false;
      }
      return true;
    }

    /**
     * @param {!print_preview.Destination} destination Destination to check.
     * @return {boolean} Whether {@code destination} is virtual, in terms of
     *     destination selection.
     * @private
     */
    isVirtualDestination_(destination) {
      if (destination.origin === print_preview.DestinationOrigin.LOCAL) {
        return destination.id ===
            print_preview.Destination.GooglePromotedId.SAVE_AS_PDF;
      }
      return destination.id === print_preview.Destination.GooglePromotedId.DOCS;
    }

    /**
     * @return {!Set<!print_preview.PrinterType>} The printer types that
     *     correspond to this destination match.
     */
    getTypes() {
      return new Set(this.origins_.map(originToType));
    }
  }

  // Export
  return {
    DestinationMatch: DestinationMatch,
    PrinterType: PrinterType,
    getPrinterTypeForDestination: getPrinterTypeForDestination,
    originToType: originToType,
  };
});
