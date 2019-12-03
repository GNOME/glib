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
  len = wcslen (commandline);

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
              while (first_argument < &commandline[len] && first_argument[0] == L' ')
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
