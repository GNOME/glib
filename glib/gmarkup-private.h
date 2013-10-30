#include "gstring.h"

typedef enum
{
  STATE_START,
  STATE_AFTER_OPEN_ANGLE,
  STATE_AFTER_CLOSE_ANGLE,
  STATE_AFTER_ELISION_SLASH, /* the slash that obviates need for end element */
  STATE_INSIDE_OPEN_TAG_NAME,
  STATE_INSIDE_ATTRIBUTE_NAME,
  STATE_AFTER_ATTRIBUTE_NAME,
  STATE_BETWEEN_ATTRIBUTES,
  STATE_AFTER_ATTRIBUTE_EQUALS_SIGN,
  STATE_INSIDE_ATTRIBUTE_VALUE_SQ,
  STATE_INSIDE_ATTRIBUTE_VALUE_DQ,
  STATE_INSIDE_TEXT,
  STATE_AFTER_CLOSE_TAG_SLASH,
  STATE_INSIDE_CLOSE_TAG_NAME,
  STATE_AFTER_CLOSE_TAG_NAME,
  STATE_INSIDE_PASSTHROUGH,
  STATE_ERROR
} GMarkupParseState;

typedef struct
{
  const char *prev_element;
  const GMarkupParser *prev_parser;
  gpointer prev_user_data;
} GMarkupRecursionTracker;

struct _GMarkupParseContext
{
  const GMarkupParser *parser;

  volatile gint ref_count;

  GMarkupParseFlags flags;

  gint line_number;
  gint char_number;

  GMarkupParseState state;

  gpointer user_data;
  GDestroyNotify dnotify;

  /* A piece of character data or an element that
   * hasn't "ended" yet so we haven't yet called
   * the callback for it.
   */
  GString *partial_chunk;
  GSList *spare_chunks;

  GSList *tag_stack;
  GSList *tag_stack_gstr;
  GSList *spare_list_nodes;

  GString **attr_names;
  GString **attr_values;
  gint cur_attr;
  gint alloc_attrs;

  const gchar *current_text;
  gsize        current_text_len;
  const gchar *current_text_end;

  /* used to save the start of the last interesting thingy */
  const gchar *start;

  const gchar *iter;

  guint document_empty : 1;
  guint parsing : 1;
  guint awaiting_pop : 1;
  gint balance;

  /* subparser support */
  GSList *subparser_stack; /* (GMarkupRecursionTracker *) */
  const char *subparser_element;
  gpointer held_user_data;
};

GLIB_AVAILABLE_IN_ALL
gboolean
g_markup_parse_context_parse_slightly (GMarkupParseContext  *context,
                                       GError              **error);
