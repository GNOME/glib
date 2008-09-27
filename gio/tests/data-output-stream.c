/* GLib testing framework examples and tests
 * Copyright (C) 2008 Red Hat, Inc.
 * Authors: Tomas Bzatek <tbzatek@redhat.com>
 *
 * This work is provided "as is"; redistribution and modification
 * in whole or in part, in any medium, physical or electronic is
 * permitted without restriction.
 *
 * This work is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * In no event shall the authors or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 */

#include <glib/glib.h>
#include <gio/gio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINES		0xFFF	
#define MAX_LINES_BUFF 		0xFFFFFF
#define MAX_BYTES_BINARY	0x100	

static void
test_read_lines (GDataStreamNewlineType newline_type)
{
  GOutputStream *stream;
  GOutputStream *base_stream;
  GError *error = NULL;
  gpointer data;
  char *lines;
  int size;
  int i;

#define TEST_STRING	"some_text"
  
  const char* endl[4] = {"\n", "\r", "\r\n", "\n"};
  
  
  data = g_malloc0 (MAX_LINES_BUFF);
  lines = g_malloc0 ((strlen (TEST_STRING) + strlen (endl[newline_type])) * MAX_LINES + 1);
  
  /* initialize objects */
  base_stream = g_memory_output_stream_new (data, MAX_LINES_BUFF, NULL, NULL);
  stream = G_OUTPUT_STREAM (g_data_output_stream_new (base_stream));

  
  /*  fill data */
  for (i = 0; i < MAX_LINES; i++)
    {
      gboolean res;
      char *s = g_strconcat (TEST_STRING, endl[newline_type], NULL);
      res = g_data_output_stream_put_string (G_DATA_OUTPUT_STREAM (stream), s, NULL, &error);
      g_stpcpy ((char*)(lines + i*strlen(s)), s);
      g_assert_no_error (error);
      g_assert (res == TRUE);
    }

  /*  Byte order testing */
  g_data_output_stream_set_byte_order (G_DATA_OUTPUT_STREAM (stream), G_DATA_STREAM_BYTE_ORDER_BIG_ENDIAN);
  g_assert_cmpint (g_data_output_stream_get_byte_order (G_DATA_OUTPUT_STREAM (stream)), ==, G_DATA_STREAM_BYTE_ORDER_BIG_ENDIAN);
  g_data_output_stream_set_byte_order (G_DATA_OUTPUT_STREAM (stream), G_DATA_STREAM_BYTE_ORDER_LITTLE_ENDIAN);
  g_assert_cmpint (g_data_output_stream_get_byte_order (G_DATA_OUTPUT_STREAM (stream)), ==, G_DATA_STREAM_BYTE_ORDER_LITTLE_ENDIAN);
  
  /*  compare data */
  size = strlen (data);
  g_assert_cmpint (size, <, MAX_LINES_BUFF);
  g_assert_cmpstr ((char*)data, ==, lines);
  
  g_object_unref (base_stream);
  g_object_unref (stream);
  g_free (data);
  g_free (lines);
}

static void
test_read_lines_LF (void)
{
  test_read_lines (G_DATA_STREAM_NEWLINE_TYPE_LF);
}

static void
test_read_lines_CR (void)
{
  test_read_lines (G_DATA_STREAM_NEWLINE_TYPE_CR);
}

static void
test_read_lines_CR_LF (void)
{
  test_read_lines (G_DATA_STREAM_NEWLINE_TYPE_CR_LF);
}

enum TestDataType {
  TEST_DATA_BYTE = 0,
  TEST_DATA_INT16,
  TEST_DATA_UINT16,
  TEST_DATA_INT32,
  TEST_DATA_UINT32,
  TEST_DATA_INT64,
  TEST_DATA_UINT64
};

#define TEST_DATA_RETYPE(a, v)	\
	(a == TEST_DATA_BYTE	? (guchar)v : \
	(a == TEST_DATA_INT16	? (gint16)v : \
	(a == TEST_DATA_UINT16	? (guint16)v : \
	(a == TEST_DATA_INT32	? (gint32)v : \
	(a == TEST_DATA_UINT32	? (guint32)v : \
	(a == TEST_DATA_INT64	? (gint64)v : \
	 (guint64)v )))))) 

#define TEST_DATA_RETYPE_BUFF(a, v)	\
	(a == TEST_DATA_BYTE	? *(guchar*)v : \
	(a == TEST_DATA_INT16	? *(gint16*)v : \
	(a == TEST_DATA_UINT16	? *(guint16*)v : \
	(a == TEST_DATA_INT32	? *(gint32*)v : \
	(a == TEST_DATA_UINT32	? *(guint32*)v : \
	(a == TEST_DATA_INT64	? *(gint64*)v : \
	 *(guint64*)v )))))) 




static void
test_data_array (gpointer buffer, int len,
		 enum TestDataType data_type, GDataStreamByteOrder byte_order)
{
  GOutputStream *stream;
  GOutputStream *base_stream;
  gpointer stream_data;
  
  GError *error = NULL;
  int pos;
  int data_size = 1;
  GDataStreamByteOrder native;
  gboolean swap;
  gboolean res;
  guint64 data;
  
  /*  create objects */
  stream_data = g_malloc0 (len);
  base_stream = g_memory_output_stream_new (stream_data, len, NULL, NULL);
  stream = G_OUTPUT_STREAM (g_data_output_stream_new (base_stream));
  g_data_output_stream_set_byte_order (G_DATA_OUTPUT_STREAM (stream), byte_order);
  

  /*  Set correct data size */
  switch (data_type)
    {
    case TEST_DATA_BYTE:
      data_size = 1;
      break;
    case TEST_DATA_INT16:
    case TEST_DATA_UINT16:
      data_size = 2;
      break;
    case TEST_DATA_INT32:
    case TEST_DATA_UINT32:
      data_size = 4;
      break;
    case TEST_DATA_INT64:
    case TEST_DATA_UINT64:
      data_size = 8;
      break; 
    }
	
  /*  Set flag to swap bytes if needed */
  native = (G_BYTE_ORDER == G_BIG_ENDIAN) ? G_DATA_STREAM_BYTE_ORDER_BIG_ENDIAN : G_DATA_STREAM_BYTE_ORDER_LITTLE_ENDIAN;
  swap = (byte_order != G_DATA_STREAM_BYTE_ORDER_HOST_ENDIAN) && (byte_order != native);

  /*  Write data to the file */
  pos = 0;
  while (pos < len)
    {
      switch (data_type)
	{
	case TEST_DATA_BYTE:
	  res = g_data_output_stream_put_byte (G_DATA_OUTPUT_STREAM (stream), TEST_DATA_RETYPE_BUFF (data_type, ((guchar*)buffer + pos)), NULL, &error);
	  break;
	case TEST_DATA_INT16:
	  res = g_data_output_stream_put_int16 (G_DATA_OUTPUT_STREAM (stream), TEST_DATA_RETYPE_BUFF (data_type, ((guchar*)buffer + pos)), NULL, &error);
	  break;
	case TEST_DATA_UINT16:
	  res = g_data_output_stream_put_uint16 (G_DATA_OUTPUT_STREAM (stream), TEST_DATA_RETYPE_BUFF (data_type, ((guchar*)buffer + pos)), NULL, &error);
	  break;
	case TEST_DATA_INT32:
	  res = g_data_output_stream_put_int32 (G_DATA_OUTPUT_STREAM (stream), TEST_DATA_RETYPE_BUFF (data_type, ((guchar*)buffer + pos)), NULL, &error);
	  break;
	case TEST_DATA_UINT32:
	  res = g_data_output_stream_put_uint32 (G_DATA_OUTPUT_STREAM (stream), TEST_DATA_RETYPE_BUFF (data_type, ((guchar*)buffer + pos)), NULL, &error);
	  break;
	case TEST_DATA_INT64:
	  res = g_data_output_stream_put_int64 (G_DATA_OUTPUT_STREAM (stream), TEST_DATA_RETYPE_BUFF (data_type, ((guchar*)buffer + pos)), NULL, &error);
	  break;
	case TEST_DATA_UINT64:
	  res = g_data_output_stream_put_uint64 (G_DATA_OUTPUT_STREAM (stream), TEST_DATA_RETYPE_BUFF (data_type, ((guchar*)buffer + pos)), NULL, &error);
	  break;
	}
      g_assert_no_error (error);
      g_assert_cmpint (res, ==, TRUE);
      pos += data_size;
    }
  
	
  /*  Compare data back */
  pos = 0;
  data = 0;
  while (pos < len)
    {
      data = TEST_DATA_RETYPE_BUFF(data_type, ((guchar*)stream_data + pos));
      if (swap)
	{
	  switch (data_type)
	    {
	    case TEST_DATA_BYTE:  
	      break;
	    case TEST_DATA_UINT16:
	    case TEST_DATA_INT16:
	      data = TEST_DATA_RETYPE(data_type, GUINT16_SWAP_LE_BE(TEST_DATA_RETYPE(data_type, data)));
	      break;
	    case TEST_DATA_UINT32:
	    case TEST_DATA_INT32:
	      data = TEST_DATA_RETYPE(data_type, GUINT32_SWAP_LE_BE(TEST_DATA_RETYPE(data_type, data)));
	      break;
	    case TEST_DATA_UINT64:
	    case TEST_DATA_INT64:
	      data = TEST_DATA_RETYPE(data_type, GUINT64_SWAP_LE_BE(TEST_DATA_RETYPE(data_type, data)));
	      break;
	    }
	}
      g_assert_cmpint (data, ==, TEST_DATA_RETYPE_BUFF(data_type, ((guchar*)buffer + pos)));
      break;
      
      pos += data_size;
    }
  
  g_object_unref (base_stream);
  g_object_unref (stream);
  g_free (stream_data);
}

static void
test_read_int (void)
{
  GRand *rand;
  gpointer buffer;
  int i;
  
  rand = g_rand_new ();
  buffer = g_malloc0(MAX_BYTES_BINARY);
  
  /*  Fill in some random data */
  for (i = 0; i < MAX_BYTES_BINARY; i++)
    {
      guchar x = 0;
      while (! x)  x = (guchar)g_rand_int (rand);
      *(guchar*)((guchar*)buffer + sizeof (guchar) * i) = x; 
    }

  for (i = 0; i < 3; i++)
    {
      int j;
      for (j = 0; j <= TEST_DATA_UINT64; j++)
	test_data_array (buffer, MAX_BYTES_BINARY, j, i);
    }
  
  g_rand_free (rand);
  g_free (buffer);
}

int
main (int   argc,
      char *argv[])
{
  g_type_init ();
  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/data-input-stream/read-lines-LF", test_read_lines_LF);
  g_test_add_func ("/data-input-stream/read-lines-CR", test_read_lines_CR);
  g_test_add_func ("/data-input-stream/read-lines-CR-LF", test_read_lines_CR_LF);
  g_test_add_func ("/data-input-stream/read-int", test_read_int);

  return g_test_run();
}
