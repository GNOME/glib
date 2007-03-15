/*
 * Copyright (C) 2005 - 2006, Marco Barisione <marco@barisione.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
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
#include <locale.h>
#include "glib.h"

#ifdef ENABLE_REGEX

/* U+20AC EURO SIGN (symbol, currency) */
#define EURO "\xe2\x82\xac"
/* U+00E0 LATIN SMALL LETTER A WITH GRAVE (letter, lowercase) */
#define AGRAVE "\xc3\xa0"
/* U+00C0 LATIN CAPITAL LETTER A WITH GRAVE (letter, uppercase) */
#define AGRAVE_UPPER "\xc3\x80"
/* U+00E8 LATIN SMALL LETTER E WITH GRAVE (letter, lowercase) */
#define EGRAVE "\xc3\xa8"
/* U+00F2 LATIN SMALL LETTER O WITH GRAVE (letter, lowercase) */
#define OGRAVE "\xc3\xb2"
/* U+014B LATIN SMALL LETTER ENG (letter, lowercase) */
#define ENG "\xc5\x8b"
/* U+0127 LATIN SMALL LETTER H WITH STROKE (letter, lowercase) */
#define HSTROKE "\xc4\xa7"
/* U+0634 ARABIC LETTER SHEEN (letter, other) */
#define SHEEN "\xd8\xb4"
/* U+1374 ETHIOPIC NUMBER THIRTY (number, other) */
#define ETH30 "\xe1\x8d\xb4"

/* A random value use to mark untouched integer variables. */
#define UNTOUCHED -559038737

static gboolean noisy = FALSE;
static gboolean abort_on_fail = FALSE;

#define PASS passed++
#define FAIL \
  G_STMT_START \
    { \
      failed++; \
      if (abort_on_fail) \
	goto end; \
    } \
  G_STMT_END

/* A replacement for strcmp that doesn't crash with null pointers. */
static gboolean
streq (const gchar *s1, const gchar *s2)
{
  if (s1 == NULL && s2 == NULL)
    return TRUE;
  else if (s1 == NULL)
    return FALSE;
  else if (s2 == NULL)
    return FALSE;
  else
    return strcmp (s1, s2) == 0;
}

static void
verbose (const gchar *format, ...)
{
  /* Function copied from glib/tests/patterntest.c by Matthias Clasen. */
  gchar *msg;
  va_list args;

  va_start (args, format);
  msg = g_strdup_vprintf (format, args);
  va_end (args);

  if (noisy) 
    g_print (msg);
  g_free (msg);
}

static gboolean
test_new (const gchar        *pattern,
	  GRegexCompileFlags  compile_opts,
	  GRegexMatchFlags    match_opts)
{
  GRegex *regex;
  
  verbose ("compiling \"%s\" \t", pattern);

  regex = g_regex_new (pattern, compile_opts, match_opts, NULL);
  if (regex == NULL)
    {
      g_print ("failed \t(pattern: \"%s\", compile: %d, match %d)\n",
	       pattern, compile_opts, match_opts);
      return FALSE;
    }

  if (!g_regex_optimize (regex, NULL))
    {
      g_print ("failed optimization \t(pattern: \"%s\", compile: %d, match %d)\n",
	       pattern, compile_opts, match_opts);
      return FALSE;
    }

  if (!streq (g_regex_get_pattern (regex), pattern))
    {
      g_print ("failed \t(pattern: \"%s\")\n",
	       pattern);
      g_regex_free (regex);
      return FALSE;
    }

  g_regex_free (regex);
  /* Free a null string. */
  g_regex_free (NULL);

  verbose ("passed\n");
  return TRUE;
}

#define TEST_NEW(pattern, compile_opts, match_opts) { \
  total++; \
  if (test_new (pattern, compile_opts, match_opts)) \
    PASS; \
  else \
    FAIL; \
}

static gboolean
test_new_fail (const gchar        *pattern,
	       GRegexCompileFlags  compile_opts)
{
  GRegex *regex;
  
  verbose ("compiling \"%s\" (expected a failure) \t", pattern);

  regex = g_regex_new (pattern, compile_opts, 0, NULL);

  if (regex != NULL)
    {
      g_print ("failed \t(pattern: \"%s\", compile: %d)\n",
	       pattern, compile_opts);
      g_regex_free (regex);
      return FALSE;
    }

  verbose ("passed\n");
  return TRUE;
}

#define TEST_NEW_FAIL(pattern, compile_opts) { \
  total++; \
  if (test_new_fail (pattern, compile_opts)) \
    PASS; \
  else \
    FAIL; \
}

static gboolean
test_copy (const gchar  *pattern)
{
  GRegex *regex1, *regex2, *regex3;

  verbose ("copying \"%s\" \t", pattern);

  regex1 = g_regex_new (pattern, 0, 0, NULL);
  if (regex1 != NULL)
    /* pattern can be not valid as we want to test what happens
     * when the regex passed to g_regex_copy() is null */
    g_regex_optimize (regex1, NULL);
  regex2 = g_regex_copy (regex1);

  if (regex1 != NULL &&
      !streq (g_regex_get_pattern (regex1), g_regex_get_pattern(regex2)))
    {
      g_print ("failed \t(pattern: \"%s\")\n", pattern);
      g_regex_free (regex1);
      g_regex_free (regex2);
      return FALSE;
    }

  g_regex_free (regex1);

  /* force the creation of the internal GRegexMatch */
  if (regex2 != NULL)
    g_regex_match (regex2, "a", 0);
  regex3 = g_regex_copy (regex2);
  g_regex_free (regex2);

  if (regex3 != NULL &&
      !streq (g_regex_get_pattern (regex3), pattern))
    {
      g_print ("failed \t(pattern: \"%s\")\n", pattern);
      g_regex_free (regex3);
      return FALSE;
    }

  g_regex_free (regex3);

  verbose ("passed\n");
  return TRUE;
}

#define TEST_COPY(pattern) { \
  total++; \
  if (test_copy (pattern)) \
    PASS; \
  else \
    FAIL; \
}

static gboolean
test_match_simple (const gchar        *pattern,
		   const gchar        *string,
		   GRegexCompileFlags  compile_opts,
		   GRegexMatchFlags    match_opts,
		   gboolean            expected)
{
  gboolean match;
  
  verbose ("matching \"%s\" against \"%s\" \t", string, pattern);

  match = g_regex_match_simple (pattern, string, compile_opts, match_opts);
  if (match != expected)
    {
      g_print ("failed \t(unexpected %s)\n", match ? "match" : "mismatch");
      return FALSE;
    }
  else
    {
      verbose ("passed (%s)\n", match ? "match" : "nomatch");
      return TRUE;
    }
}

#define TEST_MATCH_SIMPLE(pattern, string, compile_opts, match_opts, expected) { \
  total++; \
  if (test_match_simple (pattern, string, compile_opts, match_opts, expected)) \
    PASS; \
  else \
    FAIL; \
}

static gboolean
test_match (const gchar        *pattern,
	    GRegexCompileFlags  compile_opts,
	    GRegexMatchFlags    match_opts,
	    const gchar        *string,
	    gssize              string_len,
	    gint                start_position,
	    GRegexMatchFlags    match_opts2,
	    gboolean            expected)
{
  GRegex *regex;
  gboolean match;
  
  verbose ("matching \"%s\" against \"%s\" (start: %d, len: %d) \t",
	   string, pattern, start_position, string_len);

  regex = g_regex_new (pattern, compile_opts, match_opts, NULL);
  match = g_regex_match_full (regex, string, string_len,
			      start_position, match_opts2, NULL);
  if (match != expected)
    {
      g_print ("failed \t(unexpected %s)\n", match ? "match" : "mismatch");
      g_regex_free (regex);
      return FALSE;
    }

  /* Repeat the test to verify that g_regex_clear() is not needed. */
  match = g_regex_match_full (regex, string, string_len,
			      start_position, match_opts2, NULL);
  if (match != expected)
    {
      g_print ("failed \t(second match != first match)\n");
      g_regex_free (regex);
      return FALSE;
    }

  if (string_len == -1 && start_position == 0)
    {
      match = g_regex_match (regex, string, match_opts2);
      if (match != expected)
	{
	  g_print ("failed \t(pattern: \"%s\", string: \"%s\")\n",
		   pattern, string);
	  g_regex_free (regex);
	  return FALSE;
	}
    }

  g_regex_free (regex);

  verbose ("passed (%s)\n", match ? "match" : "nomatch");
  return TRUE;
}

#define TEST_MATCH(pattern, compile_opts, match_opts, string, \
		   string_len, start_position, match_opts2, expected) { \
  total++; \
  if (test_match (pattern, compile_opts, match_opts, string, \
		  string_len, start_position, match_opts2, expected)) \
    PASS; \
  else \
    FAIL; \
}

struct _Match
{
  gchar *string;
  gint start, end;
};
typedef struct _Match Match;

static void
free_match (gpointer data, gpointer user_data)
{
  Match *match = data;
  if (match == NULL)
    return;
  g_free (match->string);
  g_free (match);
}

static gboolean
test_match_next_full (const gchar          *pattern,
		      const gchar          *string,
		      gssize                string_len,
		      gint                  start_position,
		      ...)
{
  GRegex *regex;
  va_list args;
  GSList *matches = NULL;
  GSList *expected = NULL;
  GSList *l_exp, *l_match;
  gboolean ret = TRUE;
  
  verbose ("matching \"%s\" against \"%s\" (start: %d, len: %d) \t",
	   string, pattern, start_position, string_len);

  /* The va_list is a NULL-terminated sequence of: extected matched string,
   * expected start and expected end. */
  va_start (args, start_position);
  while (TRUE)
   {
      Match *match;
      const gchar *expected_string = va_arg (args, const gchar *);
      if (expected_string == NULL)
        break;
      match = g_new0 (Match, 1);
      match->string = g_strdup (expected_string);
      match->start = va_arg (args, gint);
      match->end = va_arg (args, gint);
      expected = g_slist_prepend (expected, match);
    }
  expected = g_slist_reverse (expected);
  va_end (args);

  regex = g_regex_new (pattern, 0, 0, NULL);

  while (g_regex_match_next_full (regex, string, string_len,
				  start_position, 0, NULL))
   {
      Match *match = g_new0 (Match, 1);
      match->string = g_regex_fetch (regex, 0, string);
      match->start = UNTOUCHED;
      match->end = UNTOUCHED;
      g_regex_fetch_pos (regex, 0, &match->start, &match->end);
      matches = g_slist_prepend (matches, match);
   }
  matches = g_slist_reverse (matches);

  if (g_slist_length (matches) != g_slist_length (expected))
    {
      gint match_count = g_slist_length (matches);
      g_print ("failed \t(got %d %s, expected %d)\n", match_count,
	       match_count == 1 ? "match" : "matches", 
	       g_slist_length (expected));
      ret = FALSE;
      goto exit;
    }

  l_exp = expected;
  l_match =  matches;
  while (l_exp != NULL)
    {
      Match *exp = l_exp->data;
      Match *match = l_match->data;

      if (!streq(exp->string, match->string))
	{
	  g_print ("failed \t(got \"%s\", expected \"%s\")\n",
		   match->string, exp->string);
	  ret = FALSE;
	  goto exit;
	}

      if (exp->start != match->start || exp->end != match->end)
	{
	  g_print ("failed \t(got [%d, %d], expected [%d, %d])\n",
		   match->start, match->end, exp->start, exp->end);
	  ret = FALSE;
	  goto exit;
	}

      l_exp = g_slist_next (l_exp);
      l_match = g_slist_next (l_match);
    }

exit:
  if (ret)
    {
      gint count = g_slist_length (matches);
      verbose ("passed (%d %s)\n", count, count == 1 ? "match" : "matches");
    }

  g_regex_free (regex);
  g_slist_foreach (expected, free_match, NULL);
  g_slist_free (expected);
  g_slist_foreach (matches, free_match, NULL);
  g_slist_free (matches);

  return ret;
}

static gboolean
test_match_next (const gchar          *pattern,
		 const gchar          *string,
		 ...)
{
  GRegex *regex;
  va_list args;
  GSList *matches = NULL;
  GSList *expected = NULL;
  GSList *l_exp, *l_match;
  gboolean ret = TRUE;
  
  verbose ("matching \"%s\" against \"%s\" \t", string, pattern);

  /* The va_list is a NULL-terminated sequence of: extected matched string,
   * expected start and expected end. */
  va_start (args, string);
  while (TRUE)
   {
      Match *match;
      const gchar *expected_string = va_arg (args, const gchar *);
      if (expected_string == NULL)
        break;
      match = g_new0 (Match, 1);
      match->string = g_strdup (expected_string);
      match->start = va_arg (args, gint);
      match->end = va_arg (args, gint);
      expected = g_slist_prepend (expected, match);
    }
  expected = g_slist_reverse (expected);
  va_end (args);

  regex = g_regex_new (pattern, 0, 0, NULL);

  while (g_regex_match_next (regex, string, 0))
   {
      Match *match = g_new0 (Match, 1);
      match->string = g_regex_fetch (regex, 0, string);
      match->start = UNTOUCHED;
      match->end = UNTOUCHED;
      g_regex_fetch_pos (regex, 0, &match->start, &match->end);
      matches = g_slist_prepend (matches, match);
   }
  matches = g_slist_reverse (matches);

  if (g_slist_length (matches) != g_slist_length (expected))
    {
      gint match_count = g_slist_length (matches);
      g_print ("failed \t(got %d %s, expected %d)\n", match_count,
	       match_count == 1 ? "match" : "matches", 
	       g_slist_length (expected));
      ret = FALSE;
      goto exit;
    }

  l_exp = expected;
  l_match =  matches;
  while (l_exp != NULL)
    {
      Match *exp = l_exp->data;
      Match *match = l_match->data;

      if (!streq(exp->string, match->string))
	{
	  g_print ("failed \t(got \"%s\", expected \"%s\")\n",
		   match->string, exp->string);
	  ret = FALSE;
	  goto exit;
	}

      if (exp->start != match->start || exp->end != match->end)
	{
	  g_print ("failed \t(got [%d, %d], expected [%d, %d])\n",
		   match->start, match->end, exp->start, exp->end);
	  ret = FALSE;
	  goto exit;
	}

      l_exp = g_slist_next (l_exp);
      l_match = g_slist_next (l_match);
    }

exit:
  if (ret)
    {
      gint count = g_slist_length (matches);
      verbose ("passed (%d %s)\n", count, count == 1 ? "match" : "matches");
    }

  g_regex_free (regex);
  g_slist_foreach (expected, free_match, NULL);
  g_slist_free (expected);
  g_slist_foreach (matches, free_match, NULL);
  g_slist_free (matches);

  return ret;
}

#define TEST_MATCH_NEXT0(pattern, string, string_len, start_position) { \
  total++; \
  if (test_match_next_full (pattern, string, string_len, start_position, NULL)) \
    PASS; \
  else \
    FAIL; \
  if (string_len == -1 && start_position == 0) \
  { \
    total++; \
    if (test_match_next (pattern, string, NULL)) \
      PASS; \
    else \
      FAIL; \
  } \
}

#define TEST_MATCH_NEXT1(pattern, string, string_len, start_position, \
			      t1, s1, e1) { \
  total++; \
  if (test_match_next_full (pattern, string, string_len, start_position, \
			    t1, s1, e1, NULL)) \
    PASS; \
  else \
    FAIL; \
  if (string_len == -1 && start_position == 0) \
  { \
    total++; \
    if (test_match_next (pattern, string, t1, s1, e1, NULL)) \
      PASS; \
    else \
      FAIL; \
  } \
}

#define TEST_MATCH_NEXT2(pattern, string, string_len, start_position, \
			 t1, s1, e1, t2, s2, e2) { \
  total++; \
  if (test_match_next_full (pattern, string, string_len, start_position, \
			    t1, s1, e1, t2, s2, e2, NULL)) \
    PASS; \
  else \
    FAIL; \
  if (string_len == -1 && start_position == 0) \
  { \
    total++; \
    if (test_match_next (pattern, string, t1, s1, e1, t2, s2, e2, NULL)) \
      PASS; \
    else \
      FAIL; \
  } \
}

#define TEST_MATCH_NEXT3(pattern, string, string_len, start_position, \
			 t1, s1, e1, t2, s2, e2, t3, s3, e3) { \
  total++; \
  if (test_match_next_full (pattern, string, string_len, start_position, \
			    t1, s1, e1, t2, s2, e2, t3, s3, e3, NULL)) \
    PASS; \
  else \
    FAIL; \
  if (string_len == -1 && start_position == 0) \
  { \
    total++; \
    if (test_match_next (pattern, string, t1, s1, e1, t2, s2, e2, t3, s3, e3, NULL)) \
      PASS; \
    else \
      FAIL; \
  } \
}

#define TEST_MATCH_NEXT4(pattern, string, string_len, start_position, \
			 t1, s1, e1, t2, s2, e2, t3, s3, e3, t4, s4, e4) { \
  total++; \
  if (test_match_next_full (pattern, string, string_len, start_position, \
			    t1, s1, e1, t2, s2, e2, t3, s3, e3, t4, s4, e4, NULL)) \
    PASS; \
  else \
    FAIL; \
  if (string_len == -1 && start_position == 0) \
  { \
    total++;\
    if (test_match_next (pattern, string, t1, s1, e1, t2, s2, e2, t3, s3, e3, \
			 t4, s4, e4, NULL)) \
      PASS; \
    else \
      FAIL; \
  } \
}

static gboolean
test_match_count (const gchar      *pattern,
		  const gchar      *string,
		  gint              start_position,
                  GRegexMatchFlags  match_opts,
		  gint              expected_count)
{
  GRegex *regex;
  gint count;
  
  verbose ("fetching match count (string: \"%s\", pattern: \"%s\", start: %d) \t",
	   string, pattern, start_position);

  regex = g_regex_new (pattern, 0, 0, NULL);

  g_regex_match_next_full (regex, string, -1, start_position,
			   match_opts, NULL);
  count = g_regex_get_match_count (regex);

  if (count != expected_count)
    {
      g_print ("failed \t(got %d, expected: %d)\n", count, expected_count);
      return FALSE;
    }

  g_regex_free (regex);

  verbose ("passed\n");
  return TRUE;
}

#define TEST_MATCH_COUNT(pattern, string, start_position, match_opts, expected_count) { \
  total++; \
  if (test_match_count (pattern, string, start_position, match_opts, expected_count)) \
    PASS; \
  else \
    FAIL; \
}

static gboolean
test_partial (const gchar *pattern,
	      const gchar *string,
	      gboolean     expected)
{
  GRegex *regex;
  
  verbose ("partial matching (string: \"%s\", pattern: \"%s\") \t",
	   string, pattern);

  regex = g_regex_new (pattern, 0, 0, NULL);

  g_regex_match (regex, string, G_REGEX_MATCH_PARTIAL);
  if (expected != g_regex_is_partial_match (regex))
    {
      g_print ("failed \t(got %d, expected: %d)\n", !expected, expected);
      g_regex_free (regex);
      return FALSE;
    }

  if (expected && g_regex_fetch_pos (regex, 0, NULL, NULL))
    {
      g_print ("failed \t(got sub-pattern 0)\n");
      g_regex_free (regex);
      return FALSE;
    }

  if (expected && g_regex_fetch_pos (regex, 1, NULL, NULL))
    {
      g_print ("failed \t(got sub-pattern 1)\n");
      g_regex_free (regex);
      return FALSE;
    }

  g_regex_free (regex);

  verbose ("passed\n");
  return TRUE;
}

#define TEST_PARTIAL(pattern, string, expected) { \
  total++; \
  if (test_partial (pattern, string, expected)) \
    PASS; \
  else \
    FAIL; \
}

static gboolean
test_clear (const gchar *pattern,
	    const gchar *string,
	    gint         start_position)
{
  GRegex *regex;
  gboolean match1, match2;
  gint start1 = UNTOUCHED;
  gint end1 = UNTOUCHED;
  gint start2 = UNTOUCHED;
  gint end2 = UNTOUCHED;
  gchar *text1 = NULL;
  gchar *text2 = NULL;
  gboolean ret = TRUE;
  
  verbose ("testing clear with \"%s\" against \"%s\" (start: %d) \t",
	   string, pattern, start_position);

  regex = g_regex_new (pattern, 0, 0, NULL);

  match1 = g_regex_match_next_full (regex, string, UNTOUCHED, start_position,
				    0, NULL);
  if (match1)
    {
      text1 = g_regex_fetch (regex, 0, string);
      g_regex_fetch_pos (regex, 0, &start1, &end1);
    }

  g_regex_clear (regex);

  match2 = g_regex_match_next_full (regex, string, UNTOUCHED, start_position,
				    0, NULL);
  if (match2)
    {
      text2 = g_regex_fetch (regex, 0, string);
      g_regex_fetch_pos (regex, 0, &start2, &end2);
    }

  if (match1 != match2)
    {
      g_print ("failed \t(different matches)\n");
      ret = FALSE;
    }
  else if (match1)
    {
      if (!streq (text1, text2))
	{
	  g_print ("failed \t(first: \"%s\", second: \"%s\")\n",
		   text1, text2);
	  ret = FALSE;
	}
      if (start1 != start2 || end1 != end2)
	{
	  g_print ("failed \t(first: [%d, %d], second: [%d, %d])\n",
		   start1, end1, start2, end2);
	  ret = FALSE;
	}
    }

  g_regex_free (regex);
  g_free (text1);
  g_free (text2);

  if (ret)
    verbose ("passed\n");

  return ret;
}

#define TEST_CLEAR(pattern, string, start_position) { \
  total++; \
  if (test_clear (pattern, string, start_position)) \
    PASS; \
  else \
    FAIL; \
}

static gboolean
test_sub_pattern (const gchar *pattern,
		  const gchar *string,
		  gint         start_position,
		  gint         sub_n,
		  const gchar *expected_sub,
		  gint         expected_start,
		  gint         expected_end)
{
  GRegex *regex;
  gchar *sub_expr;
  gint start = UNTOUCHED, end = UNTOUCHED;

  verbose ("fetching sub-pattern %d from \"%s\" (pattern: \"%s\") \t",
	   sub_n, string, pattern);

  regex = g_regex_new (pattern, 0, 0, NULL);
  g_regex_match_full (regex, string, -1, start_position, 0, NULL);

  sub_expr = g_regex_fetch (regex, sub_n, string);
  if (!streq(sub_expr, expected_sub))
    {
      g_print ("failed \t(got \"%s\", expected \"%s\")\n",
	       sub_expr, expected_sub);
      g_free (sub_expr);
      g_regex_free (regex);
      return FALSE;
    }
  g_free (sub_expr);

  g_regex_fetch_pos (regex, sub_n, &start, &end);
  if (start != expected_start || end != expected_end)
    {
      g_print ("failed \t(got [%d, %d], expected [%d, %d])\n",
	       start, end, expected_start, expected_end);
      g_regex_free (regex);
      return FALSE;
    }

  /* Repeat the test to verify that g_regex_clear() is not needed. */
  g_regex_match_full (regex, string, -1, start_position, 0, NULL);

  sub_expr = g_regex_fetch (regex, sub_n, string);
  if (!streq(sub_expr, expected_sub))
    {
      g_print ("failed \t(second match != first matchs)\n");
      g_free (sub_expr);
      g_regex_free (regex);
      return FALSE;
    }
  g_free (sub_expr);

  g_regex_fetch_pos (regex, sub_n, &start, &end);
  if (start != expected_start || end != expected_end)
    {
      g_print ("failed \t(second match != first matchs)\n");
      g_regex_free (regex);
      return FALSE;
    }


  g_regex_free (regex);

  verbose ("passed\n");
  return TRUE;
}

#define TEST_SUB_PATTERN(pattern, string, start_position, sub_n, expected_sub, \
			 expected_start, expected_end) { \
  total++; \
  if (test_sub_pattern (pattern, string, start_position, sub_n, expected_sub, \
			expected_start, expected_end)) \
    PASS; \
  else \
    FAIL; \
}

static gboolean
test_named_sub_pattern (const gchar *pattern,
			const gchar *string,
			gint         start_position,
			const gchar *sub_name,
			const gchar *expected_sub,
			gint         expected_start,
			gint         expected_end)
{
  GRegex *regex;
  gint start = UNTOUCHED, end = UNTOUCHED;
  gchar *sub_expr;

  verbose ("fetching sub-pattern \"%s\" from \"%s\" (pattern: \"%s\") \t",
	   sub_name, string, pattern);

  regex = g_regex_new (pattern, 0, 0, NULL);

  g_regex_match_full (regex, string, -1, start_position, 0, NULL);
  sub_expr = g_regex_fetch_named (regex, sub_name, string);
  if (!streq (sub_expr, expected_sub))
    {
      g_print ("failed \t(got \"%s\", expected \"%s\")\n",
	       sub_expr, expected_sub);
      g_free (sub_expr);
      g_regex_free (regex);
      return FALSE;
    }
  g_free (sub_expr);

  g_regex_fetch_named_pos (regex, sub_name, &start, &end);
  if (start != expected_start || end != expected_end)
    {
      g_print ("failed \t(got [%d, %d], expected [%d, %d])\n",
	       start, end, expected_start, expected_end);
      g_regex_free (regex);
      return FALSE;
    }

  g_regex_free (regex);

  verbose ("passed\n");
  return TRUE;
}

#define TEST_NAMED_SUB_PATTERN(pattern, string, start_position, sub_name, \
			       expected_sub, expected_start, expected_end) { \
  total++; \
  if (test_named_sub_pattern (pattern, string, start_position, sub_name, \
			      expected_sub, expected_start, expected_end)) \
    PASS; \
  else \
    FAIL; \
}

static gboolean
test_fetch_all (const gchar *pattern,
		const gchar *string,
		...)
{
  GRegex *regex;
  va_list args;
  GSList *expected = NULL;
  GSList *l_exp;
  gchar **matches;
  gint match_count;
  gboolean ret = TRUE;
  gint i;
  
  verbose ("fetching all sub-patterns from \"%s\" (pattern: \"%s\") \t",
	   string, pattern);

  /* The va_list is a NULL-terminated sequence of extected strings. */
  va_start (args, string);
  while (TRUE)
   {
      gchar *expected_string = va_arg (args, gchar *);
      if (expected_string == NULL)
        break;
      else
        expected = g_slist_prepend (expected, g_strdup (expected_string));
    }
  expected = g_slist_reverse (expected);
  va_end (args);

  regex = g_regex_new (pattern, 0, 0, NULL);
  g_regex_match (regex, string, 0);
  matches = g_regex_fetch_all (regex, string);
  if (matches)
    match_count = g_strv_length (matches);
  else
    match_count = 0;

  if (match_count != g_slist_length (expected))
    {
      g_print ("failed \t(got %d %s, expected %d)\n", match_count,
	       match_count == 1 ? "match" : "matches", 
	       g_slist_length (expected));
      ret = FALSE;
      goto exit;
    }

  l_exp = expected;
  for (i = 0; l_exp != NULL; i++, l_exp = g_slist_next (l_exp))
    {
      if (!streq(l_exp->data, matches [i]))
	{
	  g_print ("failed \t(got \"%s\", expected \"%s\")\n",
		   matches [i], (gchar *)l_exp->data);
	  ret = FALSE;
	  goto exit;
	}
    }

  verbose ("passed (%d %s)\n", match_count,
	   match_count == 1 ? "match" : "matches");

exit:
  g_regex_free (regex);
  g_slist_foreach (expected, (GFunc)g_free, NULL);
  g_slist_free (expected);
  g_strfreev (matches);

  return ret;
}

#define TEST_FETCH_ALL0(pattern, string) { \
  total++; \
  if (test_fetch_all (pattern, string, NULL)) \
    PASS; \
  else \
    FAIL; \
}

#define TEST_FETCH_ALL1(pattern, string, e1) { \
  total++; \
  if (test_fetch_all (pattern, string, e1, NULL)) \
    PASS; \
  else \
    FAIL; \
}

#define TEST_FETCH_ALL2(pattern, string, e1, e2) { \
  total++; \
  if (test_fetch_all (pattern, string, e1, e2, NULL)) \
    PASS; \
  else \
    FAIL; \
}

#define TEST_FETCH_ALL3(pattern, string, e1, e2, e3) { \
  total++; \
  if (test_fetch_all (pattern, string, e1, e2, e3, NULL)) \
    PASS; \
  else \
    FAIL; \
}

static gboolean
test_split_simple (const gchar *pattern,
		   const gchar *string,
		   ...)
{
  va_list args;
  GSList *expected = NULL;
  GSList *l_exp;
  gchar **tokens;
  gint token_count;
  gboolean ret = TRUE;
  gint i;
  
  verbose ("splitting \"%s\" against \"%s\" \t", string, pattern);

  /* The va_list is a NULL-terminated sequence of extected strings. */
  va_start (args, string);
  while (TRUE)
   {
      gchar *expected_string = va_arg (args, gchar *);
      if (expected_string == NULL)
        break;
      else
        expected = g_slist_prepend (expected, g_strdup (expected_string));
    }
  expected = g_slist_reverse (expected);
  va_end (args);

  tokens = g_regex_split_simple (pattern, string, 0, 0);
  if (tokens)
    token_count = g_strv_length (tokens);
  else
    token_count = 0;

  if (token_count != g_slist_length (expected))
    {
      g_print ("failed \t(got %d %s, expected %d)\n", token_count,
	       token_count == 1 ? "match" : "matches", 
	       g_slist_length (expected));
      ret = FALSE;
      goto exit;
    }

  l_exp = expected;
  for (i = 0; l_exp != NULL; i++, l_exp = g_slist_next (l_exp))
    {
      if (!streq(l_exp->data, tokens [i]))
	{
	  g_print ("failed \t(got \"%s\", expected \"%s\")\n",
		   tokens[i], (gchar *)l_exp->data);
	  ret = FALSE;
	  goto exit;
	}
    }

  verbose ("passed (%d %s)\n", token_count,
	   token_count == 1 ? "token" : "tokens");

exit:
  g_slist_foreach (expected, (GFunc)g_free, NULL);
  g_slist_free (expected);
  g_strfreev (tokens);

  return ret;
}

#define TEST_SPLIT_SIMPLE0(pattern, string) { \
  total++; \
  if (test_split_simple (pattern, string, NULL)) \
    PASS; \
  else \
    FAIL; \
}

#define TEST_SPLIT_SIMPLE1(pattern, string, e1) { \
  total++; \
  if (test_split_simple (pattern, string, e1, NULL)) \
    PASS; \
  else \
    FAIL; \
}

#define TEST_SPLIT_SIMPLE2(pattern, string, e1, e2) { \
  total++; \
  if (test_split_simple (pattern, string, e1, e2, NULL)) \
    PASS; \
  else \
    FAIL; \
}

#define TEST_SPLIT_SIMPLE3(pattern, string, e1, e2, e3) { \
  total++; \
  if (test_split_simple (pattern, string, e1, e2, e3, NULL)) \
    PASS; \
  else \
    FAIL; \
}

static gboolean
test_split_full (const gchar *pattern,
		 const gchar *string,
		 gint         start_position,
		 gint         max_tokens,
		 ...)
{
  GRegex *regex;
  va_list args;
  GSList *expected = NULL;
  GSList *l_exp;
  gchar **tokens;
  gint token_count;
  gboolean ret = TRUE;
  gint i;
  
  verbose ("splitting \"%s\" against \"%s\" (start: %d, max: %d) \t",
	   string, pattern, start_position, max_tokens);

  /* The va_list is a NULL-terminated sequence of extected strings. */
  va_start (args, max_tokens);
  while (TRUE)
   {
      gchar *expected_string = va_arg (args, gchar *);
      if (expected_string == NULL)
        break;
      else
        expected = g_slist_prepend (expected, g_strdup (expected_string));
    }
  expected = g_slist_reverse (expected);
  va_end (args);

  regex = g_regex_new (pattern, 0, 0, NULL);
  tokens = g_regex_split_full (regex, string, -1, start_position,
			       0, max_tokens, NULL);
  if (tokens)
    token_count = g_strv_length (tokens);
  else
    token_count = 0;

  if (token_count != g_slist_length (expected))
    {
      g_print ("failed \t(got %d %s, expected %d)\n", token_count,
	       token_count == 1 ? "match" : "matches", 
	       g_slist_length (expected));
      ret = FALSE;
      goto exit;
    }

  l_exp = expected;
  for (i = 0; l_exp != NULL; i++, l_exp = g_slist_next (l_exp))
    {
      if (!streq(l_exp->data, tokens [i]))
	{
	  g_print ("failed \t(got \"%s\", expected \"%s\")\n",
		   tokens[i], (gchar *)l_exp->data);
	  ret = FALSE;
	  goto exit;
	}
    }

  verbose ("passed (%d %s)\n", token_count,
	   token_count == 1 ? "token" : "tokens");

exit:
  g_regex_free (regex);
  g_slist_foreach (expected, (GFunc)g_free, NULL);
  g_slist_free (expected);
  g_strfreev (tokens);

  return ret;
}

static gboolean
test_split (const gchar *pattern,
	    const gchar *string,
	    ...)
{
  GRegex *regex;
  va_list args;
  GSList *expected = NULL;
  GSList *l_exp;
  gchar **tokens;
  gint token_count;
  gboolean ret = TRUE;
  gint i;
  
  verbose ("splitting \"%s\" against \"%s\" \t", string, pattern);

  /* The va_list is a NULL-terminated sequence of extected strings. */
  va_start (args, string);
  while (TRUE)
   {
      gchar *expected_string = va_arg (args, gchar *);
      if (expected_string == NULL)
        break;
      else
        expected = g_slist_prepend (expected, g_strdup (expected_string));
    }
  expected = g_slist_reverse (expected);
  va_end (args);

  regex = g_regex_new (pattern, 0, 0, NULL);
  tokens = g_regex_split (regex, string, 0);
  if (tokens)
    token_count = g_strv_length (tokens);
  else
    token_count = 0;

  if (token_count != g_slist_length (expected))
    {
      g_print ("failed \t(got %d %s, expected %d)\n", token_count,
	       token_count == 1 ? "match" : "matches", 
	       g_slist_length (expected));
      ret = FALSE;
      goto exit;
    }

  l_exp = expected;
  for (i = 0; l_exp != NULL; i++, l_exp = g_slist_next (l_exp))
    {
      if (!streq(l_exp->data, tokens [i]))
	{
	  g_print ("failed \t(got \"%s\", expected \"%s\")\n",
		   tokens[i], (gchar *)l_exp->data);
	  ret = FALSE;
	  goto exit;
	}
    }

  verbose ("passed (%d %s)\n", token_count,
	   token_count == 1 ? "token" : "tokens");

exit:
  g_regex_free (regex);
  g_slist_foreach (expected, (GFunc)g_free, NULL);
  g_slist_free (expected);
  g_strfreev (tokens);

  return ret;
}

#define TEST_SPLIT0(pattern, string, start_position, max_tokens) { \
  total++; \
  if (test_split_full (pattern, string, start_position, max_tokens, NULL)) \
    PASS; \
  else \
    FAIL; \
  if (start_position == 0 && max_tokens <= 0) \
  { \
    total++; \
    if (test_split (pattern, string, NULL)) \
      PASS; \
    else \
      FAIL; \
  } \
}

#define TEST_SPLIT1(pattern, string, start_position, max_tokens, e1) { \
  total++; \
  if (test_split_full (pattern, string, start_position, max_tokens, e1, NULL)) \
    PASS; \
  else \
    FAIL; \
  if (start_position == 0 && max_tokens <= 0) \
  { \
    total++; \
    if (test_split (pattern, string, e1, NULL)) \
      PASS; \
    else \
      FAIL; \
  } \
}

#define TEST_SPLIT2(pattern, string, start_position, max_tokens, e1, e2) { \
  total++; \
  if (test_split_full (pattern, string, start_position, max_tokens, e1, e2, NULL)) \
    PASS; \
  else \
    FAIL; \
  if (start_position == 0 && max_tokens <= 0) \
  { \
    total++; \
    if (test_split (pattern, string, e1, e2, NULL)) \
      PASS; \
    else \
      FAIL; \
  } \
}

#define TEST_SPLIT3(pattern, string, start_position, max_tokens, e1, e2, e3) { \
  total++; \
  if (test_split_full (pattern, string, start_position, max_tokens, e1, e2, e3, NULL)) \
    PASS; \
  else \
    FAIL; \
  if (start_position == 0 && max_tokens <= 0) \
  { \
    total++; \
    if (test_split (pattern, string, e1, e2, e3, NULL)) \
      PASS; \
    else \
      FAIL; \
  } \
}

static gboolean
test_split_next_full (const gchar *pattern,
		      const gchar *string,
		      gint         start_position,
		      ...)
{
  GRegex *regex;
  va_list args;
  GSList *expected = NULL;
  GSList *tokens;
  GSList *l_exp, *l_token;
  gint token_count;
  gchar *token;
  gboolean ret = TRUE;
  
  verbose ("splitting \"%s\" against \"%s\" (start: %d) \t",
	   string, pattern, start_position);

  /* The va_list is a NULL-terminated sequence of extected strings. */
  va_start (args, start_position);
  while (TRUE)
   {
      gchar *expected_string = va_arg (args, gchar *);
      if (expected_string == NULL)
        break;
      else
        expected = g_slist_prepend (expected, g_strdup (expected_string));
    }
  expected = g_slist_reverse (expected);
  va_end (args);

  regex = g_regex_new (pattern, 0, 0, NULL);

  tokens = NULL;
  while ((token = g_regex_split_next_full (regex, string, -1,
					   start_position, 0, NULL)))
    {
      tokens = g_slist_prepend (tokens, token);
    }
  tokens = g_slist_reverse (tokens);
  token_count = g_slist_length (tokens);

  if (token_count != g_slist_length (expected))
    {
      g_print ("failed \t(got %d %s, expected %d)\n", token_count,
	       token_count == 1 ? "match" : "matches", 
	       g_slist_length (expected));
      ret = FALSE;
      goto exit;
    }

  l_exp = expected;
  l_token = tokens;
  while (l_exp != NULL)
    {
      if (!streq(l_exp->data, l_token->data))
	{
	  g_print ("failed \t(got \"%s\", expected \"%s\")\n",
		   (gchar *)l_token->data, (gchar *)l_exp->data);
	  ret = FALSE;
	  goto exit;
	}

      l_exp = g_slist_next (l_exp);
      l_token = g_slist_next (l_token);
    }

  verbose ("passed (%d %s)\n", token_count,
	   token_count == 1 ? "token" : "tokens");

exit:
  g_regex_free (regex);
  g_slist_foreach (expected, (GFunc)g_free, NULL);
  g_slist_free (expected);
  g_slist_foreach (tokens, (GFunc)g_free, NULL);
  g_slist_free (tokens);

  return ret;
}

static gboolean
test_split_next (const gchar *pattern,
		 const gchar *string,
		 ...)
{
  GRegex *regex;
  va_list args;
  GSList *expected = NULL;
  GSList *tokens;
  GSList *l_exp, *l_token;
  gint token_count;
  gchar *token;
  gboolean ret = TRUE;
  
  verbose ("splitting \"%s\" against \"%s\" \t", string, pattern);

  /* The va_list is a NULL-terminated sequence of extected strings. */
  va_start (args, string);
  while (TRUE)
   {
      gchar *expected_string = va_arg (args, gchar *);
      if (expected_string == NULL)
        break;
      else
        expected = g_slist_prepend (expected, g_strdup (expected_string));
    }
  expected = g_slist_reverse (expected);
  va_end (args);

  regex = g_regex_new (pattern, 0, 0, NULL);

  tokens = NULL;
  while ((token = g_regex_split_next (regex, string, 0)))
    {
      tokens = g_slist_prepend (tokens, token);
    }
  tokens = g_slist_reverse (tokens);
  token_count = g_slist_length (tokens);

  if (token_count != g_slist_length (expected))
    {
      g_print ("failed \t(got %d %s, expected %d)\n", token_count,
	       token_count == 1 ? "match" : "matches", 
	       g_slist_length (expected));
      ret = FALSE;
      goto exit;
    }

  l_exp = expected;
  l_token = tokens;
  while (l_exp != NULL)
    {
      if (!streq(l_exp->data, l_token->data))
	{
	  g_print ("failed \t(got \"%s\", expected \"%s\")\n",
		   (gchar *)l_token->data, (gchar *)l_exp->data);
	  ret = FALSE;
	  goto exit;
	}

      l_exp = g_slist_next (l_exp);
      l_token = g_slist_next (l_token);
    }

  verbose ("passed (%d %s)\n", token_count,
	   token_count == 1 ? "token" : "tokens");

exit:
  g_regex_free (regex);
  g_slist_foreach (expected, (GFunc)g_free, NULL);
  g_slist_free (expected);
  g_slist_foreach (tokens, (GFunc)g_free, NULL);
  g_slist_free (tokens);

  return ret;
}

#define TEST_SPLIT_NEXT1(pattern, string, start_position, e1) { \
  total++; \
  if (test_split_next_full (pattern, string, start_position, e1, NULL)) \
    PASS; \
  else \
    FAIL; \
  if (start_position == 0) \
  { \
    total++; \
    if (test_split_next (pattern, string, e1, NULL)) \
      PASS; \
    else \
      FAIL; \
  } \
}

#define TEST_SPLIT_NEXT2(pattern, string, start_position, e1, e2) { \
  total++; \
  if (test_split_next_full (pattern, string, start_position, e1, e2, NULL)) \
    PASS; \
  else \
    FAIL; \
  if (start_position == 0) \
  { \
    total++; \
    if (test_split_next (pattern, string, e1, e2, NULL)) \
      PASS; \
    else \
      FAIL; \
  } \
}

#define TEST_SPLIT_NEXT3(pattern, string, start_position, e1, e2, e3) { \
  total++; \
  if (test_split_next_full (pattern, string, start_position, e1, e2, e3, NULL)) \
    PASS; \
  else \
    FAIL; \
  if (start_position == 0) \
  { \
    total++; \
    if (test_split_next (pattern, string, e1, e2, e3, NULL)) \
      PASS; \
    else \
      FAIL; \
  } \
}

static gboolean
test_expand (const gchar *pattern,
	     const gchar *string,
	     const gchar *string_to_expand,
	     gboolean     raw,
	     const gchar *expected)
{
  GRegex *regex;
  gchar *res;
  
  verbose ("expanding the references in \"%s\" (pattern: \"%s\", string: \"%s\") \t",
	   string_to_expand, pattern, string);

  regex = g_regex_new (pattern, raw ? G_REGEX_RAW : 0, 0, NULL);
  g_regex_match (regex, string, 0);
  res = g_regex_expand_references (regex, string, string_to_expand, NULL);
  if (!streq (res, expected))
    {
      g_print ("failed \t(got \"%s\", expected \"%s\")\n", res, expected);
      g_free (res);
      g_regex_free (regex);
      return FALSE;
    }

  g_free (res);
  g_regex_free (regex);

  verbose ("passed\n");
  return TRUE;
}

#define TEST_EXPAND(pattern, string, string_to_expand, raw, expected) { \
  total++; \
  if (test_expand (pattern, string, string_to_expand, raw, expected)) \
    PASS; \
  else \
    FAIL; \
}

static gboolean
test_replace (const gchar *pattern,
	      const gchar *string,
	      gint         start_position,
	      const gchar *replacement,
	      const gchar *expected)
{
  GRegex *regex;
  gchar *res;
  
  verbose ("replacing \"%s\" in \"%s\" (pattern: \"%s\", start: %d) \t",
	   replacement, string, pattern, start_position);

  regex = g_regex_new (pattern, 0, 0, NULL);
  res = g_regex_replace (regex, string, -1, start_position, replacement, 0, NULL);
  if (!streq (res, expected))
    {
      g_print ("failed \t(got \"%s\", expected \"%s\")\n", res, expected);
      g_free (res);
      g_regex_free (regex);
      return FALSE;
    }

  g_free (res);
  g_regex_free (regex);

  verbose ("passed\n");
  return TRUE;
}

#define TEST_REPLACE(pattern, string, start_position, replacement, expected) { \
  total++; \
  if (test_replace (pattern, string, start_position, replacement, expected)) \
    PASS; \
  else \
    FAIL; \
}

static gboolean
test_replace_lit (const gchar *pattern,
		  const gchar *string,
		  gint         start_position,
		  const gchar *replacement,
		  const gchar *expected)
{
  GRegex *regex;
  gchar *res;
  
  verbose ("replacing literally \"%s\" in \"%s\" (pattern: \"%s\", start: %d) \t",
	   replacement, string, pattern, start_position);

  regex = g_regex_new (pattern, 0, 0, NULL);
  res = g_regex_replace_literal (regex, string, -1, start_position,
				 replacement, 0, NULL);
  if (!streq (res, expected))
    {
      g_print ("failed \t(got \"%s\", expected \"%s\")\n", res, expected);
      g_free (res);
      g_regex_free (regex);
      return FALSE;
    }

  g_free (res);
  g_regex_free (regex);

  verbose ("passed\n");
  return TRUE;
}

#define TEST_REPLACE_LIT(pattern, string, start_position, replacement, expected) { \
  total++; \
  if (test_replace_lit (pattern, string, start_position, replacement, expected)) \
    PASS; \
  else \
    FAIL; \
}

static gboolean
test_get_string_number (const gchar *pattern,
			const gchar *name,
			gint         expected_num)
{
  GRegex *regex;
  gint num;
  
  verbose ("getting the number of \"%s\" (pattern: \"%s\") \t",
	   name, pattern);

  regex = g_regex_new (pattern, 0, 0, NULL);
  num = g_regex_get_string_number (regex, name);
  g_regex_free (regex);

  if (num != expected_num)
    {
      g_print ("failed \t(got %d, expected %d)\n", num, expected_num);
      return FALSE;
    }
  else
    {
      verbose ("passed\n");
      return TRUE;
    }
}

#define TEST_GET_STRING_NUMBER(pattern, name, expected_num) { \
  total++; \
  if (test_get_string_number (pattern, name, expected_num)) \
    PASS; \
  else \
    FAIL; \
}

static gboolean
test_escape (const gchar *string,
	     gint         length,
	     const gchar *expected)
{
  gchar *escaped;
  
  verbose ("escaping \"%s\" (len: %d) \t", string, length);

  escaped = g_regex_escape_string (string, length);

  if (!streq (escaped, expected))
    {
      g_print ("failed \t(got \"%s\", expected \"%s\")\n", escaped, expected);
      g_free (escaped);
      return FALSE;
    }

  g_free (escaped);

  verbose ("passed\n");
  return TRUE;
}

#define TEST_ESCAPE(string, length, expected) { \
  total++; \
  if (test_escape (string, length, expected)) \
    PASS; \
  else \
    FAIL; \
}

static gboolean
test_match_all_full (const gchar *pattern,
		     const gchar *string,
		     gssize       string_len,
		     gint         start_position,
		     ...)
{
  GRegex *regex;
  va_list args;
  GSList *expected = NULL;
  GSList *l_exp;
  gboolean match_ok;
  gboolean ret = TRUE;
  gint match_count;
  gint i;
  
  verbose ("matching all in \"%s\" against \"%s\" (start: %d, len: %d) \t",
	   string, pattern, start_position, string_len);

  /* The va_list is a NULL-terminated sequence of: extected matched string,
   * expected start and expected end. */
  va_start (args, start_position);
  while (TRUE)
   {
      Match *match;
      const gchar *expected_string = va_arg (args, const gchar *);
      if (expected_string == NULL)
        break;
      match = g_new0 (Match, 1);
      match->string = g_strdup (expected_string);
      match->start = va_arg (args, gint);
      match->end = va_arg (args, gint);
      expected = g_slist_prepend (expected, match);
    }
  expected = g_slist_reverse (expected);
  va_end (args);

  regex = g_regex_new (pattern, 0, 0, NULL);
  match_ok = g_regex_match_all_full (regex, string, string_len,
				     start_position, 0, NULL);

  if (match_ok && g_slist_length (expected) == 0)
    {
      g_print ("failed\n");
      ret = FALSE;
      goto exit;
    }
  if (!match_ok && g_slist_length (expected) != 0)
    {
      g_print ("failed\n");
      ret = FALSE;
      goto exit;
    }

  match_count = g_regex_get_match_count (regex);
  if (match_count != g_slist_length (expected))
    {
      g_print ("failed \t(got %d %s, expected %d)\n", match_count,
	       match_count == 1 ? "match" : "matches", 
	       g_slist_length (expected));
      ret = FALSE;
      goto exit;
    }

  l_exp = expected;
  for (i = 0; i < match_count; i++)
    {
      gint start, end;
      gchar *matched_string;
      Match *exp = l_exp->data;

      matched_string = g_regex_fetch (regex, i, string);
      g_regex_fetch_pos (regex, i, &start, &end);

      if (!streq(exp->string, matched_string))
	{
	  g_print ("failed \t(got \"%s\", expected \"%s\")\n",
		   matched_string, exp->string);
          g_free (matched_string);
	  ret = FALSE;
	  goto exit;
	}
      g_free (matched_string);

      if (exp->start != start || exp->end != end)
	{
	  g_print ("failed \t(got [%d, %d], expected [%d, %d])\n",
		   start, end, exp->start, exp->end);
	  ret = FALSE;
	  goto exit;
	}

      l_exp = g_slist_next (l_exp);
    }

exit:
  if (ret)
    {
      verbose ("passed (%d %s)\n", match_count, match_count == 1 ? "match" : "matches");
    }

  g_regex_free (regex);
  g_slist_foreach (expected, free_match, NULL);
  g_slist_free (expected);

  return ret;
}

static gboolean
test_match_all (const gchar *pattern,
		const gchar *string,
                ...)
{
  GRegex *regex;
  va_list args;
  GSList *expected = NULL;
  GSList *l_exp;
  gboolean match_ok;
  gboolean ret = TRUE;
  gint match_count;
  gint i;
  
  verbose ("matching all in \"%s\" against \"%s\" \t", string, pattern);

  /* The va_list is a NULL-terminated sequence of: extected matched string,
   * expected start and expected end. */
  va_start (args, string);
  while (TRUE)
   {
      Match *match;
      const gchar *expected_string = va_arg (args, const gchar *);
      if (expected_string == NULL)
        break;
      match = g_new0 (Match, 1);
      match->string = g_strdup (expected_string);
      match->start = va_arg (args, gint);
      match->end = va_arg (args, gint);
      expected = g_slist_prepend (expected, match);
    }
  expected = g_slist_reverse (expected);
  va_end (args);

  regex = g_regex_new (pattern, 0, 0, NULL);
  match_ok = g_regex_match_all (regex, string, 0);

  if (match_ok && g_slist_length (expected) == 0)
    {
      g_print ("failed\n");
      ret = FALSE;
      goto exit;
    }
  if (!match_ok && g_slist_length (expected) != 0)
    {
      g_print ("failed\n");
      ret = FALSE;
      goto exit;
    }

  match_count = g_regex_get_match_count (regex);
  if (match_count != g_slist_length (expected))
    {
      g_print ("failed \t(got %d %s, expected %d)\n", match_count,
	       match_count == 1 ? "match" : "matches", 
	       g_slist_length (expected));
      ret = FALSE;
      goto exit;
    }

  l_exp = expected;
  for (i = 0; i < match_count; i++)
    {
      gint start, end;
      gchar *matched_string;
      Match *exp = l_exp->data;

      matched_string = g_regex_fetch (regex, i, string);
      g_regex_fetch_pos (regex, i, &start, &end);

      if (!streq(exp->string, matched_string))
	{
	  g_print ("failed \t(got \"%s\", expected \"%s\")\n",
		   matched_string, exp->string);
          g_free (matched_string);
	  ret = FALSE;
	  goto exit;
	}
      g_free (matched_string);

      if (exp->start != start || exp->end != end)
	{
	  g_print ("failed \t(got [%d, %d], expected [%d, %d])\n",
		   start, end, exp->start, exp->end);
	  ret = FALSE;
	  goto exit;
	}

      l_exp = g_slist_next (l_exp);
    }

exit:
  if (ret)
    {
      verbose ("passed (%d %s)\n", match_count, match_count == 1 ? "match" : "matches");
    }

  g_regex_free (regex);
  g_slist_foreach (expected, free_match, NULL);
  g_slist_free (expected);

  return ret;
}

#define TEST_MATCH_ALL0(pattern, string, string_len, start_position) { \
  total++; \
  if (test_match_all_full (pattern, string, string_len, start_position, NULL)) \
    PASS; \
  else \
    FAIL; \
  if (string_len == -1 && start_position == 0) \
  { \
    total++; \
    if (test_match_all (pattern, string, NULL)) \
      PASS; \
    else \
      FAIL; \
  } \
}

#define TEST_MATCH_ALL1(pattern, string, string_len, start_position, \
			t1, s1, e1) { \
  total++; \
  if (test_match_all_full (pattern, string, string_len, start_position, \
			   t1, s1, e1, NULL)) \
    PASS; \
  else \
    FAIL; \
  if (string_len == -1 && start_position == 0) \
  { \
    total++; \
    if (test_match_all (pattern, string, t1, s1, e1, NULL)) \
      PASS; \
    else \
      FAIL; \
  } \
}

#define TEST_MATCH_ALL2(pattern, string, string_len, start_position, \
			t1, s1, e1, t2, s2, e2) { \
  total++; \
  if (test_match_all_full (pattern, string, string_len, start_position, \
			   t1, s1, e1, t2, s2, e2, NULL)) \
    PASS; \
  else \
    FAIL; \
  if (string_len == -1 && start_position == 0) \
  { \
    total++; \
    if (test_match_all (pattern, string, t1, s1, e1, t2, s2, e2, NULL)) \
      PASS; \
    else \
      FAIL; \
  } \
}

#define TEST_MATCH_ALL3(pattern, string, string_len, start_position, \
			t1, s1, e1, t2, s2, e2, t3, s3, e3) { \
  total++; \
  if (test_match_all_full (pattern, string, string_len, start_position, \
			   t1, s1, e1, t2, s2, e2, t3, s3, e3, NULL)) \
    PASS; \
  else \
    FAIL; \
  if (string_len == -1 && start_position == 0) \
  { \
    total++; \
    if (test_match_all (pattern, string, t1, s1, e1, t2, s2, e2, t3, s3, e3, NULL)) \
      PASS; \
    else \
      FAIL; \
  } \
}

#define TEST_NULL_MATCH(code) \
  G_STMT_START \
    { \
      GRegex *re = g_regex_new ("a", 0, 0, NULL); \
      verbose ("trying '" #code "' on a clean regex \t"); \
      code; \
      g_regex_free (re); \
      re = g_regex_new ("a", 0, 0, NULL); \
      g_regex_match (re, "b", 0); \
      g_regex_clear (re); \
      code; \
      g_regex_free (re); \
      /* this test always passes if the code does not crash */ \
      PASS; \
      verbose ("passed\n"); \
    } \
  G_STMT_END

#define TEST_NULL_MATCH_RET(code, expected, type, format) \
  G_STMT_START \
    { \
      type ret; \
      GRegex *re = g_regex_new ("a", 0, 0, NULL); \
      verbose ("trying '" #code "' on a clean regex \t"); \
      ret = code; \
      g_regex_free (re); \
      if (ret != expected) \
        { \
          g_print ("failed \t(got '" format "', expected '" format \
                   "', with a newly created regex)\n", ret, expected); \
          FAIL; \
        } \
      else \
        { \
          re = g_regex_new ("a", 0, 0, NULL); \
          g_regex_match (re, "a", 0); \
          g_regex_clear (re); \
          ret = code; \
          g_regex_free (re); \
          if (ret != expected) \
            { \
              g_print ("failed \t(got " format ", expected " format \
                       ", with a cleared regex)\n", ret, expected); \
              FAIL; \
            } \
          else \
            { \
              verbose ("passed\n"); \
              PASS; \
            } \
        } \
    } \
  G_STMT_END

int
main (int argc, char *argv[])
{
  gint total = 0;
  gint passed = 0;
  gint failed = 0;
  gint i = 0;

  setlocale (LC_ALL, "");

  for (i = 1; i < argc; i++)
    {
      if (streq ("--noisy", argv[i]))
	noisy = TRUE;
      else if (streq ("--abort", argv[i]))
	abort_on_fail = TRUE;
    }

  g_setenv ("G_DEBUG", "fatal_warnings", TRUE);

  /* TEST_NEW(pattern, compile_opts, match_opts) */
  TEST_NEW("", 0, 0);
  TEST_NEW(".*", 0, 0);
  TEST_NEW(".*", G_REGEX_MULTILINE, 0);
  TEST_NEW(".*", G_REGEX_DOTALL, 0);
  TEST_NEW(".*", G_REGEX_DOTALL, G_REGEX_MATCH_NOTBOL);
  TEST_NEW("(123\\d*)[a-zA-Z]+(?P<hello>.*)", 0, 0);
  TEST_NEW("(123\\d*)[a-zA-Z]+(?P<hello>.*)", G_REGEX_CASELESS, 0);
  TEST_NEW("(?P<A>x)|(?P<A>y)", G_REGEX_DUPNAMES, 0);
  /* This gives "internal error: code overflow" with pcre 6.0 */
  TEST_NEW("(?i)(?-i)", 0, 0);

  /* TEST_NEW_FAIL(pattern, compile_opts) */
  TEST_NEW_FAIL("(", 0);
  TEST_NEW_FAIL(")", 0);
  TEST_NEW_FAIL("[", 0);
  TEST_NEW_FAIL("*", 0);
  TEST_NEW_FAIL("?", 0);
  TEST_NEW_FAIL("(?P<A>x)|(?P<A>y)", 0);

  /* TEST_COPY(pattern) */
  TEST_COPY("");
  TEST_COPY(".*");
  TEST_COPY("a|b");
  TEST_COPY("(123\\d*)[a-zA-Z]+(?P<hello>.*)");
  /* Test if g_regex_copy() works with null regexes. */
  TEST_COPY("(");

  /* TEST_MATCH_SIMPLE(pattern, string, compile_opts, match_opts, expected) */
  TEST_MATCH_SIMPLE("a", "", 0, 0, FALSE);
  TEST_MATCH_SIMPLE("a", "a", 0, 0, TRUE);
  TEST_MATCH_SIMPLE("a", "ba", 0, 0, TRUE);
  TEST_MATCH_SIMPLE("^a", "ba", 0, 0, FALSE);
  TEST_MATCH_SIMPLE("a", "ba", G_REGEX_ANCHORED, 0, FALSE);
  TEST_MATCH_SIMPLE("a", "ba", 0, G_REGEX_MATCH_ANCHORED, FALSE);
  TEST_MATCH_SIMPLE("a", "ab", G_REGEX_ANCHORED, 0, TRUE);
  TEST_MATCH_SIMPLE("a", "ab", 0, G_REGEX_MATCH_ANCHORED, TRUE);
  TEST_MATCH_SIMPLE("a", "a", G_REGEX_CASELESS, 0, TRUE);
  TEST_MATCH_SIMPLE("a", "A", G_REGEX_CASELESS, 0, TRUE);
  /* These are needed to test extended properties. */
  TEST_MATCH_SIMPLE(AGRAVE, AGRAVE, G_REGEX_CASELESS, 0, TRUE);
  TEST_MATCH_SIMPLE(AGRAVE, AGRAVE_UPPER, G_REGEX_CASELESS, 0, TRUE);
  TEST_MATCH_SIMPLE("\\p{L}", "a", 0, 0, TRUE);
  TEST_MATCH_SIMPLE("\\p{L}", "1", 0, 0, FALSE);
  TEST_MATCH_SIMPLE("\\p{L}", AGRAVE, 0, 0, TRUE);
  TEST_MATCH_SIMPLE("\\p{L}", AGRAVE_UPPER, 0, 0, TRUE);
  TEST_MATCH_SIMPLE("\\p{L}", SHEEN, 0, 0, TRUE);
  TEST_MATCH_SIMPLE("\\p{L}", ETH30, 0, 0, FALSE);
  TEST_MATCH_SIMPLE("\\p{Ll}", "a", 0, 0, TRUE);
  TEST_MATCH_SIMPLE("\\p{Ll}", AGRAVE, 0, 0, TRUE);
  TEST_MATCH_SIMPLE("\\p{Ll}", AGRAVE_UPPER, 0, 0, FALSE);
  TEST_MATCH_SIMPLE("\\p{Ll}", ETH30, 0, 0, FALSE);
  TEST_MATCH_SIMPLE("\\p{Sc}", AGRAVE, 0, 0, FALSE);
  TEST_MATCH_SIMPLE("\\p{Sc}", EURO, 0, 0, TRUE);
  TEST_MATCH_SIMPLE("\\p{Sc}", ETH30, 0, 0, FALSE);
  TEST_MATCH_SIMPLE("\\p{N}", "a", 0, 0, FALSE);
  TEST_MATCH_SIMPLE("\\p{N}", "1", 0, 0, TRUE);
  TEST_MATCH_SIMPLE("\\p{N}", AGRAVE, 0, 0, FALSE);
  TEST_MATCH_SIMPLE("\\p{N}", AGRAVE_UPPER, 0, 0, FALSE);
  TEST_MATCH_SIMPLE("\\p{N}", SHEEN, 0, 0, FALSE);
  TEST_MATCH_SIMPLE("\\p{N}", ETH30, 0, 0, TRUE);
  TEST_MATCH_SIMPLE("\\p{Nd}", "a", 0, 0, FALSE);
  TEST_MATCH_SIMPLE("\\p{Nd}", "1", 0, 0, TRUE);
  TEST_MATCH_SIMPLE("\\p{Nd}", AGRAVE, 0, 0, FALSE);
  TEST_MATCH_SIMPLE("\\p{Nd}", AGRAVE_UPPER, 0, 0, FALSE);
  TEST_MATCH_SIMPLE("\\p{Nd}", SHEEN, 0, 0, FALSE);
  TEST_MATCH_SIMPLE("\\p{Nd}", ETH30, 0, 0, FALSE);
  TEST_MATCH_SIMPLE("\\p{Common}", SHEEN, 0, 0, FALSE);
  TEST_MATCH_SIMPLE("\\p{Common}", "a", 0, 0, FALSE);
  TEST_MATCH_SIMPLE("\\p{Common}", AGRAVE, 0, 0, FALSE);
  TEST_MATCH_SIMPLE("\\p{Common}", AGRAVE_UPPER, 0, 0, FALSE);
  TEST_MATCH_SIMPLE("\\p{Common}", ETH30, 0, 0, FALSE);
  TEST_MATCH_SIMPLE("\\p{Common}", "%", 0, 0, TRUE);
  TEST_MATCH_SIMPLE("\\p{Common}", "1", 0, 0, TRUE);
  TEST_MATCH_SIMPLE("\\p{Arabic}", SHEEN, 0, 0, TRUE);
  TEST_MATCH_SIMPLE("\\p{Arabic}", "a", 0, 0, FALSE);
  TEST_MATCH_SIMPLE("\\p{Arabic}", AGRAVE, 0, 0, FALSE);
  TEST_MATCH_SIMPLE("\\p{Arabic}", AGRAVE_UPPER, 0, 0, FALSE);
  TEST_MATCH_SIMPLE("\\p{Arabic}", ETH30, 0, 0, FALSE);
  TEST_MATCH_SIMPLE("\\p{Arabic}", "%", 0, 0, FALSE);
  TEST_MATCH_SIMPLE("\\p{Arabic}", "1", 0, 0, FALSE);
  TEST_MATCH_SIMPLE("\\p{Latin}", SHEEN, 0, 0, FALSE);
  TEST_MATCH_SIMPLE("\\p{Latin}", "a", 0, 0, TRUE);
  TEST_MATCH_SIMPLE("\\p{Latin}", AGRAVE, 0, 0, TRUE);
  TEST_MATCH_SIMPLE("\\p{Latin}", AGRAVE_UPPER, 0, 0, TRUE);
  TEST_MATCH_SIMPLE("\\p{Latin}", ETH30, 0, 0, FALSE);
  TEST_MATCH_SIMPLE("\\p{Latin}", "%", 0, 0, FALSE);
  TEST_MATCH_SIMPLE("\\p{Latin}", "1", 0, 0, FALSE);
  TEST_MATCH_SIMPLE("\\p{Ethiopic}", SHEEN, 0, 0, FALSE);
  TEST_MATCH_SIMPLE("\\p{Ethiopic}", "a", 0, 0, FALSE);
  TEST_MATCH_SIMPLE("\\p{Ethiopic}", AGRAVE, 0, 0, FALSE);
  TEST_MATCH_SIMPLE("\\p{Ethiopic}", AGRAVE_UPPER, 0, 0, FALSE);
  TEST_MATCH_SIMPLE("\\p{Ethiopic}", ETH30, 0, 0, TRUE);
  TEST_MATCH_SIMPLE("\\p{Ethiopic}", "%", 0, 0, FALSE);
  TEST_MATCH_SIMPLE("\\p{Ethiopic}", "1", 0, 0, FALSE);
  TEST_MATCH_SIMPLE("\\p{L}(?<=\\p{Arabic})", SHEEN, 0, 0, TRUE);
  TEST_MATCH_SIMPLE("\\p{L}(?<=\\p{Latin})", SHEEN, 0, 0, FALSE);
  /* Invalid patterns. */
  TEST_MATCH_SIMPLE("\\", "a", 0, 0, FALSE);
  TEST_MATCH_SIMPLE("[", "", 0, 0, FALSE);

  /* TEST_MATCH(pattern, compile_opts, match_opts, string,
   * 		string_len, start_position, match_opts2, expected) */
  TEST_MATCH("a", 0, 0, "a", -1, 0, 0, TRUE);
  TEST_MATCH("a", 0, 0, "A", -1, 0, 0, FALSE);
  TEST_MATCH("a", G_REGEX_CASELESS, 0, "A", -1, 0, 0, TRUE);
  TEST_MATCH("a", 0, 0, "ab", -1, 1, 0, FALSE);
  TEST_MATCH("a", 0, 0, "ba", 1, 0, 0, FALSE);
  TEST_MATCH("a", 0, 0, "bab", -1, 0, 0, TRUE);
  TEST_MATCH("a", 0, 0, "b", -1, 0, 0, FALSE);
  TEST_MATCH("a", 0, G_REGEX_ANCHORED, "a", -1, 0, 0, TRUE);
  TEST_MATCH("a", 0, G_REGEX_ANCHORED, "ab", -1, 1, 0, FALSE);
  TEST_MATCH("a", 0, G_REGEX_ANCHORED, "ba", 1, 0, 0, FALSE);
  TEST_MATCH("a", 0, G_REGEX_ANCHORED, "bab", -1, 0, 0, FALSE);
  TEST_MATCH("a", 0, G_REGEX_ANCHORED, "b", -1, 0, 0, FALSE);
  TEST_MATCH("a", 0, 0, "a", -1, 0, G_REGEX_ANCHORED, TRUE);
  TEST_MATCH("a", 0, 0, "ab", -1, 1, G_REGEX_ANCHORED, FALSE);
  TEST_MATCH("a", 0, 0, "ba", 1, 0, G_REGEX_ANCHORED, FALSE);
  TEST_MATCH("a", 0, 0, "bab", -1, 0, G_REGEX_ANCHORED, FALSE);
  TEST_MATCH("a", 0, 0, "b", -1, 0, G_REGEX_ANCHORED, FALSE);
  TEST_MATCH("a|b", 0, 0, "a", -1, 0, 0, TRUE);
  TEST_MATCH("\\d", 0, 0, EURO, -1, 0, 0, FALSE);
  TEST_MATCH("^.$", 0, 0, EURO, -1, 0, 0, TRUE);
  TEST_MATCH("^.{3}$", 0, 0, EURO, -1, 0, 0, FALSE);
  TEST_MATCH("^.$", G_REGEX_RAW, 0, EURO, -1, 0, 0, FALSE);
  TEST_MATCH("^.{3}$", G_REGEX_RAW, 0, EURO, -1, 0, 0, TRUE);
  TEST_MATCH(AGRAVE, G_REGEX_CASELESS, 0, AGRAVE_UPPER, -1, 0, 0, TRUE);

  /* New lines handling. */
  TEST_MATCH("^a\\Rb$", 0, 0, "a\r\nb", -1, 0, 0, TRUE);
  TEST_MATCH("^a\\Rb$", 0, 0, "a\nb", -1, 0, 0, TRUE);
  TEST_MATCH("^a\\Rb$", 0, 0, "a\rb", -1, 0, 0, TRUE);
  TEST_MATCH("^a\\Rb$", 0, 0, "a\n\rb", -1, 0, 0, FALSE);
  TEST_MATCH("^a\\R\\Rb$", 0, 0, "a\n\rb", -1, 0, 0, TRUE);
  TEST_MATCH("^a\\nb$", 0, 0, "a\r\nb", -1, 0, 0, FALSE);
  TEST_MATCH("^a\\r\\nb$", 0, 0, "a\r\nb", -1, 0, 0, TRUE);

  TEST_MATCH("^b$", 0, 0, "a\nb\nc", -1, 0, 0, FALSE);
  TEST_MATCH("^b$", G_REGEX_MULTILINE, 0, "a\nb\nc", -1, 0, 0, TRUE);
  TEST_MATCH("^b$", G_REGEX_MULTILINE, 0, "a\r\nb\r\nc", -1, 0, 0, TRUE);
  TEST_MATCH("^b$", G_REGEX_MULTILINE, 0, "a\rb\rc", -1, 0, 0, TRUE);
  TEST_MATCH("^b$", G_REGEX_MULTILINE | G_REGEX_NEWLINE_CR, 0, "a\nb\nc", -1, 0, 0, FALSE);
  TEST_MATCH("^b$", G_REGEX_MULTILINE | G_REGEX_NEWLINE_LF, 0, "a\nb\nc", -1, 0, 0, TRUE);
  TEST_MATCH("^b$", G_REGEX_MULTILINE | G_REGEX_NEWLINE_CRLF, 0, "a\nb\nc", -1, 0, 0, FALSE);
  TEST_MATCH("^b$", G_REGEX_MULTILINE | G_REGEX_NEWLINE_CR, 0, "a\r\nb\r\nc", -1, 0, 0, FALSE);
  TEST_MATCH("^b$", G_REGEX_MULTILINE | G_REGEX_NEWLINE_LF, 0, "a\r\nb\r\nc", -1, 0, 0, FALSE);
  TEST_MATCH("^b$", G_REGEX_MULTILINE | G_REGEX_NEWLINE_CRLF, 0, "a\r\nb\r\nc", -1, 0, 0, TRUE);
  TEST_MATCH("^b$", G_REGEX_MULTILINE | G_REGEX_NEWLINE_CR, 0, "a\rb\rc", -1, 0, 0, TRUE);
  TEST_MATCH("^b$", G_REGEX_MULTILINE | G_REGEX_NEWLINE_LF, 0, "a\rb\rc", -1, 0, 0, FALSE);
  TEST_MATCH("^b$", G_REGEX_MULTILINE | G_REGEX_NEWLINE_CRLF, 0, "a\rb\rc", -1, 0, 0, FALSE);
  TEST_MATCH("^b$", G_REGEX_MULTILINE, G_REGEX_MATCH_NEWLINE_CR, "a\nb\nc", -1, 0, 0, FALSE);
  TEST_MATCH("^b$", G_REGEX_MULTILINE, G_REGEX_MATCH_NEWLINE_LF, "a\nb\nc", -1, 0, 0, TRUE);
  TEST_MATCH("^b$", G_REGEX_MULTILINE, G_REGEX_MATCH_NEWLINE_CRLF, "a\nb\nc", -1, 0, 0, FALSE);
  TEST_MATCH("^b$", G_REGEX_MULTILINE, G_REGEX_MATCH_NEWLINE_CR, "a\r\nb\r\nc", -1, 0, 0, FALSE);
  TEST_MATCH("^b$", G_REGEX_MULTILINE, G_REGEX_MATCH_NEWLINE_LF, "a\r\nb\r\nc", -1, 0, 0, FALSE);
  TEST_MATCH("^b$", G_REGEX_MULTILINE, G_REGEX_MATCH_NEWLINE_CRLF, "a\r\nb\r\nc", -1, 0, 0, TRUE);
  TEST_MATCH("^b$", G_REGEX_MULTILINE, G_REGEX_MATCH_NEWLINE_CR, "a\rb\rc", -1, 0, 0, TRUE);
  TEST_MATCH("^b$", G_REGEX_MULTILINE, G_REGEX_MATCH_NEWLINE_LF, "a\rb\rc", -1, 0, 0, FALSE);
  TEST_MATCH("^b$", G_REGEX_MULTILINE, G_REGEX_MATCH_NEWLINE_CRLF, "a\rb\rc", -1, 0, 0, FALSE);

  TEST_MATCH("^b$", G_REGEX_MULTILINE | G_REGEX_NEWLINE_CR, G_REGEX_MATCH_NEWLINE_ANY, "a\nb\nc", -1, 0, 0, TRUE);
  TEST_MATCH("^b$", G_REGEX_MULTILINE | G_REGEX_NEWLINE_CR, G_REGEX_MATCH_NEWLINE_ANY, "a\rb\rc", -1, 0, 0, TRUE);
  TEST_MATCH("^b$", G_REGEX_MULTILINE | G_REGEX_NEWLINE_CR, G_REGEX_MATCH_NEWLINE_ANY, "a\r\nb\r\nc", -1, 0, 0, TRUE);
  TEST_MATCH("^b$", G_REGEX_MULTILINE | G_REGEX_NEWLINE_CR, G_REGEX_MATCH_NEWLINE_LF, "a\nb\nc", -1, 0, 0, TRUE);
  TEST_MATCH("^b$", G_REGEX_MULTILINE | G_REGEX_NEWLINE_CR, G_REGEX_MATCH_NEWLINE_LF, "a\rb\rc", -1, 0, 0, FALSE);
  TEST_MATCH("^b$", G_REGEX_MULTILINE | G_REGEX_NEWLINE_CR, G_REGEX_MATCH_NEWLINE_CRLF, "a\r\nb\r\nc", -1, 0, 0, TRUE);
  TEST_MATCH("^b$", G_REGEX_MULTILINE | G_REGEX_NEWLINE_CR, G_REGEX_MATCH_NEWLINE_CRLF, "a\rb\rc", -1, 0, 0, FALSE);

  TEST_MATCH("a#\nb", G_REGEX_EXTENDED, 0, "a", -1, 0, 0, FALSE);
  TEST_MATCH("a#\r\nb", G_REGEX_EXTENDED, 0, "a", -1, 0, 0, FALSE);
  TEST_MATCH("a#\rb", G_REGEX_EXTENDED, 0, "a", -1, 0, 0, FALSE);
  TEST_MATCH("a#\nb", G_REGEX_EXTENDED, G_REGEX_MATCH_NEWLINE_CR, "a", -1, 0, 0, FALSE);
  TEST_MATCH("a#\nb", G_REGEX_EXTENDED | G_REGEX_NEWLINE_CR, 0, "a", -1, 0, 0, TRUE);

  /* TEST_MATCH_NEXT#(pattern, string, string_len, start_position, ...) */
  TEST_MATCH_NEXT0("a", "x", -1, 0);
  TEST_MATCH_NEXT0("a", "ax", -1, 1);
  TEST_MATCH_NEXT0("a", "xa", 1, 0);
  TEST_MATCH_NEXT0("a", "axa", 1, 2);
  TEST_MATCH_NEXT1("a", "a", -1, 0, "a", 0, 1);
  TEST_MATCH_NEXT1("a", "xax", -1, 0, "a", 1, 2);
  TEST_MATCH_NEXT1(EURO, ENG EURO, -1, 0, EURO, 2, 5);
  TEST_MATCH_NEXT1("a*", "", -1, 0, "", 0, 0);
  TEST_MATCH_NEXT2("a*", "aa", -1, 0, "aa", 0, 2, "", 2, 2);
  TEST_MATCH_NEXT2(EURO "*", EURO EURO, -1, 0, EURO EURO, 0, 6, "", 6, 6);
  TEST_MATCH_NEXT2("a", "axa", -1, 0, "a", 0, 1, "a", 2, 3);
  TEST_MATCH_NEXT2("a+", "aaxa", -1, 0, "aa", 0, 2, "a", 3, 4);
  TEST_MATCH_NEXT2("a", "aa", -1, 0, "a", 0, 1, "a", 1, 2);
  TEST_MATCH_NEXT2("a", "ababa", -1, 2, "a", 2, 3, "a", 4, 5);
  TEST_MATCH_NEXT2(EURO "+", EURO "-" EURO, -1, 0, EURO, 0, 3, EURO, 4, 7);
  TEST_MATCH_NEXT3("", "ab", -1, 0, "", 0, 0, "", 1, 1, "", 2, 2);
  TEST_MATCH_NEXT3("", AGRAVE "b", -1, 0, "", 0, 0, "", 2, 2, "", 3, 3);
  TEST_MATCH_NEXT3("a", "aaxa", -1, 0, "a", 0, 1, "a", 1, 2, "a", 3, 4);
  TEST_MATCH_NEXT3("a", "aa" OGRAVE "a", -1, 0, "a", 0, 1, "a", 1, 2, "a", 4, 5);
  TEST_MATCH_NEXT3("a*", "aax", -1, 0, "aa", 0, 2, "", 2, 2, "", 3, 3);
  TEST_MATCH_NEXT4("a*", "aaxa", -1, 0, "aa", 0, 2, "", 2, 2, "a", 3, 4, "", 4, 4);

  /* TEST_MATCH_COUNT(pattern, string, start_position, match_opts, expected_count) */
  TEST_MATCH_COUNT("a", "", 0, 0, 0);
  TEST_MATCH_COUNT("a", "a", 0, 0, 1);
  TEST_MATCH_COUNT("a", "a", 1, 0, 0);
  TEST_MATCH_COUNT("(.)", "a", 0, 0, 2);
  TEST_MATCH_COUNT("(.)", EURO, 0, 0, 2);
  TEST_MATCH_COUNT("(?:.)", "a", 0, 0, 1);
  TEST_MATCH_COUNT("(?P<A>.)", "a", 0, 0, 2);
  TEST_MATCH_COUNT("a$", "a", 0, G_REGEX_MATCH_NOTEOL, 0);
  TEST_MATCH_COUNT("(a)?(b)", "b", 0, 0, 3);
  TEST_MATCH_COUNT("(a)?(b)", "ab", 0, 0, 3);

  /* TEST_PARTIAL(pattern, string, expected) */
  TEST_PARTIAL("^ab", "a", TRUE);
  TEST_PARTIAL("^ab", "xa", FALSE);
  TEST_PARTIAL("ab", "xa", TRUE);
  TEST_PARTIAL("ab", "ab", FALSE); /* normal match. */
  TEST_PARTIAL("a+b", "aa", FALSE); /* PCRE_ERROR_BAD_PARTIAL */
  TEST_PARTIAL("(a)+b", "aa", TRUE);
  TEST_PARTIAL("a?b", "a", TRUE);

  /* TEST_CLEAR(pattern, string, start_position) */
  TEST_CLEAR("$^", "aaa", 0);
  TEST_CLEAR("a", "xax", 0);
  TEST_CLEAR("a", "xax", 1);
  TEST_CLEAR("a", "xax", 2);
  TEST_CLEAR("a", "aa", 0);
  TEST_CLEAR(HSTROKE, HSTROKE, 0);

  /* TEST_SUB_PATTERN(pattern, string, start_position, sub_n, expected_sub,
   * 		      expected_start, expected_end) */
  TEST_SUB_PATTERN("a", "a", 0, 0, "a", 0, 1);
  TEST_SUB_PATTERN("a(.)", "ab", 0, 1, "b", 1, 2);
  TEST_SUB_PATTERN("a(.)", "a" EURO, 0, 1, EURO, 1, 4);
  TEST_SUB_PATTERN("(?:.*)(a)(.)", "xxa" ENG, 0, 2, ENG, 3, 5);
  TEST_SUB_PATTERN("(" HSTROKE ")", "a" HSTROKE ENG, 0, 1, HSTROKE, 1, 3);
  TEST_SUB_PATTERN("a", "a", 0, 1, NULL, UNTOUCHED, UNTOUCHED);
  TEST_SUB_PATTERN("a", "a", 0, 1, NULL, UNTOUCHED, UNTOUCHED);
  TEST_SUB_PATTERN("(a)?(b)", "b", 0, 0, "b", 0, 1);
  TEST_SUB_PATTERN("(a)?(b)", "b", 0, 1, "", -1, -1);
  TEST_SUB_PATTERN("(a)?(b)", "b", 0, 2, "b", 0, 1);

  /* TEST_NAMED_SUB_PATTERN(pattern, string, start_position, sub_name,
   * 			    expected_sub, expected_start, expected_end) */
  TEST_NAMED_SUB_PATTERN("a(?P<A>.)(?P<B>.)?", "ab", 0, "A", "b", 1, 2);
  TEST_NAMED_SUB_PATTERN("a(?P<A>.)(?P<B>.)?", "aab", 1, "A", "b", 2, 3);
  TEST_NAMED_SUB_PATTERN("a(?P<A>.)(?P<B>.)?", EURO "ab", 0, "A", "b", 4, 5);
  TEST_NAMED_SUB_PATTERN("a(?P<A>.)(?P<B>.)?", EURO "ab", 0, "B", NULL, UNTOUCHED, UNTOUCHED);
  TEST_NAMED_SUB_PATTERN("a(?P<A>.)(?P<B>.)?", EURO "ab", 0, "C", NULL, UNTOUCHED, UNTOUCHED);
  TEST_NAMED_SUB_PATTERN("a(?P<A>.)(?P<B>.)?", "a" EGRAVE "x", 0, "A", EGRAVE, 1, 3);
  TEST_NAMED_SUB_PATTERN("a(?P<A>.)(?P<B>.)?", "a" EGRAVE "x", 0, "B", "x", 3, 4);
  TEST_NAMED_SUB_PATTERN("(?P<A>a)?(?P<B>b)", "b", 0, "A", "", -1, -1);
  TEST_NAMED_SUB_PATTERN("(?P<A>a)?(?P<B>b)", "b", 0, "B", "b", 0, 1);

  /* TEST_FETCH_ALL#(pattern, string, ...) */
  TEST_FETCH_ALL0("a", "");
  TEST_FETCH_ALL0("a", "b");
  TEST_FETCH_ALL1("a", "a", "a");
  TEST_FETCH_ALL1("a+", "aa", "aa");
  TEST_FETCH_ALL1("(?:a)", "a", "a");
  TEST_FETCH_ALL2("(a)", "a", "a", "a");
  TEST_FETCH_ALL2("a(.)", "ab", "ab", "b");
  TEST_FETCH_ALL2("a(.)", "a" HSTROKE, "a" HSTROKE, HSTROKE);
  TEST_FETCH_ALL3("(?:.*)(a)(.)", "xyazk", "xyaz", "a", "z");
  TEST_FETCH_ALL3("(?P<A>.)(a)", "xa", "xa", "x", "a");
  TEST_FETCH_ALL3("(?P<A>.)(a)", ENG "a", ENG "a", ENG, "a");
  TEST_FETCH_ALL3("(a)?(b)", "b", "b", "", "b");
  TEST_FETCH_ALL3("(a)?(b)", "ab", "ab", "a", "b");

  /* TEST_SPLIT_SIMPLE#(pattern, string, ...) */
  TEST_SPLIT_SIMPLE0("", "");
  TEST_SPLIT_SIMPLE0("a", "");
  TEST_SPLIT_SIMPLE1(",", "a", "a");
  TEST_SPLIT_SIMPLE1("(,)\\s*", "a", "a");
  TEST_SPLIT_SIMPLE2(",", "a,b", "a", "b");
  TEST_SPLIT_SIMPLE3(",", "a,b,c", "a", "b", "c");
  TEST_SPLIT_SIMPLE3(",\\s*", "a,b,c", "a", "b", "c");
  TEST_SPLIT_SIMPLE3(",\\s*", "a, b, c", "a", "b", "c");
  TEST_SPLIT_SIMPLE3("(,)\\s*", "a,b", "a", ",", "b");
  TEST_SPLIT_SIMPLE3("(,)\\s*", "a, b", "a", ",", "b");
  /* Not matched sub-strings. */
  TEST_SPLIT_SIMPLE2("a|(b)", "xay", "x", "y");
  TEST_SPLIT_SIMPLE3("a|(b)", "xby", "x", "b", "y");
  /* Empty matches. */
  TEST_SPLIT_SIMPLE3("", "abc", "a", "b", "c");
  TEST_SPLIT_SIMPLE3(" *", "ab c", "a", "b", "c");
  /* Invalid patterns. */
  TEST_SPLIT_SIMPLE0("\\", "");
  TEST_SPLIT_SIMPLE0("[", "");

  /* TEST_SPLIT#(pattern, string, start_position, max_tokens, ...) */
  TEST_SPLIT0("", "", 0, 0);
  TEST_SPLIT0("a", "", 0, 0);
  TEST_SPLIT0("a", "", 0, 1);
  TEST_SPLIT0("a", "", 0, 2);
  TEST_SPLIT0("a", "a", 1, 0);
  TEST_SPLIT1(",", "a", 0, 0, "a");
  TEST_SPLIT1(",", "a,b", 0, 1, "a,b");
  TEST_SPLIT1("(,)\\s*", "a", 0, 0, "a");
  TEST_SPLIT1(",", "a,b", 2, 0, "b");
  TEST_SPLIT2(",", "a,b", 0, 0, "a", "b");
  TEST_SPLIT2(",", "a,b,c", 0, 2, "a", "b,c");
  TEST_SPLIT2(",", "a,b", 1, 0, "", "b");
  TEST_SPLIT2(",", "a,", 0, 0, "a", "");
  TEST_SPLIT3(",", "a,b,c", 0, 0, "a", "b", "c");
  TEST_SPLIT3(",\\s*", "a,b,c", 0, 0, "a", "b", "c");
  TEST_SPLIT3(",\\s*", "a, b, c", 0, 0, "a", "b", "c");
  TEST_SPLIT3("(,)\\s*", "a,b", 0, 0, "a", ",", "b");
  TEST_SPLIT3("(,)\\s*", "a, b", 0, 0, "a", ",", "b");
  /* Not matched sub-strings. */
  TEST_SPLIT2("a|(b)", "xay", 0, 0, "x", "y");
  TEST_SPLIT3("a|(b)", "xby", 0, -1, "x", "b", "y");
  /* Empty matches. */
  TEST_SPLIT2(" *", "ab c", 1, 0, "b", "c");
  TEST_SPLIT3("", "abc", 0, 0, "a", "b", "c");
  TEST_SPLIT3(" *", "ab c", 0, 0, "a", "b", "c");
  TEST_SPLIT1(" *", "ab c", 0, 1, "ab c");
  TEST_SPLIT2(" *", "ab c", 0, 2, "a", "b c");
  TEST_SPLIT3(" *", "ab c", 0, 3, "a", "b", "c");
  TEST_SPLIT3(" *", "ab c", 0, 4, "a", "b", "c");

  /* TEST_SPLIT_NEXT#(pattern, string, start_position, ...) */
  TEST_SPLIT_NEXT1(",", "a", 0, "a");
  TEST_SPLIT_NEXT1("(,)\\s*", "a", 0, "a");
  TEST_SPLIT_NEXT1(",", "a,b", 2, "b");
  TEST_SPLIT_NEXT2(",", "a,b", 0, "a", "b");
  TEST_SPLIT_NEXT2(",", "a,b", 1, "", "b");
  TEST_SPLIT_NEXT2(",", "a,", 0, "a", "");
  TEST_SPLIT_NEXT3(",", "a,b,c", 0, "a", "b", "c");
  TEST_SPLIT_NEXT3(",\\s*", "a,b,c", 0, "a", "b", "c");
  TEST_SPLIT_NEXT3(",\\s*", "a, b, c", 0, "a", "b", "c");
  TEST_SPLIT_NEXT3("(,)\\s*", "a,b", 0, "a", ",", "b");
  TEST_SPLIT_NEXT3("(,)\\s*", "a, b", 0, "a", ",", "b");
  /* Not matched sub-strings. */
  TEST_SPLIT_NEXT2("a|(b)", "xay", 0, "x", "y");
  TEST_SPLIT_NEXT3("a|(b)", "xby", 0, "x", "b", "y");
  /* Empty matches. */
  TEST_SPLIT_NEXT2(" *", "ab c", 1, "b", "c");
  TEST_SPLIT_NEXT3("", "abc", 0, "a", "b", "c");
  TEST_SPLIT_NEXT3(" *", "ab c", 0, "a", "b", "c");

  /* TEST_EXPAND(pattern, string, string_to_expand, raw, expected) */
  TEST_EXPAND("a", "a", "", FALSE, "");
  TEST_EXPAND("a", "a", "\\0", FALSE, "a");
  TEST_EXPAND("a", "a", "\\1", FALSE, "");
  TEST_EXPAND("(a)", "ab", "\\1", FALSE, "a");
  TEST_EXPAND("(a)", "a", "\\1", FALSE, "a");
  TEST_EXPAND("(a)", "a", "\\g<1>", FALSE, "a");
  TEST_EXPAND("a", "a", "\\0130", FALSE, "X");
  TEST_EXPAND("a", "a", "\\\\\\0", FALSE, "\\a");
  TEST_EXPAND("a(?P<G>.)c", "xabcy", "X\\g<G>X", FALSE, "XbX");
  TEST_EXPAND("(.)(?P<1>.)", "ab", "\\1", FALSE, "a");
  TEST_EXPAND("(.)(?P<1>.)", "ab", "\\g<1>", FALSE, "a");
  TEST_EXPAND(".", EURO, "\\0", FALSE, EURO);
  TEST_EXPAND("(.)", EURO, "\\1", FALSE, EURO);
  TEST_EXPAND("(?P<G>.)", EURO, "\\g<G>", FALSE, EURO);
  TEST_EXPAND(".", "a", EURO, FALSE, EURO);
  TEST_EXPAND(".", "a", EURO "\\0", FALSE, EURO "a");
  TEST_EXPAND(".", "", "\\Lab\\Ec", FALSE, "abc");
  TEST_EXPAND(".", "", "\\LaB\\EC", FALSE, "abC");
  TEST_EXPAND(".", "", "\\Uab\\Ec", FALSE, "ABc");
  TEST_EXPAND(".", "", "a\\ubc", FALSE, "aBc");
  TEST_EXPAND(".", "", "a\\lbc", FALSE, "abc");
  TEST_EXPAND(".", "", "A\\uBC", FALSE, "ABC");
  TEST_EXPAND(".", "", "A\\lBC", FALSE, "AbC");
  TEST_EXPAND(".", "", "A\\l\\\\BC", FALSE, "A\\BC");
  TEST_EXPAND(".", "", "\\L" AGRAVE "\\E", FALSE, AGRAVE);
  TEST_EXPAND(".", "", "\\U" AGRAVE "\\E", FALSE, AGRAVE_UPPER);
  TEST_EXPAND(".", "", "\\u" AGRAVE "a", FALSE, AGRAVE_UPPER "a");
  TEST_EXPAND(".", "ab", "x\\U\\0y\\Ez", FALSE, "xAYz");
  TEST_EXPAND(".(.)", "AB", "x\\L\\1y\\Ez", FALSE, "xbyz");
  TEST_EXPAND(".", "ab", "x\\u\\0y\\Ez", FALSE, "xAyz");
  TEST_EXPAND(".(.)", "AB", "x\\l\\1y\\Ez", FALSE, "xbyz");
  TEST_EXPAND(".(.)", "a" AGRAVE_UPPER, "x\\l\\1y", FALSE, "x" AGRAVE "y");
  TEST_EXPAND("a", "bab", "\\x{61}", FALSE, "a");
  TEST_EXPAND("a", "bab", "\\x61", FALSE, "a");
  TEST_EXPAND("a", "bab", "\\x5a", FALSE, "Z");
  TEST_EXPAND("a", "bab", "\\0\\x5A", FALSE, "aZ");
  TEST_EXPAND("a", "bab", "\\1\\x{5A}", FALSE, "Z");
  TEST_EXPAND("a", "bab", "\\x{00E0}", FALSE, AGRAVE);
  TEST_EXPAND("", "bab", "\\x{0634}", FALSE, SHEEN);
  TEST_EXPAND("", "bab", "\\x{634}", FALSE, SHEEN);
  TEST_EXPAND("", "", "\\t", FALSE, "\t");
  TEST_EXPAND("", "", "\\v", FALSE, "\v");
  TEST_EXPAND("", "", "\\r", FALSE, "\r");
  TEST_EXPAND("", "", "\\n", FALSE, "\n");
  TEST_EXPAND("", "", "\\f", FALSE, "\f");
  TEST_EXPAND("", "", "\\a", FALSE, "\a");
  TEST_EXPAND("", "", "\\b", FALSE, "\b");
  TEST_EXPAND("a(.)", "abc", "\\0\\b\\1", FALSE, "ab\bb");
  TEST_EXPAND("a(.)", "abc", "\\0141", FALSE, "a");
  TEST_EXPAND("a(.)", "abc", "\\078", FALSE, "\a8");
  TEST_EXPAND("a(.)", "abc", "\\077", FALSE, "?");
  TEST_EXPAND("a(.)", "abc", "\\0778", FALSE, "?8");
  TEST_EXPAND("a(.)", "a" AGRAVE "b", "\\1", FALSE, AGRAVE);
  TEST_EXPAND("a(.)", "a" AGRAVE "b", "\\1", TRUE, "\xc3");
  TEST_EXPAND("a(.)", "a" AGRAVE "b", "\\0", TRUE, "a\xc3");
  /* Invalid strings. */
  TEST_EXPAND("", "", "\\Q", FALSE, NULL);
  TEST_EXPAND("", "", "x\\Ay", FALSE, NULL);
  TEST_EXPAND("", "", "\\g<", FALSE, NULL);
  TEST_EXPAND("", "", "\\g<>", FALSE, NULL);
  TEST_EXPAND("", "", "\\g<1a>", FALSE, NULL);
  TEST_EXPAND("", "", "\\g<a$>", FALSE, NULL);
  TEST_EXPAND("", "", "\\", FALSE, NULL);
  TEST_EXPAND("a", "a", "\\x{61", FALSE, NULL);
  TEST_EXPAND("a", "a", "\\x6X", FALSE, NULL);

  /* TEST_REPLACE(pattern, string, start_position, replacement, expected) */
  TEST_REPLACE("a", "ababa", 0, "A", "AbAbA");
  TEST_REPLACE("a", "ababa", 1, "A", "abAbA");
  TEST_REPLACE("a", "ababa", 2, "A", "abAbA");
  TEST_REPLACE("a", "ababa", 3, "A", "ababA");
  TEST_REPLACE("a", "ababa", 4, "A", "ababA");
  TEST_REPLACE("a", "abababa", 2, "A", "abAbAbA");
  TEST_REPLACE("$^", "abc", 0, "X", "abc");
  TEST_REPLACE("(.)a", "ciao", 0, "a\\1", "caio");
  TEST_REPLACE("a.", "abc", 0, "\\0\\0", "ababc");
  TEST_REPLACE("a", "asd", 0, "\\0101", "Asd");
  TEST_REPLACE("(a).\\1", "aba cda", 0, "\\1\\n", "a\n cda");
  TEST_REPLACE("a" AGRAVE "a", "a" AGRAVE "a", 0, "x", "x");
  TEST_REPLACE("a" AGRAVE "a", "a" AGRAVE "a", 0, OGRAVE, OGRAVE);
  TEST_REPLACE("[^-]", "-" EURO "-x-" HSTROKE, 0, "a", "-a-a-a");
  TEST_REPLACE("[^-]", "-" EURO "-" HSTROKE, 0, "a\\g<0>a", "-a" EURO "a-a" HSTROKE "a");
  TEST_REPLACE("-", "-" EURO "-" HSTROKE, 0, "", EURO HSTROKE);
  TEST_REPLACE(".*", "hello", 0, "\\U\\0\\E", "HELLO");
  TEST_REPLACE(".*", "hello", 0, "\\u\\0", "Hello");
  TEST_REPLACE("\\S+", "hello world", 0, "\\U-\\0-", "-HELLO- -WORLD-");
  TEST_REPLACE(".", "a", 0, "\\A", NULL);
  TEST_REPLACE(".", "a", 0, "\\g", NULL);

  /* TEST_REPLACE_LIT(pattern, string, start_position, replacement, expected) */
  TEST_REPLACE_LIT("a", "ababa", 0, "A", "AbAbA");
  TEST_REPLACE_LIT("a", "ababa", 1, "A", "abAbA");
  TEST_REPLACE_LIT("a", "ababa", 2, "A", "abAbA");
  TEST_REPLACE_LIT("a", "ababa", 3, "A", "ababA");
  TEST_REPLACE_LIT("a", "ababa", 4, "A", "ababA");
  TEST_REPLACE_LIT("a", "abababa", 2, "A", "abAbAbA");
  TEST_REPLACE_LIT("a", "abcadaa", 0, "A", "AbcAdAA");
  TEST_REPLACE_LIT("$^", "abc", 0, "X", "abc");
  TEST_REPLACE_LIT("(.)a", "ciao", 0, "a\\1", "ca\\1o");
  TEST_REPLACE_LIT("a.", "abc", 0, "\\0\\0\\n", "\\0\\0\\nc");
  TEST_REPLACE_LIT("a" AGRAVE "a", "a" AGRAVE "a", 0, "x", "x");
  TEST_REPLACE_LIT("a" AGRAVE "a", "a" AGRAVE "a", 0, OGRAVE, OGRAVE);
  TEST_REPLACE_LIT(AGRAVE, "-" AGRAVE "-" HSTROKE, 0, "a" ENG "a", "-a" ENG "a-" HSTROKE);
  TEST_REPLACE_LIT("[^-]", "-" EURO "-" AGRAVE "-" HSTROKE, 0, "a", "-a-a-a");
  TEST_REPLACE_LIT("[^-]", "-" EURO "-" AGRAVE, 0, "a\\g<0>a", "-a\\g<0>a-a\\g<0>a");
  TEST_REPLACE_LIT("-", "-" EURO "-" AGRAVE "-" HSTROKE, 0, "", EURO AGRAVE HSTROKE);

  /* TEST_GET_STRING_NUMBER(pattern, name, expected_num) */
  TEST_GET_STRING_NUMBER("", "A", -1);
  TEST_GET_STRING_NUMBER("(?P<A>.)", "A", 1);
  TEST_GET_STRING_NUMBER("(?P<A>.)", "B", -1);
  TEST_GET_STRING_NUMBER("(?P<A>.)(?P<B>a)", "A", 1);
  TEST_GET_STRING_NUMBER("(?P<A>.)(?P<B>a)", "B", 2);
  TEST_GET_STRING_NUMBER("(?P<A>.)(?P<B>a)", "C", -1);
  TEST_GET_STRING_NUMBER("(?P<A>.)(.)(?P<B>a)", "A", 1);
  TEST_GET_STRING_NUMBER("(?P<A>.)(.)(?P<B>a)", "B", 3);
  TEST_GET_STRING_NUMBER("(?P<A>.)(.)(?P<B>a)", "C", -1);
  TEST_GET_STRING_NUMBER("(?:a)(?P<A>.)", "A", 1);
  TEST_GET_STRING_NUMBER("(?:a)(?P<A>.)", "B", -1);

  /* TEST_ESCAPE(string, length, expected) */
  TEST_ESCAPE("hello world", -1, "hello world");
  TEST_ESCAPE("hello world", 5, "hello");
  TEST_ESCAPE("hello.world", -1, "hello\\.world");
  TEST_ESCAPE("a(b\\b.$", -1, "a\\(b\\\\b\\.\\$");
  TEST_ESCAPE("hello\0world", -1, "hello");
  TEST_ESCAPE("hello\0world", 11, "hello\\0world");
  TEST_ESCAPE(EURO "*" ENG, -1, EURO "\\*" ENG);
  TEST_ESCAPE("a$", -1, "a\\$");
  TEST_ESCAPE("$a", -1, "\\$a");
  TEST_ESCAPE("a$a", -1, "a\\$a");
  TEST_ESCAPE("$a$", -1, "\\$a\\$");
  TEST_ESCAPE("$a$", 0, "");
  TEST_ESCAPE("$a$", 1, "\\$");
  TEST_ESCAPE("$a$", 2, "\\$a");
  TEST_ESCAPE("$a$", 3, "\\$a\\$");
  TEST_ESCAPE("$a$", 4, "\\$a\\$\\0");
  TEST_ESCAPE("|()[]{}^$*+?.", -1, "\\|\\(\\)\\[\\]\\{\\}\\^\\$\\*\\+\\?\\.");
  TEST_ESCAPE("a|a(a)a[a]a{a}a^a$a*a+a?a.a", -1,
	      "a\\|a\\(a\\)a\\[a\\]a\\{a\\}a\\^a\\$a\\*a\\+a\\?a\\.a");

  /* TEST_MATCH_ALL#(pattern, string, string_len, start_position, ...) */
  TEST_MATCH_ALL0("<.*>", "", -1, 0);
  TEST_MATCH_ALL0("a+", "", -1, 0);
  TEST_MATCH_ALL0("a+", "a", 0, 0);
  TEST_MATCH_ALL0("a+", "a", -1, 1);
  TEST_MATCH_ALL1("<.*>", "<a>", -1, 0, "<a>", 0, 3);
  TEST_MATCH_ALL1("a+", "a", -1, 0, "a", 0, 1);
  TEST_MATCH_ALL1("a+", "aa", 1, 0, "a", 0, 1);
  TEST_MATCH_ALL1("a+", "aa", -1, 1, "a", 1, 2);
  TEST_MATCH_ALL1("a+", "aa", 2, 1, "a", 1, 2);
  TEST_MATCH_ALL1(".+", ENG, -1, 0, ENG, 0, 2);
  TEST_MATCH_ALL2("<.*>", "<a><b>", -1, 0, "<a><b>", 0, 6, "<a>", 0, 3);
  TEST_MATCH_ALL2("a+", "aa", -1, 0, "aa", 0, 2, "a", 0, 1);
  TEST_MATCH_ALL2(".+", ENG EURO, -1, 0, ENG EURO, 0, 5, ENG, 0, 2);
  TEST_MATCH_ALL3("<.*>", "<a><b><c>", -1, 0, "<a><b><c>", 0, 9,
		  "<a><b>", 0, 6, "<a>", 0, 3);
  TEST_MATCH_ALL3("a+", "aaa", -1, 0, "aaa", 0, 3, "aa", 0, 2, "a", 0, 1);

  /* TEST_NULL_MATCH(code) */
  /* TEST_NULL_MATCH_RET(code, expected, type) */
  /* Test to see what happens if a function needing GRegexMatch is called
   * when GRegexMatch is NULL. The result should be the same when the function
   * is called after g_regex_clear.
   * "re" is a GRegex, the pattern is "a". */
  TEST_NULL_MATCH(g_regex_clear (re));
  TEST_NULL_MATCH(g_regex_get_pattern (re));
  TEST_NULL_MATCH_RET(g_regex_optimize (re, NULL), TRUE, gboolean, "%d");
  TEST_NULL_MATCH_RET(g_regex_match (re, "a", 0), TRUE, gboolean, "%d");
  TEST_NULL_MATCH_RET(g_regex_match (re, "b", 0), FALSE, gboolean, "%d");
  TEST_NULL_MATCH_RET(g_regex_match_full (re, "a", -1, 0, 0, NULL), TRUE, gboolean, "%d");
  TEST_NULL_MATCH_RET(g_regex_match_full (re, "a", -1, 1, 0, NULL), FALSE, gboolean, "%d");
  TEST_NULL_MATCH_RET(g_regex_match_full (re, "b", -1, 0, 0, NULL), FALSE, gboolean, "%d");
  TEST_NULL_MATCH_RET(g_regex_get_match_count (re), -1, gint, "%d");
  TEST_NULL_MATCH_RET(g_regex_is_partial_match (re), FALSE, gboolean, "%d");
  TEST_NULL_MATCH_RET(g_regex_fetch (re, 0, "abc"), NULL, gchar *, "%p");
  TEST_NULL_MATCH_RET(g_regex_fetch_pos (re, 0, NULL, NULL), FALSE, gboolean, "%d");
  TEST_NULL_MATCH_RET(g_regex_fetch_all (re, "b"), NULL, gchar **, "%p");
  TEST_NULL_MATCH_RET(g_regex_get_string_number (re, "X"), -1, gint, "%d");

end: /* if abort_on_fail is TRUE the flow passes to this label. */
  verbose ("\n%u tests passed, %u failed\n", passed, failed);
  return failed;
}

#else /* ENABLE_REGEX false */

int
main (int argc, char *argv[])
{
  g_print ("GRegex is disabled.\n");
  return 0;
}

#endif /* ENABLE_REGEX */
