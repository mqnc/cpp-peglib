//
//  peglib.h
//
//  Copyright (c) 2020 Yuji Hirose. All rights reserved.
//  MIT License
//

#pragma once

#include "includes.h"

namespace peg {

#include "scopeexit.h"
#include "unicode.h"
#include "escape.h"
#include "tokentonumber.h"
#include "trie.h"
#include "helpers.h"
#include "semanticvalues.h"
#include "semanticaction.h"
#include "errorhandling.h"
#include "context.h"
#include "operator.h"
#include "sequence.h"
#include "choice.h"
#include "repetition.h"
#include "lookahead.h"
#include "literal.h"
#include "capturing.h"
#include "referencing.h"
#include "advanced.h"
#include "factories.h"
#include "visitor.h"
#include "idvisitors.h"
#include "tokenvisitors.h"
#include "sanitizevisitors.h"
#include "referencevisitors.h"

/*
 * Keywords
 */
static const char *WHITESPACE_DEFINITION_NAME = "%whitespace";
static const char *WORD_DEFINITION_NAME = "%word";
static const char *RECOVER_DEFINITION_NAME = "%recover";

#include "definition.h"
#include "implementation.h"
#include "visitorimplementation.h"

/*-----------------------------------------------------------------------------
 *  PEG parser generator
 *---------------------------------------------------------------------------*/

using Rules = std::unordered_map<std::string, std::shared_ptr<Ope>>;

class ParserGenerator {
#include "parsergeneratorhead.h"
#include "make_grammar.h"
#include "setup_actions.h"
#include "apply_precedence_instruction.h"
#include "perform_core.h"
  Grammar g;
};

#include "ast.h"
#include "parser.h"

} // namespace peg