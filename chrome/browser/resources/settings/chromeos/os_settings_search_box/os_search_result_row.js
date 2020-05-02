// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'os-search-result-row' is the container for one search result.
 */
cr.define('settings', function() {
  /**
   * A list of characters that will be ignored during the tokenization and
   * comparison of search result text.
   * TODO(crbug/1072912): Ask linguist for a list of delimiters that make sense.
   * @type {!Array<string>}
   */
  const IGNORED_CHARS = ['-'];

  /**
   * String form of the regexp expressing ignored chars.
   * @type {string}
   */
  const IGNORED_CHARS_REGEX_STR = `[${IGNORED_CHARS.join('')}]`;

  /**
   * Regexp expressing ignored chars.
   * @type {!RegExp}
   */
  const IGNORED_CHARS_REGEX = new RegExp(IGNORED_CHARS_REGEX_STR, 'g');

  /**
   * @param {string} sourceString The string to be modified.
   * @return {string} The sourceString lowercased with accents in the range
   *     \u0300 - \u036f removed.
   */
  function removeAccents(sourceString) {
    return sourceString.toLocaleLowerCase().normalize('NFD').replace(
        /[\u0300-\u036f]/g, '');
  }

  /**
   * Used to convert the query and result into the same format without ignored
   * characters and accents so that easy string comparisons can be performed.
   *     e.g. |sourceString| = 'BRÛLÉE' returns "brulee"
   * @param {string} sourceString The string to be normalized.
   * @return {string} The sourceString lowercased with accents in the range
   *     \u0300 - \u036f removed, and with ignored characters removed.
   */
  function normalizeString(sourceString) {
    return removeAccents(sourceString).replace(IGNORED_CHARS_REGEX, '');
  }

  /**
   * Bolds all strings in |substringsToBold| that occur in |sourceString|,
   * regardless of case.
   *     e.g. |sourceString| = "Turn on Wi-Fi"
   *          |substringsToBold| = ['o', 'wi-f', 'ur']
   *          returns 'T<b>ur</b>n <b>o</b>n <b>Wi-F</b>i'
   * @param {string} sourceString The case sensitive string to be bolded.
   * @param {?Array<string>} substringsToBold The case-insensitive substrings
   *     that will be bolded in the |sourceString|, if they are html substrings
   *     of the |sourceString|.
   * @return {string} An innerHTML string of |sourceString| with any
   *     |substringsToBold| regardless of case bolded.
   */
  function boldSubStrings(sourceString, substringsToBold) {
    if (!substringsToBold || !substringsToBold.length) {
      return sourceString;
    }
    const subStrRegex =
        new RegExp('(\)(' + substringsToBold.join('|') + ')(\)', 'ig');
    return sourceString.replace(subStrRegex, (match) => match.bold());
  }

  Polymer({
    is: 'os-search-result-row',

    behaviors: [I18nBehavior, cr.ui.FocusRowBehavior],

    properties: {
      /** Whether the search result row is selected. */
      selected: {
        type: Boolean,
        reflectToAttribute: true,
        observer: 'makeA11yAnnouncementIfSelectedAndUnfocused_',
      },

      /** Aria label for the row. */
      ariaLabel: {
        type: String,
        computed: 'computeAriaLabel_(searchResult)',
        reflectToAttribute: true,
      },

      /** The query used to fetch this result. */
      searchQuery: String,

      /** @type {!chromeos.settings.mojom.SearchResult} */
      searchResult: Object,

      /** Number of rows in the list this row is part of. */
      listLength: Number,

      /** @private */
      resultText_: {
        type: String,
        computed: 'computeResultText_(searchResult)',
      },
    },

    /** @override */
    attached() {
      // Initialize the announcer once.
      Polymer.IronA11yAnnouncer.requestAvailability();
    },

    /** @private */
    makeA11yAnnouncementIfSelectedAndUnfocused_() {
      if (!this.selected || this.lastFocused) {
        // Do not alert the user if the result is not selected, or
        // the list is focused, defer to aria tags instead.
        return;
      }

      // The selected item is normally not focused when selected, the
      // selected search result should be verbalized as it changes.
      this.fire('iron-announce', {text: this.ariaLabel});
    },

    /**
     * @return {string} The result string.
     * @private
     */
    computeResultText_() {
      // The C++ layer stores the text result as an array of 16 bit char codes,
      // so it must be converted to a JS String.
      return String.fromCharCode.apply(null, this.searchResult.resultText.data);
    },

    /**
     * Bolds individual characters in the result text that are characters in the
     * search query, regardless of order. Some languages represent words with
     * single characters and do not include spaces. In those instances, use
     * exact character matching.
     *     e.g |this.resultText_| = "一二三四"
     *         |this.searchQuery| = "三一"
     *         returns "<b>一</b>二<b>三</b>四"
     * @return {string} An innerHTML string of |this.resultText_| with any
     *     character that is in |this.searchQuery| bolded.
     * @private
     */
    getMatchingIndividualCharsBolded_() {
      return boldSubStrings(
          /*sourceString=*/this.resultText_,
          /*substringsToBold=*/this.searchQuery.split(''));
    },

    /**
     * @param {string} innerHtmlToken A case sensitive segment of the result
     *     text which may or may not contain ignored characters or accents on
     *     characters, and does not contain blank spaces.
     * @param {string} normalizedQuery A lowercased query which does not contain
     *     punctuation.
     * @param {!Array<string>} queryTokens |normalizedQuery| tokenized by
     *     blankspaces of any kind.
     * @return {string} The innerHtmlToken with <b> tags around segments that
     *     match queryTokens, but also includes ignored characters and accents
     *     on characters.
     * @private
     */
    getModifiedInnerHtmlToken_(innerHtmlToken, normalizedQuery, queryTokens) {
      // For comparison purposes with query tokens, lowercase the html token to
      // be displayed and remove ignored characters. The resulting
      // |normalizedToken| will not be the displayed token.
      const normalizedToken = normalizeString(innerHtmlToken);
      if (normalizedQuery.includes(normalizedToken)) {
        // Bold the entire html token to be displayed, if the result is a
        // substring of the query, regardless of blank spaces that may or
        // may not have not been extraneous.
        return normalizedToken ? innerHtmlToken.bold() : innerHtmlToken;
      }

      // Filters out query tokens that are not substrings of the currently
      // processing text token to be displayed.
      const queryTokenFilter = (queryToken) => {
        return !!queryToken && normalizedToken.includes(queryToken);
      };

      // Maps the queryToken to the segment(s) of the html token that contain
      // the queryToken interweaved with any of the ignored characters that were
      // filtered out during normalization. For example, |innerHtmlToken| =
      // 'Wi-Fi-no-blankspsc-WiFi', (i.e. |normalizedToken| =
      // 'WiFinoblankspcWiFi') and |queryTokenLowerCaseNoSpecial| = 'wif', the
      // resulting mapping would be ['Wi-F', 'WiF'].
      const queryTokenToSegment = (queryToken) => {
        const regExpStr =
            queryToken.split('').join(`${IGNORED_CHARS_REGEX_STR}*`);

        // Since |queryToken| does not contain accents and |innerHtmlToken| may
        // have accents matches must be made without accents on characters.
        const innerHtmlTokenNoAccents = removeAccents(innerHtmlToken);
        const matchesNoAccents =
            innerHtmlTokenNoAccents.match(new RegExp(regExpStr, 'g'));

        // Return matches with original accents restored.
        return matchesNoAccents.map(
            match => innerHtmlToken.toLocaleLowerCase().substr(
                innerHtmlTokenNoAccents.indexOf(match), match.length));
      };

      // Contains lowercase segments of the innerHtmlToken that may or may not
      // contain ignored characters and accents on characters.
      const matches =
          queryTokens.filter(queryTokenFilter).map(queryTokenToSegment).flat();

      if (!matches.length) {
        // No matches, return token to displayed as is.
        return innerHtmlToken;
      }

      // Get the length of the longest matched substring(s).
      const maxStrLen =
          matches.reduce((a, b) => a.length > b.length ? a : b).length;

      // Bold the longest substring(s).
      const bolded =
          matches.filter(sourceString => sourceString.length === maxStrLen);
      return boldSubStrings(
          /*sourceString=*/innerHtmlToken, /*substringsToBold=*/bolded);
    },

    /**
     * Tokenize the result and query text, and match the tokens even if they
     * are out of order. Both the result and query text are tokenized by
     * blankspaces, and compared without ignored characters or accents on
     * characters. As each result token is processed, it is compared with every
     * query token. Bold the segment of the result token that is a substring of
     * a query token. e.g. Smaller query block: if "wif on" is queried, a result
     * text of "Turn on Wi-Fi" should have "on" and "Wi-F" bolded. e.g. Larger
     * query block: If "onwifi" is queried, a result text of "Turn on Wi-Fi"
     * should have "Wi-Fi" bolded.
     * @return {string} Result string with <b> tags around query sub string.
     * @private
     */
    getTokenizeMatchedBoldTagged_() {
      // Lowercase and remove ignored characters from the query.
      const normalizedQuery = normalizeString(this.searchQuery);

      // Use blankspace to tokenize the query without ignored characters.
      const queryTokens = normalizedQuery.split(/\s/);

      // Get innerHtmlTokens with bold tags around matching segments.
      const innerHtmlTokensWithBoldTags = this.resultText_.split(/\s/).map(
          innerHtmlToken => this.getModifiedInnerHtmlToken_(
              innerHtmlToken, normalizedQuery, queryTokens));

      // Get all blankspace types.
      const blankspaces = this.resultText_.match(/\s/g);

      if (!blankspaces) {
        // No blankspaces, return |innterHtmlTokensWithBoldTags| as a string.
        return innerHtmlTokensWithBoldTags.join('');
      }

      // Add blankspaces make to where they were located in the string, and
      // form one string to be added to the html.
      // e.g |blankspaces| = [' ', '\xa0']
      //     |innerHtmlTokensWithBoldTags| = ['a', '<b>b</b>', 'c']
      // returns 'a <b>b</b>&nbps;c'
      return innerHtmlTokensWithBoldTags
          .map((token, idx) => {
            return idx !== blankspaces.length ? token + blankspaces[idx] :
                                                token;
          })
          .join('');
    },

    /**
     * @return {string} The result string with <span> tags around keywords.
     * @private
     */
    getResultInnerHtml_() {
      if (this.resultText_.match(/\s/) ||
          this.resultText_.toLocaleLowerCase() !=
              this.resultText_.toLocaleUpperCase()) {
        // If the result text includes blankspaces (as they commonly will in
        // languages like Arabic and Hindi), or if the result text includes
        // at least one character such that the lowercase is different from
        // the uppercase (as they commonly will in languages like English
        // and Russian), tokenize the result text by blankspaces, and bold based
        // off of matching substrings in the tokens.
        return this.getTokenizeMatchedBoldTagged_();
      }

      // If the result text does not contain blankspaces or characters that
      // have upper/lower case differentiation (as they commonly do in languages
      // like Chinese and Japanese), bold exact characters that match.
      return this.getMatchingIndividualCharsBolded_();
    },

    /**
     * @return {string} Aria label string for ChromeVox to verbalize.
     * @private
     */
    computeAriaLabel_() {
      return this.i18n(
          'searchResultSelected', this.focusRowIndex + 1, this.listLength,
          this.computeResultText_());
    },

    /**
     * Only relevant when the focus-row-control is focus()ed. This keypress
     * handler specifies that pressing 'Enter' should cause a route change.
     * TODO(crbug/1056909): Add test for this specific case.
     * @param {!KeyboardEvent} e
     * @private
     */
    onKeyPress_(e) {
      if (e.key === 'Enter') {
        e.stopPropagation();
        this.navigateToSearchResultRoute();
      }
    },

    navigateToSearchResultRoute() {
      assert(this.searchResult.urlPathWithParameters, 'Url path is empty.');

      // |this.searchResult.urlPathWithParameters| separates the path and params
      // by a '?' char.
      const pathAndOptParams =
          this.searchResult.urlPathWithParameters.split('?');

      // There should be at most 2 items in the array (the path and the params).
      assert(pathAndOptParams.length <= 2, 'Path and params format error.');

      const route = assert(
          settings.Router.getInstance().getRouteForPath(
              '/' + pathAndOptParams[0]),
          'Supplied path does not map to an existing route.');
      const params = pathAndOptParams.length == 2 ?
          new URLSearchParams(pathAndOptParams[1]) :
          undefined;

      settings.Router.getInstance().navigateTo(route, params);
      this.fire('navigated-to-result-route');
    },

    /**
     * @return {string} The name of the icon to use.
     * @private
     */
    getResultIcon_() {
      const Icon = chromeos.settings.mojom.SearchResultIcon;
      switch (this.searchResult.icon) {
        case Icon.kA11y:
          return 'os-settings:accessibility';
        case Icon.kAndroid:
          return 'os-settings:android';
        case Icon.kAppsGrid:
          return 'os-settings:apps';
        case Icon.kAssistant:
          return 'os-settings:assistant';
        case Icon.kAvatar:
          return 'cr:person';
        case Icon.kBluetooth:
          return 'cr:bluetooth';
        case Icon.kCellular:
          return 'os-settings:cellular';
        case Icon.kChrome:
          return 'os-settings:chrome';
        case Icon.kClock:
          return 'os-settings:access-time';
        case Icon.kDisplay:
          return 'os-settings:display';
        case Icon.kDrive:
          return 'os-settings:google-drive';
        case Icon.kEthernet:
          return 'os-settings:settings-ethernet';
        case Icon.kFingerprint:
          return 'os-settings:fingerprint';
        case Icon.kFolder:
          return 'os-settings:folder-outline';
        case Icon.kGlobe:
          return 'os-settings:language';
        case Icon.kGooglePlay:
          return 'os-settings:google-play';
        case Icon.kHardDrive:
          return 'os-settings:hard-drive';
        case Icon.kInstantTethering:
          return 'os-settings:magic-tethering';
        case Icon.kKeyboard:
          return 'os-settings:keyboard';
        case Icon.kLaptop:
          return 'os-settings:laptop-chromebook';
        case Icon.kLock:
          return 'os-settings:lock';
        case Icon.kMagnifyingGlass:
          return 'cr:search';
        case Icon.kMessages:
          return 'os-settings:multidevice-messages';
        case Icon.kMouse:
          return 'os-settings:mouse';
        case Icon.kPaintbrush:
          return 'os-settings:paint-brush';
        case Icon.kPenguin:
          return 'os-settings:crostini-mascot';
        case Icon.kPhone:
          return 'os-settings:multidevice-better-together-suite';
        case Icon.kPluginVm:
          return 'os-settings:plugin-vm';
        case Icon.kPower:
          return 'os-settings:power';
        case Icon.kPrinter:
          return 'os-settings:print';
        case Icon.kReset:
          return 'os-settings:restore';
        case Icon.kShield:
          return 'cr:security';
        case Icon.kStylus:
          return 'os-settings:stylus';
        case Icon.kSync:
          return 'os-settings:sync';
        case Icon.kWallpaper:
          return 'os-settings:wallpaper';
        case Icon.kWifi:
          return 'os-settings:network-wifi';
        default:
          return 'os-settings:settings-general';
      }
    },
  });

  // #cr_define_end
  return {};
});
