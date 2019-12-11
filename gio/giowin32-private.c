static gsize
g_utf16_len (const gunichar2 *str)
{
  gsize result;

  for (result = 0; str[0] != 0; str++, result++)
    ;

  return result;
}

static gunichar2 *
g_wcsdup (const gunichar2 *str, gssize str_size)
{
  if (str_size == -1)
    {
      str_size = g_utf16_len (str) + 1;
      str_size *= sizeof (gunichar2);
    }

  return g_memdup (str, str_size);
}

static const gunichar2 *
g_utf16_wchr (const gunichar2 *str, const wchar_t wchr)
{
  for (; str != NULL && str[0] != 0; str++)
    if ((wchar_t) str[0] == wchr)
      return str;

  return NULL;
}

static gboolean
g_utf16_to_utf8_and_fold (const gunichar2  *str,
                          gchar           **str_u8,
                          gchar           **str_u8_folded)
{
  gchar *u8;
  gchar *folded;
  u8 = g_utf16_to_utf8 (str, -1, NULL, NULL, NULL);

  if (u8 == NULL)
    return FALSE;

  folded = g_utf8_casefold (u8, -1);

  if (str_u8)
    *str_u8 = u8;
  else
    g_free (u8);

  if (str_u8_folded)
    *str_u8_folded = folded;
  else
    g_free (folded);

  return TRUE;
}

/* Make sure @commandline is a valid UTF-16 string before
 * calling this function!
 * follow_class_chain_to_handler() does perform such validation.
 */
static void
_g_win32_extract_executable (gunichar2  *commandline,
                             gchar     **ex_out,
                             gchar     **ex_basename_out,
                             gchar     **ex_folded_out,
                             gchar     **ex_folded_basename_out,
                             gchar     **dll_function_out)
{
  gchar *ex;
  gchar *ex_folded;
  gunichar2 *p;
  gunichar2 *first_argument;
  gboolean quoted;
  size_t len;
  size_t execlen;
  gunichar2 *exepart;
  gboolean found;

  quoted = FALSE;
  execlen = 0;
  found = FALSE;
  len = g_utf16_len (commandline);
  p = commandline;
  first_argument = NULL;

  while (p < &commandline[len])
    {
      switch ((wchar_t) p[0])
        {
        case L'"':
          quoted = !quoted;
          break;
        case L' ':
          if (!quoted)
            {
              execlen = p - commandline;
              first_argument = p;
              while ((wchar_t) first_argument[0] == L' ')
                first_argument++;
              p = &commandline[len];
              found = TRUE;
            }
          break;
        default:
          break;
        }
      p += 1;
    }

  if (!found)
    execlen = len;

  exepart = g_wcsdup (commandline, (execlen + 1) * sizeof (gunichar2));
  exepart[execlen] = 0;

  p = &exepart[0];

  while (execlen > 0 && exepart[0] == L'"' && exepart[execlen - 1] == L'"')
    {
      p = &exepart[1];
      exepart[execlen - 1] = 0;
      execlen -= 2;
    }

  if (!g_utf16_to_utf8_and_fold (p, &ex, &ex_folded))
    /* Currently no code to handle this case. It shouldn't happen though... */
    g_assert_not_reached ();

  g_free (exepart);

  if (dll_function_out)
    *dll_function_out = NULL;

  /* See if the executable basename is "rundll32.exe". If so, then
   * parse the rest of the commandline as r'"?path-to-dll"?[ ]*,*[ ]*dll_function_to_invoke'
   */
  /* Using just "rundll32.exe", without an absolute path, seems
   * very exploitable, but MS does that sometimes, so we have
   * to accept that.
   */
  if ((g_strcmp0 (ex_folded, "rundll32.exe") == 0 ||
       g_str_has_suffix (ex_folded, "\\rundll32.exe") ||
       g_str_has_suffix (ex_folded, "/rundll32.exe")) &&
      first_argument != NULL &&
      dll_function_out != NULL)
    {
      /* Corner cases:
       * > rundll32.exe c:\some,file,with,commas.dll,some_function
       * is treated by rundll32 as:
       * dll=c:\some
       * function=file,with,commas.dll,some_function
       * unless the dll name is surrounded by double quotation marks:
       * > rundll32.exe "c:\some,file,with,commas.dll",some_function
       * in which case everything works normally.
       * Also, quoting only works if it surrounds the file name, i.e:
       * > rundll32.exe "c:\some,file"",with,commas.dll",some_function
       * will not work.
       * Also, comma is optional when filename is quoted or when function
       * name is separated from the filename by space(s):
       * > rundll32.exe "c:\some,file,with,commas.dll"some_function
       * will work,
       * > rundll32.exe c:\some_dll_without_commas_or_spaces.dll some_function
       * will work too.
       * Also, any number of commas is accepted:
       * > rundll32.exe c:\some_dll_without_commas_or_spaces.dll , , ,,, , some_function
       * both work just fine.
       * Good job, Microsoft!
       */
      gunichar2 *filename_end = NULL;
      size_t filename_len = 0;
      gunichar2 *function_begin = NULL;
      size_t function_len = 0;
      gunichar2 *dllpart;
      quoted = FALSE;
      p = first_argument;

      if ((wchar_t) p[0] == L'"')
        {
          quoted = TRUE;
          p += 1;
        }

      while (p < &commandline[len])
        {
          switch ((wchar_t) p[0])
            {
            case L'"':
              if (quoted)
                {
                  filename_end = p;
                  p = &commandline[len];
                }
              break;
            case L' ':
            case L',':
              if (!quoted)
                {
                  filename_end = p;
                  p = &commandline[len];
                }
              break;
            default:
              filename_len += 1;
              break;
            }
          p += 1;
        }

      if (filename_end == NULL && !quoted)
        filename_end = &commandline[len];

      if (filename_end != NULL && filename_len > 0)
        {
          function_begin = filename_end;
          if (quoted)
            function_begin += 1;
          while ((wchar_t) function_begin[0] == L',' || (wchar_t) function_begin[0] == L' ')
            function_begin += 1;
          if (function_begin[0] != 0)
            {
              gchar *dllpart_utf8;
              gchar *dllpart_utf8_folded;
              gchar *function_utf8;
              const gunichar2 *space = g_utf16_wchr (function_begin, L' ');

              if (space)
                function_len = space - function_begin;
              else
                function_len = g_utf16_len (function_begin);

              p = first_argument;
              filename_len = filename_end - first_argument;

              if (quoted)
                {
                  p += 1;
                  filename_len -= 1;
                }

              dllpart = g_wcsdup (p, (filename_len + 1) * sizeof (gunichar2));
              dllpart[filename_len] = 0;

              if (!g_utf16_to_utf8_and_fold (dllpart, &dllpart_utf8, &dllpart_utf8_folded))
                g_assert_not_reached ();

              function_utf8 = g_utf16_to_utf8 (function_begin, function_len, NULL, NULL, NULL);
              
              g_debug ("Dismissing wrapper `%s', using `%s' as the executable DLL that runs %s",
                       ex,
                       dllpart_utf8,
                       function_utf8);

              if (dll_function_out)
                *dll_function_out = function_utf8;
              else
                g_free (function_utf8);

              /*
               * Free our previous output candidate (rundll32) and replace it with the DLL path,
               * then proceed forward as if nothing has changed.
               */
              g_free (ex);
              g_free (ex_folded);

              ex = dllpart_utf8;
              ex_folded = dllpart_utf8_folded;

              g_free (dllpart);
            }
        }
    }

  if (ex_out)
    {
      *ex_out = ex;

      if (ex_basename_out)
        {
          *ex_basename_out = &ex[strlen (ex) - 1];

          while (*ex_basename_out > ex)
            {
              if ((*ex_basename_out)[0] == '/' ||
                  (*ex_basename_out)[0] == '\\')
                {
                  *ex_basename_out += 1;
                  break;
                }

              *ex_basename_out -= 1;
            }
        }
    }
  else
    {
      g_free (ex);
    }

  if (ex_folded_out)
    {
      *ex_folded_out = ex_folded;

      if (ex_folded_basename_out)
        {
          *ex_folded_basename_out = &ex_folded[strlen (ex_folded) - 1];

          while (*ex_folded_basename_out > ex_folded)
            {
              if ((*ex_folded_basename_out)[0] == '/' ||
                  (*ex_folded_basename_out)[0] == '\\')
                {
                  *ex_folded_basename_out += 1;
                  break;
                }

              *ex_folded_basename_out -= 1;
            }
        }
    }
  else
    {
      g_free (ex_folded);
    }
}

/**
 * rundll32 accepts many different commandlines. Among them is this:
 * > rundll32.exe "c:/program files/foo/bar.dll",,, , ,,,, , function_name %1
 * rundll32 just reads the first argument as a potentially quoted
 * filename until the quotation ends (if quoted) or until a comma,
 * or until a space. Then ignores all subsequent spaces (if any) and commas (if any;
 * at least one comma is mandatory only if the filename is not quoted),
 * and then interprets the rest of the commandline (until a space or a NUL-byte)
 * as a name of a function.
 * When GLib tries to run a program, it attempts to correctly re-quote the arguments,
 * turning the first argument into "c:/program files/foo/bar.dll,,,".
 * This breaks rundll32 parsing logic.
 * Try to work around this by ensuring that the syntax is like this:
 * > rundll32.exe "c:/program files/foo/bar.dll" function_name
 * This syntax is valid for rundll32 *and* GLib spawn routines won't break it.
 *
 * @commandline must have at least 2 arguments, and the second argument
 * must contain a (possibly quoted) filename, followed by a space or
 * a comma. This can be checked for with an extract_executable() call -
 * it should return a non-null dll_function.
 */
static void
_g_win32_fixup_broken_microsoft_rundll_commandline (gunichar2 *commandline)
{
  size_t len;
  gunichar2 *p;
  gunichar2 *first_argument = NULL;
  gboolean quoted;
  gunichar2 *first_char_after_filename = NULL;

  p = commandline;
  quoted = FALSE;
  len = g_utf16_len (commandline);

  while (p < &commandline[len])
    {
      switch (p[0])
        {
        case L'"':
          quoted = !quoted;
          break;
        case L' ':
          if (!quoted)
            {
              first_argument = p;
              while (first_argument[0] == L' ')
                first_argument++;
              p = &commandline[len];
            }
          break;
        default:
          break;
        }
      p += 1;
    }

  g_assert (first_argument != NULL);

  quoted = FALSE;
  p = first_argument;

  if (p[0] == L'"')
    {
      quoted = TRUE;
      p += 1;
    }

  while (p < &commandline[len])
    {
      switch (p[0])
        {
        case L'"':
          if (quoted)
            {
              first_char_after_filename = &p[1];
              p = &commandline[len];
            }
          break;
        case L' ':
        case L',':
          if (!quoted)
            {
              first_char_after_filename = p;
              p = &commandline[len];
            }
          break;
        default:
          break;
        }
      p += 1;
    }

  g_assert (first_char_after_filename != NULL);

  if (first_char_after_filename[0] == L',')
    first_char_after_filename[0] = L' ';
  /* Else everything is ok (first char after filename is ' ' or the first char
   * of the function name - either way this will work).
   */
}
