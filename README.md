Tree-Sitter Lexer for Raku
==========================

**This is a work in progress. The actual implementation, particularly
the scanner, is due to change drastically as it is realized.**

This is an *investigatory* implementation of a Raku grammar sufficient
for syntax highlighting and indentation in most editors, particularly
Emacs.

The goals of this implementation are specifically to support

 * *Complete* and *correct* heredoc support
 * *Complete* and *correct* POD6 support
 * Use of arbitrary balanced braces where permitted
 * Proper interpolation construct support in strings, Q-constructs,
   and heredocs
 * Proper support for multi-line comments

### Future Possibilities

Items that would be useful to implement in the long term, if it can be
proven that tree-sitter - aided by an external scanner - can
effectively parse Raku code, are as follows

 * SLang-awareness, not as far as actually handling arbitrary
   grammars, but as much as being able to provide SLang details in the
   parse tree and skip the remainder of a SLangified block
