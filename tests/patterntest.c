/* GLIB - Library of useful routines for C programming
 * Copyright (C) 2001 Matthias Clasen <matthiasc@poet.de>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#undef G_DISABLE_ASSERT
#undef G_LOG_DOMAIN

#include <string.h>

#include "glib.h"
#include "glib/gpattern.h"

static gboolean noisy = FALSE;

static void
verbose (const gchar *format, ...)
{
  gchar *msg;
  va_list args;

  va_start (args, format);
  msg = g_strdup_vprintf (format, args);
  va_end (args);

  if (noisy) 
    g_print (msg);
  g_free (msg);
}

/* keep enum and structure of gpattern.c and patterntest.c in sync */
typedef enum
{
  G_MATCH_ALL,       /* "*A?A*" */
  G_MATCH_ALL_TAIL,  /* "*A?AA" */
  G_MATCH_HEAD,      /* "AAAA*" */
  G_MATCH_TAIL,      /* "*AAAA" */
  G_MATCH_EXACT,     /* "AAAAA" */
  G_MATCH_LAST
} GMatchType;

struct _GPatternSpec
{
  GMatchType match_type;
  guint      pattern_length;
  guint      min_length;
  gchar     *pattern;
};


static gchar *
match_type_name (GMatchType match_type)
{
  switch (match_type)
    {
    case G_MATCH_ALL: 
      return "G_MATCH_ALL";
      break;
    case G_MATCH_ALL_TAIL:
      return "G_MATCH_ALL_TAIL";
      break;
    case G_MATCH_HEAD:
      return "G_MATCH_HEAD";
      break;
    case G_MATCH_TAIL:
      return "G_MATCH_TAIL";
      break;
    case G_MATCH_EXACT:
      return "G_MATCH_EXACT";
      break;
    default:
      return "unknown GMatchType";
      break;
    }
}

static gboolean
test_compilation (gchar *src, 
		  GMatchType match_type, 
		  gchar *pattern,
		  guint min)
{
  GPatternSpec *spec; 

  verbose ("compiling \"%s\" \t", src);
  spec = g_pattern_spec_new (src);

  if (spec->match_type != match_type)
    {
      g_print ("failed \t(match_type: %s, expected %s)\n",
	       match_type_name (spec->match_type), 
	       match_type_name (match_type));
      return FALSE;
    }
  
  if (strcmp (spec->pattern, pattern) != 0)
    {
      g_print ("failed \t(pattern: \"%s\", expected \"%s\")\n",
	       spec->pattern,
	       pattern);
      return FALSE;
    }
  
  if (spec->pattern_length != strlen (spec->pattern))
    {
      g_print ("failed \t(pattern_length: %d, expected %d)\n",
	       spec->pattern_length,
	       (gint)strlen (spec->pattern));
      return FALSE;
    }
  
  if (spec->min_length != min)
    {
      g_print ("failed \t(min_length: %d, expected %d)\n",
	       spec->min_length,
	       min);
      return FALSE;
    }
  
  verbose ("passed (%s: \"%s\")\n",
	   match_type_name (spec->match_type),
	   spec->pattern);
  
  return TRUE;
}

static gboolean
test_match (gchar *pattern, 
	    gchar *string, 
	    gboolean match)
{
  verbose ("matching \"%s\" against \"%s\" \t", string, pattern);
  
  if (g_pattern_match_simple (pattern, string) != match)
    {
      g_print ("failed \t(unexpected %s)\n", (match ? "mismatch" : "match"));
      return FALSE;
    }
  
  verbose ("passed (%s)\n", match ? "match" : "nomatch");

  return TRUE;
}

static gboolean
test_equal (gchar *pattern1,
	    gchar *pattern2,
	    gboolean expected)
{
  GPatternSpec *p1 = g_pattern_spec_new (pattern1);
  GPatternSpec *p2 = g_pattern_spec_new (pattern2);
  gboolean equal = g_pattern_spec_equal (p1, p2);

  verbose ("comparing \"%s\" with \"%s\" \t", pattern1, pattern2);

  if (expected != equal)
    {
      g_print ("failed \t{%s, %u, \"%s\"} %s {%s, %u, \"%s\"}\n",
	       match_type_name (p1->match_type), p1->pattern_length, p1->pattern,
	       expected ? "!=" : "==",
	       match_type_name (p2->match_type), p2->pattern_length, p2->pattern);
    }
  else
    verbose ("passed (%s)\n", equal ? "equal" : "unequal");
  
  g_pattern_spec_free (p1);
  g_pattern_spec_free (p2);

  return expected == equal;
}

#define TEST_COMPILATION(src, type, pattern, min) { \
  total++; \
  if (test_compilation (src, type, pattern, min)) \
    passed++; \
  else \
    failed++; \
}

#define TEST_MATCH(pattern, string, match) { \
  total++; \
  if (test_match (pattern, string, match)) \
    passed++; \
  else \
    failed++; \
}

#define TEST_EQUAL(pattern1, pattern2, match) { \
  total++; \
  if (test_equal (pattern1, pattern2, match)) \
    passed++; \
  else \
    failed++; \
}

int
main (int argc, char** argv)
{
  gint total = 0;
  gint passed = 0;
  gint failed = 0;
  gint i;

  for (i = 1; i < argc; i++) 
      if (strcmp ("--noisy", argv[i]) == 0)
	noisy = TRUE;

  TEST_COMPILATION("*A?B*", G_MATCH_ALL, "*A?B*", 3);
  TEST_COMPILATION("ABC*DEFGH", G_MATCH_ALL_TAIL, "HGFED*CBA", 8);
  TEST_COMPILATION("ABCDEF*GH", G_MATCH_ALL, "ABCDEF*GH", 8);
  TEST_COMPILATION("ABC**?***??**DEF*GH", G_MATCH_ALL, "ABC*???DEF*GH", 11);
  TEST_COMPILATION("*A?AA", G_MATCH_ALL_TAIL, "AA?A*", 4);
  TEST_COMPILATION("ABCD*", G_MATCH_HEAD, "ABCD", 4);
  TEST_COMPILATION("*ABCD", G_MATCH_TAIL, "ABCD", 4);
  TEST_COMPILATION("ABCDE", G_MATCH_EXACT, "ABCDE", 5);
  TEST_COMPILATION("A?C?E", G_MATCH_ALL, "A?C?E", 5);
  TEST_COMPILATION("*?x", G_MATCH_ALL_TAIL, "x?*", 2);
  TEST_COMPILATION("?*x", G_MATCH_ALL_TAIL, "x?*", 2);
  TEST_COMPILATION("*?*x", G_MATCH_ALL_TAIL, "x?*", 2);
  TEST_COMPILATION("x*??", G_MATCH_ALL_TAIL, "??*x", 3);

  TEST_EQUAL("*A?B*", "*A?B*", TRUE);
  TEST_EQUAL("A*BCD", "A*BCD", TRUE);
  TEST_EQUAL("ABCD*", "ABCD****", TRUE);
  TEST_EQUAL("A1*", "A1*", TRUE);
  TEST_EQUAL("*YZ", "*YZ", TRUE);
  TEST_EQUAL("A1x", "A1x", TRUE);
  TEST_EQUAL("AB*CD", "AB**CD", TRUE);
  TEST_EQUAL("AB*?*CD", "AB*?CD", TRUE);
  TEST_EQUAL("AB*?CD", "AB?*CD", TRUE);
  TEST_EQUAL("AB*CD", "AB*?*CD", FALSE);
  TEST_EQUAL("ABC*", "ABC?", FALSE);

  TEST_MATCH("*x", "x", TRUE);
  TEST_MATCH("*x", "xx", TRUE);
  TEST_MATCH("*x", "yyyx", TRUE);
  TEST_MATCH("*x", "yyxy", FALSE);
  TEST_MATCH("?x", "x", FALSE);
  TEST_MATCH("?x", "xx", TRUE);
  TEST_MATCH("?x", "yyyx", FALSE);
  TEST_MATCH("?x", "yyxy", FALSE);
  TEST_MATCH("*?x", "xx", TRUE);
  TEST_MATCH("?*x", "xx", TRUE);
  TEST_MATCH("*?x", "x", FALSE);
  TEST_MATCH("?*x", "x", FALSE);
  TEST_MATCH("*?*x", "yx", TRUE);
  TEST_MATCH("*?*x", "xxxx", TRUE);
  TEST_MATCH("x*??", "xyzw", TRUE);
  TEST_MATCH("*x", "\xc3\x84x", TRUE);
  TEST_MATCH("?x", "\xc3\x84x", TRUE);
  TEST_MATCH("??x", "\xc3\x84x", FALSE);
  TEST_MATCH("ab\xc3\xa4\xc3\xb6", "ab\xc3\xa4\xc3\xb6", TRUE);
  TEST_MATCH("ab\xc3\xa4\xc3\xb6", "abao", FALSE);
  TEST_MATCH("ab?\xc3\xb6", "ab\xc3\xa4\xc3\xb6", TRUE);
  TEST_MATCH("ab?\xc3\xb6", "abao", FALSE);
  TEST_MATCH("ab\xc3\xa4?", "ab\xc3\xa4\xc3\xb6", TRUE);
  TEST_MATCH("ab\xc3\xa4?", "abao", FALSE);
  TEST_MATCH("ab??", "ab\xc3\xa4\xc3\xb6", TRUE);
  TEST_MATCH("ab*", "ab\xc3\xa4\xc3\xb6", TRUE);
  TEST_MATCH("ab*\xc3\xb6", "ab\xc3\xa4\xc3\xb6", TRUE);
  TEST_MATCH("ab*\xc3\xb6", "aba\xc3\xb6x\xc3\xb6", TRUE);
  TEST_MATCH("", "", TRUE);
  TEST_MATCH("", "abc", FALSE);
  
  verbose ("\n%u tests passed, %u failed\n", passed, failed);

  return failed;
}


