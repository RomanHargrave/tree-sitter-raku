/* Tree-sitter Grammar for Raku  -*- c-file-style: "bsd"; c-basic-offset: 2; -*-
 *
 * Custom scanner implementation. Covers the following cases that
 * can't be expressed easily in the high-level grammar:
 *
 * MINMUM REQUIRED STANDARD IS C11
 */

#include <tree_sitter/parser.h>
#include <stdint.h>
#include <assert.h>
#include <wchar.h>
#include <string.h>

// this is separate just to avoid having to see it all the time.
#include "brace_table.h"

enum token
{
  QUOTE_CONS_OPEN,
  QUOTE_CONS_CLOSE,
  MULTILINE_COMMENT,
  HEREDOC_BODY,
};

/* The heredoc stack tracks how many heredocs remain to be processed.
 * this is important in order to propertly cases where multiple
 * heredocs are used, and where heredocs contain interpolated code.
 */

struct heredoc_stack
{
  const size_t depth;
  
  /**
   * This contains the sequence of characters that, if placed alone on
   * a line following a heredoc body, signals the end of that heredoc.
   */
  const uint32_t* sentinel;

  // in service of fast serialization
  const size_t sentinel_length;

  union {
    struct {
      /**
       * This determines whether the scanner will treat {...} sequences in
       * the heredoc as code interpolation or text.
       */
      const bool may_interp_closure : 1;

      /**
       * Whether or not the scanner should process $name expressions
       */
      const bool may_interp_scalar  : 1;
      const bool may_interp_array   : 1;
      const bool may_interp_hash    : 1;
      const bool may_interp_fun     : 1;
      const bool may_interp_substr  : 1;
    } flags;

    uint32_t all_flags;
  };
    
  struct heredoc_stack* next;
};

static void heredoc_stack_free(struct heredoc_stack* head)
{
  if (head == NULL)
  {
    return;
  }

  assert(head->next != head);

  heredoc_stack_free(head);

  free(head);
}

static struct heredoc_stack* heredoc_stack_push(struct heredoc_stack* head, const uint32_t* sentinel)
{
  assert(head != NULL);

}

static struct heredoc_stack* heredoc_stack_pop(struct heredoc_stack* head)
{
  assert(head != NULL);
  assert(head->depth > 0 /* head is not root */);
  assert(head->next != NULL);

  struct heredoc_stack* next = head->next;

  // set to null to avoid freeing the rest of the stack
  head->next = NULL;
  heredoc_stack_free(head);

  return next;
}

/* The brace stack helps track special balanced constructs. Contrary
 * to its name, it is not intended for use in lexing common
 * brace-enclosed constructs such as blocks ({ ... }) but rather
 * constructs that may be enclosed by arbitrary brace pairs -
 * particularly (Q)uoting constructs.
 */

struct brace_stack
{
  const size_t depth;
  
  const uint32_t closing_brace;  
  struct brace_stack* next;
};

static void brace_stack_free(struct brace_stack* head)
{
  if (head == NULL)
  {
    return;
  }

  assert(head->next != head);
  
  brace_stack_free(head->next);

  free(head);
}

static struct brace_stack* brace_stack_push(struct brace_stack* head, const uint32_t brace)
{
  assert(head != NULL);
  
  // assume that unless there is a corresponding closing brace, that
  // the closing brace matches the opening brace.
  uint32_t closing_brace = brace;

  if (brace < BACE_TABLE_MAX && BRACE_TABLE[brace] != 0) {
    closing_brace = BRACE_TABLE[brace];
  }

  struct brace_stack* new_head = malloc(sizeof(*new_head));
  new_head->depth = head->depth + 1;
  new_head->close = closing_brace;
  new_head->next  = head;

  return new_head;
}

static struct brace_stack* brace_stack_pop(struct brace_stack* head)
{
  assert(head != NULL);
  assert(head->depth > 0 /* head is not root */);
  assert(head->next != NULL);
  
  struct brace_stack* next = head->next;

  // set next to null to avoid freeing the entire stack
  head->next = NULL;
  brace_stack_free(head);

  return next;
}

/* Main scanner implementation */

struct scanner_state {
  struct brace_stack* current_brace;
  struct heredoc_stack* current_heredoc;
};

struct scanner_state* tree_sitter_raku_external_scanner_create()
{
  struct scanner_state* new_state    = malloc(sizeof(*new_state));
  struct brace_stack* brace_root     = calloc(sizeof(*brace_root));
  struct heredoc_stack* heredoc_root = calloc(sizeof(*heredoc_root));

  // just to be clear, even though calloc will do this for us
  brace_root->depth = 0;
  heredoc_root->depth = 0;
  
  new_state->current_brace = brace_root;
  new_state->current_heredoc = heredoc_root;

  return new_state;
}

void tree_sitter_raku_external_scanner_destroy(struct scanner_state* state)
{
  assert(state != NULL);
  assert(state->current_brace != NULL);
  assert(state->current_heredoc != NULL);

  brace_stack_free(state->current_brace);
  heredoc_stack_free(state->current_heredoc);

  free(state);
}

// a somewhate helpful macro to dry up the serializer
#define BUF_WRITE(ptr, type, what)               \
  *((type *) ptr) = what;                        \
  (uintptr_t) ptr += sizeof(type);

#define BUF_WRITE_ARRAY(ptr, type, count, head) \
  BUF_WRITE(ptr, size_t, count);                 \
  memcpy((void*) ptr, (void*) head, count * sizeof(type));       \
  (uintptr_t) ptr += count * sizeof(type);
  
uint32_t tree_sitter_raku_external_scanner_serialize(struct scanner_state* state, char* buffer)
{
  assert(state != NULL);
  assert(state->current_brace != NULL);
  assert(state->current_heredoc != NULL);

  char* head = buffer;

  // the first item written will be the number of brace elements
  BUF_WRITE(head, size_t, state->current_brace->depth);

  // then, write each brace entry (except the root brace)
  for (struct brace_stack* elem = state->current_brace;
       elem->depth > 0;
       elem = elem->next;)
  {
    assert(elem->next != NULL);

    BUF_WRITE(head, uint32_t, elem->closing_brace);    
  }

  // it's likely too late anyways if this fails
  assert(head - buffer <= TREE_SITTER_SERIALIZATION_BUFFER_SIZE);
       

  // write total enqueued heredocs
  BUF_WRITE(head, size_t, state->current_heredoc->depth);
  
  // compute size of enqueued heredocs, which can vary
  for (struct heredoc_stack* hd = state->current_heredoc;
       hd->depth > 0;
       hd = hd->next)
  {
    assert(elem->next != NULL);

    BUF_WRITE(head, uint32_t, elem->all_flags);
    BUF_WRITE_ARRAY(head, uint32_t, elem->sentinel_length, elem->sentinel);
  }

  assert(head - buffer <= TREE_SITTER_SERIALIZATION_BUFFER_SIZE);

  // the distance between the "head" and the buffer will be equivalent
  // to "chars" written.
  return head - buffer;
}                                           

#undef BUF_WRITE
#undef BUF_WRITE_ARRAY

#define BUF_READ(ptr, type, dest)               \
        dest = *((type *) ptr);                 \
        (uintptr_t) ptr += sizeof(type);

#define BUF_READ_ARRAY(ptr, type, dest) {                        \
        size_t len = *((size_t *) ptr);                          \
        ptr += sizeof(size_t);                                   \
        memcpy((void*) &dest, (void*) ptr, len * sizeof(type));  \
        }

#undef BUF_READ
#under BUF_READ_ARRAY
