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
#include <string.h>

#include "glib.h"
#include "glib/gpattern.h"


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

/* this leakes memory, but we don't care */

#define utf8(str) g_convert (str, -1, "Latin1", "UTF-8", NULL, NULL, NULL)
#define latin1(str) g_convert (str, -1, "UTF-8", "Latin1", NULL, NULL, NULL)

static gboolean
test_compilation (gchar *src, 
		  GMatchType match_type, 
		  gchar *pattern,
		  guint min)
{
  GPatternSpec *spec; 

  g_print ("compiling \"%s\" \t", utf8(src));
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
	       utf8(spec->pattern),
	       utf8(pattern));
      return FALSE;
    }
  
  if (spec->pattern_length != strlen (spec->pattern))
    {
      g_print ("failed \t(pattern_length: %d, expected %d)\n",
	       spec->pattern_length,
	       strlen (spec->pattern));
      return FALSE;
    }
  
  if (spec->min_length != min)
    {
      g_print ("failed \t(min_length: %d, expected %d)\n",
	       spec->min_length,
	       min);
      return FALSE;
    }
  
  g_print ("passed (%s: \"%s\")\n",
	   match_type_name (spec->match_type),
	   spec->pattern);
  
  return TRUE;
}

static gboolean
test_match (gchar *pattern, 
	    gchar *string, 
	    gboolean match)
{
  g_print ("matching \"%s\" against \"%s\" \t", utf8(string), utf8(pattern));
  
  if (g_pattern_match_simple (pattern, string) != match)
    {
      g_print ("failed \t(unexpected %s)\n", (match ? "mismatch" : "match"));
      return FALSE;
    }
  
  g_print ("passed (%s)\n", match ? "match" : "nomatch");
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

  g_print ("comparing \"%s\" with \"%s\" \t", utf8(pattern1), utf8(pattern2));

  if (expected != equal)
    {
      g_print ("failed \t{%s, %u, \"%s\"} %s {%s, %u, \"%s\"}\n",
	       match_type_name (p1->match_type), p1->pattern_length, utf8(p1->pattern),
	       expected ? "!=" : "==",
	       match_type_name (p2->match_type), p2->pattern_length, utf8(p2->pattern));
    }
  else
    g_print ("passed (%s)\n", equal ? "equal" : "unequal");
  
  g_pattern_spec_free (p1);
  g_pattern_spec_free (p2);

  return expected == equal;
}

#define TEST_COMPILATION(src, type, pattern, min) { \
  total++; \
  if (test_compilation (latin1(src), type, latin1(pattern), min)) \
    passed++; \
  else \
    failed++; \
}

#define TEST_MATCH(pattern, string, match) { \
  total++; \
  if (test_match (latin1(pattern), latin1(string), match)) \
    passed++; \
  else \
    failed++; \
}

#define TEST_EQUAL(pattern1, pattern2, match) { \
  total++; \
  if (test_equal (latin1(pattern1), latin1(pattern2), match)) \
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

  TEST_EQUAL ("*A?B*", "*A?B*", TRUE);
  TEST_EQUAL ("A*BCD", "A*BCD", TRUE);
  TEST_EQUAL ("ABCD*", "ABCD****", TRUE);
  TEST_EQUAL ("A1*", "A1*", TRUE);
  TEST_EQUAL ("*YZ", "*YZ", TRUE);
  TEST_EQUAL ("A1x", "A1x", TRUE);
  TEST_EQUAL ("AB*CD", "AB**CD", TRUE);
  TEST_EQUAL ("AB*?*CD", "AB*?CD", TRUE);
  TEST_EQUAL ("AB*?CD", "AB?*CD", TRUE);
  TEST_EQUAL ("AB*CD", "AB*?*CD", FALSE);
  TEST_EQUAL ("ABC*", "ABC?", FALSE);

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
  TEST_MATCH("*x", "Äx", TRUE);
  TEST_MATCH("?x", "Äx", TRUE);
  TEST_MATCH("??x", "Äx", FALSE);
  TEST_MATCH("abäö", "abäö", TRUE);
  TEST_MATCH("abäö", "abao", FALSE);
  TEST_MATCH("ab?ö", "abäö", TRUE);
  TEST_MATCH("ab?ö", "abao", FALSE);
  TEST_MATCH("abä?", "abäö", TRUE);
  TEST_MATCH("abä?", "abao", FALSE);
  TEST_MATCH("ab??", "abäö", TRUE);
  TEST_MATCH("ab*", "abäö", TRUE);
  TEST_MATCH("ab*ö", "abäö", TRUE);
  TEST_MATCH("ab*ö", "abaöxö", TRUE);
  
  g_print ("\n%u tests passed, %u failed\n", passed, failed);

  return 0;
}
