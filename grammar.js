/* Tree-sitter Grammar for Raku -*- mode: web; -*-
 *
 * Copyright (C) 2021 Roman Hargrave <roman@hargrave.info>
 * SPDX-License-Identifier: GPL-3.0
 */

module.exports = grammar({
   name: 'raku',

   externals: $ => [
      $._quote_construct_open,
      $._quote_construct_close,
      $._any_brace_open,
      $._any_brace_close,
      $.multiline_comment,
      $.heredoc_body
   ],

   // these rules/tokens can occur anywhere. this is
   // critical in order to implement heredoc
   extras: $ => [
      $.comment,
      $.heredoc_body
   ],

   // for fun, let's try to mirror the official grammar as
   // best as we can...
   // Caveat: tree-sitter defaults to implicit whitespace
   // between tokens, which is nice. 
   rules: {
      _comp_unit: $ => $.statement_list,

      statement_list: $ => repeat(
         seq(
            $.statement,
            $._eat_terminator
         )
      ),

      // "Meta" syntax

      // Special twigils found in declarator block special comments.
      // https://docs.raku.org/language/pod#Declarator_blocks
      declarator_twigil: $ => choice('|', '='),

      _comment_body: $ => choice(),
      
      // multiline comments need to support N-deep sequences
      // of the same open/close constructs in order to be fully
      // up-to-spec. ex. #`( text ( text ) text )
      _comment_body_multiline: $ => choice(
         $._any_brace_open, $._any_brace_close, $._comment_body
      ),
      
      comment: $ => token(seq(
         '#', optional($.declarator_twigil),
         choice(
            // Multi-line
            seq('`', $._any_brace_open, $._comment_body, $._any_brace_close),
            
            // Single-line
            
         )
      ))
      
      // Specials

      // In rakudo, this also matches VC conflicts; however,
      // this is simply for the purpose of producing errors,
      // and is of no concern to us. Treesitter should skate
      // over invalid syntax for us. We can always add that
      // later.
      _vws: $ => /[\r\n\v]/

      // Horizonatal whitespace
      _unv: $ => choice(
         ' ', /[\t]/
      )
      
      // rakudo/src/Perl6/Grammar.nqp:692 @ a78c9f0
      _unsp: $ => seq(
         '\\',
         choice(
            $._vws,
            $._unv
         )
      ),

      _ws: $ => repeat(choice(
         
      )),
      
      // Actual language stuff

      label: $ => seq(
         $.identifier, ':', optional(/\s+/)
      ),

      statement_control_use: $ => seq(
         'use',
         choice(
            $.version
         )
      ),
      
      _statement_control: $ => choice(
         $.statement_control_use
      ),
      
      statement: $ => choice(
         $.label,
         $._statement_control
         // | EXPR
         // | ?[;]
         // | ?stopper
      ),

      _eat_terminator: $ => choice(
         ';',
         /$/,
         // endsmt <.ws>
         // <?before ) | ] | }>
         // $ (EOL/F)
         // <?stopper>
      ),

      identifier: $ =>
         /[_\D][_-\w\d]+/,

      long_identifier: $ => token(seq(
         optional('::'),
         $.identifier,
         repeat(seq('::', $.identifier))
      ))

      // Lifecycle blocks
      
      _quote_construct_init: $ => choice(
         'Q', 'qq', 'qw', 'qww', 'qqw', 'qqww',
         'qx', 'qqx'
      ),
      
      _quote_construct_adverb: $ => choice(
         
      ),

      // Literals

      _integer_plain: $ =>
         /[\d_]+/,

      _integer_arb_base: $ => seq(
         ':',
         field('base', $._integer_plain)
      )
      
      decimal_number: $ =>
         /?[\d_]+(\.[\d_]+)?/,
      
      _rational_number: $ => seq(
         $._decimal_number,
         'e'
      ),
      
      number: $ => choice(),

      // Version Literal, e.g. v1.a.b.3
      // rakudo/src/Perl6/Grammar.nqp:730 @ a78c9f0

      version: $ => token(seq(
         // we need to deal with the classic issue of permitting
         // 1.2, but not 1. or .1
         // we also want to ensure that va.1 is not valid,
         // but both v1.a and v1a are
         'v', /\d+\w*/,
         // <vnum=
         repeat(
            '.',
            choice(/\w+/, '*')
         )
         // >
         optional('+')
      )),

      // Pairs
      
      _arrow_pair: $ => choice(),

      _bool_pair: $ => seq(
         ':',
         optional(
            field('negation', '!')
         ),
         field('name', $._identifier)
      ),
      
      _colon_pair: $ => choice(),

      pair: $ => choice(
         $._colon_pair,
         $._bool_pair,
      ),
   }
})
