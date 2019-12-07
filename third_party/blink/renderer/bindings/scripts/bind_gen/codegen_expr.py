# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import web_idl


_CODE_GEN_EXPR_PASS_KEY = object()


class CodeGenExpr(object):
    """
    Represents an expression which is composable to produce another expression.

    This is designed primarily to represent conditional expressions and basic
    logical operators (expr_not, expr_and, expr_or) come along with.
    """

    def __init__(self, expr, is_compound=False, pass_key=None):
        assert isinstance(expr, (bool, str))
        assert isinstance(is_compound, bool)
        assert pass_key is _CODE_GEN_EXPR_PASS_KEY

        if isinstance(expr, bool):
            self._text = "true" if expr else "false"
        else:
            self._text = expr
        self._is_compound = is_compound
        self._is_always_false = expr is False
        self._is_always_true = expr is True

    def __eq__(self, other):
        if not isinstance(self, other.__class__):
            return NotImplemented
        # Assume that, as long as the two texts are the same, the two
        # expressions must be the same, i.e. |_is_compound|, etc. must be the
        # same or do not matter.
        return self.to_text() == other.to_text()

    def __ne__(self, other):
        return not (self == other)

    def __hash__(self):
        return hash(self.to_text())

    def __str__(self):
        """
        __str__ is designed to be used when composing another expression.  If
        you'd only like to have a string representation, |to_text| works better.
        """
        if self._is_compound:
            return "({})".format(self.to_text())
        return self.to_text()

    def to_text(self):
        return self._text

    @property
    def is_always_false(self):
        """
        The expression is always False, and code generators have chances of
        optimizations.
        """
        return self._is_always_false

    @property
    def is_always_true(self):
        """
        The expression is always True, and code generators have chances of
        optimizations.
        """
        return self._is_always_true


def _Expr(*args, **kwargs):
    return CodeGenExpr(*args, pass_key=_CODE_GEN_EXPR_PASS_KEY, **kwargs)


def _unary_op(op, term):
    assert isinstance(op, str)
    assert isinstance(term, CodeGenExpr)

    return _Expr("{}{}".format(op, term), is_compound=True)


def _binary_op(op, terms):
    assert isinstance(op, str)
    assert isinstance(terms, (list, tuple))
    assert all(isinstance(term, CodeGenExpr) for term in terms)
    assert all(
        not (term.is_always_false or term.is_always_true) for term in terms)

    return _Expr(op.join(map(str, terms)), is_compound=True)


def expr_not(term):
    assert isinstance(term, CodeGenExpr)

    if term.is_always_false:
        return _Expr(True)
    if term.is_always_true:
        return _Expr(False)
    return _unary_op("!", term)


def expr_and(terms):
    assert isinstance(terms, (list, tuple))
    assert all(isinstance(term, CodeGenExpr) for term in terms)
    assert terms

    if any(term.is_always_false for term in terms):
        return _Expr(False)
    terms = filter(lambda x: not x.is_always_true, terms)
    if not terms:
        return _Expr(True)
    if len(terms) == 1:
        return terms[0]
    return _binary_op(" && ", expr_uniq(terms))


def expr_or(terms):
    assert isinstance(terms, (list, tuple))
    assert all(isinstance(term, CodeGenExpr) for term in terms)
    assert terms

    if any(term.is_always_true for term in terms):
        return _Expr(True)
    terms = filter(lambda x: not x.is_always_false, terms)
    if not terms:
        return _Expr(False)
    if len(terms) == 1:
        return terms[0]
    return _binary_op(" || ", expr_uniq(terms))


def expr_uniq(terms):
    assert isinstance(terms, (list, tuple))
    assert all(isinstance(term, CodeGenExpr) for term in terms)

    uniq_terms = []
    for term in terms:
        if term not in uniq_terms:
            uniq_terms.append(term)
    return uniq_terms


def expr_from_exposure(exposure, in_global=None):
    """
    Args:
        exposure: web_idl.Exposure
        in_global: A global name of [Exposed] that the ExecutionContext is
            supposed to be / represent.
    """
    assert isinstance(exposure, web_idl.Exposure)
    assert in_global is None or isinstance(in_global, str)

    def ref_enabled(feature):
        return _Expr("RuntimeEnabledFeatures::{}Enabled()".format(feature))

    top_terms = [_Expr(True)]

    # [Exposed]
    GLOBAL_NAME_TO_EXECUTION_CONTEXT_TEST = {
        "AnimationWorklet": "IsAnimationWorkletGlobalScope",
        "AudioWorklet": "IsAudioWorkletGlobalScope",
        "DedicatedWorker": "IsDedicatedWorkerGlobalScope",
        "LayoutWorklet": "IsLayoutWorkletGlobalScope",
        "PaintWorklet": "IsPaintWorkletGlobalScope",
        "ServiceWorker": "IsServiceWorkerGlobalScope",
        "SharedWorker": "IsSharedWorkerGlobalScope",
        "Window": "IsDocument",
        "Worker": "IsWorkerGlobalScope",
        "Worklet": "IsWorkletGlobalScope",
    }
    in_globals = set()
    if in_global:
        in_globals.add(in_global)
        for category_name in ("Worker", "Worklet"):
            if in_global.endswith(category_name):
                in_globals.add(category_name)
    exposed_terms = []
    for entry in exposure.global_names_and_features:
        terms = []
        if entry.global_name not in in_globals:
            pred = GLOBAL_NAME_TO_EXECUTION_CONTEXT_TEST[entry.global_name]
            terms.append(_Expr("${{execution_context}}->{}()".format(pred)))
        if entry.feature:
            terms.append(ref_enabled(entry.feature))
        if terms:
            exposed_terms.append(expr_and(terms))
    if exposed_terms:
        top_terms.append(expr_or(exposed_terms))

    # [RuntimeEnabled]
    if exposure.runtime_enabled_features:
        terms = map(ref_enabled, exposure.runtime_enabled_features)
        top_terms.append(expr_or(terms))

    # [SecureContext]
    if exposure.only_in_secure_contexts is True:
        top_terms.append(_Expr("${in_secure_context}"))
    elif exposure.only_in_secure_contexts is False:
        top_terms.append(_Expr(True))
    else:
        terms = map(ref_enabled, exposure.only_in_secure_contexts)
        top_terms.append(
            expr_or([_Expr("${in_secure_context}"),
                     expr_not(expr_and(terms))]))

    return expr_and(top_terms)
