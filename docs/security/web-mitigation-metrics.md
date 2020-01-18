# Web Mitigation Metrics

The web platform offers a number of tools to web developers which enable
mitigation of a few important threats. In order to understand how these are
being used in the wild, and evaluate our success at promulgating them, we
collect some usage statistics; this document outlines those counters and
points to some helpful graphs.

## Content Security Policy

We believe that a carefully-crafted [Content Security Policy][csp] can help
protect web applications from injection attacks that would otherwise lead to
script execution. [Strict CSP][strict-csp] is a reasonable approach, one which
we'd like to encourage.

[csp]: https://w3c.github.io/webappsec-csp/
[strict-csp]: https://csp.withgoogle.com/docs/strict-csp.html

In order to understand CSP's use in the wild, we can look at a few counters that
give some insight into the percentage of Chrome users' page views that use CSP
in a given way:

*   `kContentSecurityPolicy`
    ([graph](https://chromestatus.com/metrics/feature/timeline/popularity/15))
    tracks the overall usage of `Content-Security-Policy` headers. Likewise,
    `kContentSecurityPolicyReportOnly`
    ([graph](https://chromestatus.com/metrics/feature/timeline/popularity/16))
    tracks the report-only variant.

To get a feel for the general quality of policies in the wild, we want to
evaluate how closely developers are hewing to the strictures of Strict CSP.
We've boiled that down to three categories:

*   Does the policy reasonably restrict [`object-src`][object-src]? The only
    "reasonable" restriction, unfortunately, is `object-src 'none'`.
    `kCSPWithReasonableObjectRestrictions` and
    `kCSPROWithReasonableObjectRestrictions` track that directive value in
    enforced and report-only modes respectively.

*   Does the policy reasonably restrict `base-uri` in order to avoid malicious
    redirection of relative URLs? `base-uri 'none'` and `base-uri 'self'` are
    both appropriate, and are tracked via `kCSPWithReasonableBaseRestrictions`
    and `kCSPROWithReasonableBaseRestrictions` in enforced and report-only modes
    respectively.

*   Does the policy avoid using a list of hosts or schemes (which [research has
    shown to be mostly ineffective at mitigating attack][csp-is-dead])?
    `kCSPWithReasonableScriptRestrictions` and
    `kCSPROWithReasonableScriptRestrictions` track the policies whose
    [`script-src`][script-src] directives rely upon cryptographic nonces and/or
    hashes rather than lists of trusted servers, and which also avoid relying
    upon `'unsafe-inline'`.

Policies that sufficiently restrict all of the directives above (`object-src`,
`base-uri`, and `script-src`) are tracked via `kCSPWithReasonableRestrictions`
and `kCSPROWithReasonableRestrictions`. This is the baseline we'd like pages
generally to meet, and a number we hope we can drive up over time.

We're also tracking a higher bar, which includes all the restrictions above,
but also avoids relying upon `'strict-dynamic'`, via
`kCSPWithBetterThanReasonableRestrictions` and
`kCSPROWithBetterThanReasonableRestrictions`.

[object-src]: https://w3c.github.io/webappsec-csp/#directive-object-src
[base-uri]: https://w3c.github.io/webappsec-csp/#directive-base-uri
[script-src]: https://w3c.github.io/webappsec-csp/#directive-script-src
[csp-is-dead]: https://research.google/pubs/pub45542/
