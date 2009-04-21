#include <string.h>

#include "girepository.h"
#include "girffi.h"

/**
 * g_field_info_get_field:
 * @field_info: a #GIFieldInfo
 * @mem: pointer to a block of memory representing a C structure or union
 * @value: a #GArgument into which to store the value retrieved
 *
 * Reads a field identified by a #GFieldInfo from a C structure or
 * union.  This only handles fields of simple C types. It will fail
 * for a field of a composite type like a nested structure or union
 * even if that is actually readable.
 *
 * Returns: %TRUE if reading the field succeeded, otherwise %FALSE
 */
gboolean
g_field_info_get_field (GIFieldInfo *field_info,
			gpointer     mem,
			GArgument   *value)
{
    int offset;
    GITypeInfo *type_info;
    gboolean result = FALSE;

    if ((g_field_info_get_flags (field_info) & GI_FIELD_IS_READABLE) == 0)
      return FALSE;

    offset = g_field_info_get_offset (field_info);
    type_info = g_field_info_get_type (field_info);

    if (g_type_info_is_pointer (type_info))
      {
	value->v_pointer = G_STRUCT_MEMBER(gpointer, mem, offset);
	result = TRUE;
      }
    else
      {
	switch (g_type_info_get_tag (type_info))
	  {
	  case GI_TYPE_TAG_VOID:
	    g_warning("Field %s: should not be have void type",
		      g_base_info_get_name ((GIBaseInfo *)field_info));
	    break;
	  case GI_TYPE_TAG_BOOLEAN:
	    value->v_boolean = G_STRUCT_MEMBER(gboolean, mem, offset) != FALSE;
	    result = TRUE;
	    break;
	  case GI_TYPE_TAG_INT8:
	  case GI_TYPE_TAG_UINT8:
	    value->v_uint8 = G_STRUCT_MEMBER(guint8, mem, offset);
	    result = TRUE;
	    break;
	  case GI_TYPE_TAG_INT16:
	  case GI_TYPE_TAG_UINT16:
	  case GI_TYPE_TAG_SHORT:
	  case GI_TYPE_TAG_USHORT:
	    value->v_uint16 = G_STRUCT_MEMBER(guint16, mem, offset);
	    result = TRUE;
	    break;
	  case GI_TYPE_TAG_INT32:
	  case GI_TYPE_TAG_UINT32:
	  case GI_TYPE_TAG_INT:
	  case GI_TYPE_TAG_UINT:
	    value->v_uint32 = G_STRUCT_MEMBER(guint32, mem, offset);
	    result = TRUE;
	    break;
	  case GI_TYPE_TAG_INT64:
	  case GI_TYPE_TAG_UINT64:
	    value->v_uint64 = G_STRUCT_MEMBER(guint64, mem, offset);
	    result = TRUE;
	    break;
	  case GI_TYPE_TAG_LONG:
	  case GI_TYPE_TAG_ULONG:
	    value->v_ulong = G_STRUCT_MEMBER(gulong, mem, offset);
	    result = TRUE;
	    break;
	  case GI_TYPE_TAG_SSIZE:
	  case GI_TYPE_TAG_SIZE:
	  case GI_TYPE_TAG_GTYPE:
	    value->v_size = G_STRUCT_MEMBER(gsize, mem, offset);
	    result = TRUE;
	    break;
	  case GI_TYPE_TAG_FLOAT:
	    value->v_float = G_STRUCT_MEMBER(gfloat, mem, offset);
	    result = TRUE;
	    break;
	  case GI_TYPE_TAG_DOUBLE:
	    value->v_double = G_STRUCT_MEMBER(gdouble, mem, offset);
	    result = TRUE;
	    break;
	  case GI_TYPE_TAG_TIME_T:
	    value->v_long = G_STRUCT_MEMBER(time_t, mem, offset);
	    result = TRUE;
	    break;
	  case GI_TYPE_TAG_UTF8:
	  case GI_TYPE_TAG_FILENAME:
	  case GI_TYPE_TAG_ARRAY:
	  case GI_TYPE_TAG_GLIST:
	  case GI_TYPE_TAG_GSLIST:
	  case GI_TYPE_TAG_GHASH:
	    g_warning("Field %s: type %s should have is_pointer set",
		      g_base_info_get_name ((GIBaseInfo *)field_info),
		      g_type_tag_to_string (g_type_info_get_tag (type_info)));
	    break;
	  case GI_TYPE_TAG_ERROR:
	    /* Needs to be handled by the language binding directly */
	    break;
	  case GI_TYPE_TAG_INTERFACE:
	    {
	      GIBaseInfo *interface = g_type_info_get_interface (type_info);
	      switch (g_base_info_get_type (interface))
		{
		case GI_INFO_TYPE_STRUCT:
		case GI_INFO_TYPE_UNION:
		case GI_INFO_TYPE_BOXED:
		  /* Needs to be handled by the language binding directly */
		  break;
		case GI_INFO_TYPE_OBJECT:
		  break;
		case GI_INFO_TYPE_ENUM:
		case GI_INFO_TYPE_FLAGS:
		  {
		    /* FIXME: there's a mismatch here between the value->v_int we use
		     * here and the glong result returned from g_value_info_get_value().
		     * But to switch this to glong, we'd have to make g_function_info_invoke()
		     * translate value->v_long to the proper ABI for an enum function
		     * call parameter, which will usually be int, and then fix up language
		     * bindings.
		     */
		    GITypeTag storage_type = g_enum_info_get_storage_type ((GIEnumInfo *)interface);
		    switch (storage_type)
		      {
		      case GI_TYPE_TAG_INT8:
		      case GI_TYPE_TAG_UINT8:
			value->v_int = (gint)G_STRUCT_MEMBER(guint8, mem, offset);
			result = TRUE;
			break;
		      case GI_TYPE_TAG_INT16:
		      case GI_TYPE_TAG_UINT16:
		      case GI_TYPE_TAG_SHORT:
		      case GI_TYPE_TAG_USHORT:
			value->v_int = (gint)G_STRUCT_MEMBER(guint16, mem, offset);
			result = TRUE;
			break;
		      case GI_TYPE_TAG_INT32:
		      case GI_TYPE_TAG_UINT32:
		      case GI_TYPE_TAG_INT:
		      case GI_TYPE_TAG_UINT:
			value->v_int = (gint)G_STRUCT_MEMBER(guint32, mem, offset);
			result = TRUE;
			break;
		      case GI_TYPE_TAG_INT64:
		      case GI_TYPE_TAG_UINT64:
			value->v_int = (gint)G_STRUCT_MEMBER(guint64, mem, offset);
			result = TRUE;
			break;
		      case GI_TYPE_TAG_LONG:
		      case GI_TYPE_TAG_ULONG:
			value->v_int = (gint)G_STRUCT_MEMBER(gulong, mem, offset);
			result = TRUE;
			break;
		      default:
			g_warning("Field %s: Unexpected enum storage type %s",
				  g_base_info_get_name ((GIBaseInfo *)field_info),
				  g_type_tag_to_string (storage_type));
			break;
		      }
		    break;
		  }
		case GI_INFO_TYPE_VFUNC:
		case GI_INFO_TYPE_CALLBACK:
		  g_warning("Field %s: Interface type %d should have is_pointer set",
			    g_base_info_get_name ((GIBaseInfo *)field_info),
			    g_base_info_get_type (interface));
		  break;
		case GI_INFO_TYPE_INVALID:
		case GI_INFO_TYPE_INTERFACE:
		case GI_INFO_TYPE_FUNCTION:
		case GI_INFO_TYPE_CONSTANT:
		case GI_INFO_TYPE_ERROR_DOMAIN:
		case GI_INFO_TYPE_VALUE:
		case GI_INFO_TYPE_SIGNAL:
		case GI_INFO_TYPE_PROPERTY:
		case GI_INFO_TYPE_FIELD:
		case GI_INFO_TYPE_ARG:
		case GI_INFO_TYPE_TYPE:
		case GI_INFO_TYPE_UNRESOLVED:
		  g_warning("Field %s: Interface type %d not expected",
			    g_base_info_get_name ((GIBaseInfo *)field_info),
			    g_base_info_get_type (interface));
		  break;
		}

	      g_base_info_unref ((GIBaseInfo *)interface);
	      break;
	    }
	    break;
	  }
      }

    g_base_info_unref ((GIBaseInfo *)type_info);

    return result;
}

/**
 * g_field_info_set_field:
 * @field_info: a #GIFieldInfo
 * @mem: pointer to a block of memory representing a C structure or union
 * @value: a #GArgument holding the value to store
 *
 * Writes a field identified by a #GFieldInfo to a C structure or
 * union.  This only handles fields of simple C types. It will fail
 * for a field of a composite type like a nested structure or union
 * even if that is actually writable. Note also that that it will refuse
 * to write fields where memory management would by required. A field
 * with a type such as 'char *' must be set with a setter function.
 *
 * Returns: %TRUE if writing the field succeeded, otherwise %FALSE
 */
gboolean
g_field_info_set_field (GIFieldInfo     *field_info,
			gpointer         mem,
			const GArgument *value)
{
    int offset;
    GITypeInfo *type_info;
    gboolean result = FALSE;

    if ((g_field_info_get_flags (field_info) & GI_FIELD_IS_WRITABLE) == 0)
      return FALSE;

    offset = g_field_info_get_offset (field_info);
    type_info = g_field_info_get_type (field_info);

    if (!g_type_info_is_pointer (type_info))
      {
	switch (g_type_info_get_tag (type_info))
	  {
	  case GI_TYPE_TAG_VOID:
	    g_warning("Field %s: should not be have void type",
		      g_base_info_get_name ((GIBaseInfo *)field_info));
	    break;
	  case GI_TYPE_TAG_BOOLEAN:
	    G_STRUCT_MEMBER(gboolean, mem, offset) = value->v_boolean != FALSE;
	    result = TRUE;
	    break;
	  case GI_TYPE_TAG_INT8:
	  case GI_TYPE_TAG_UINT8:
	    G_STRUCT_MEMBER(guint8, mem, offset) = value->v_uint8;
	    result = TRUE;
	    break;
	  case GI_TYPE_TAG_INT16:
	  case GI_TYPE_TAG_UINT16:
	  case GI_TYPE_TAG_SHORT:
	  case GI_TYPE_TAG_USHORT:
	    G_STRUCT_MEMBER(guint16, mem, offset) = value->v_uint16;
	    result = TRUE;
	    break;
	  case GI_TYPE_TAG_INT32:
	  case GI_TYPE_TAG_UINT32:
	  case GI_TYPE_TAG_INT:
	  case GI_TYPE_TAG_UINT:
	    G_STRUCT_MEMBER(guint32, mem, offset) = value->v_uint32;
	    result = TRUE;
	    break;
	  case GI_TYPE_TAG_INT64:
	  case GI_TYPE_TAG_UINT64:
	    G_STRUCT_MEMBER(guint64, mem, offset) = value->v_uint64;
	    result = TRUE;
	    break;
	  case GI_TYPE_TAG_LONG:
	  case GI_TYPE_TAG_ULONG:
	    G_STRUCT_MEMBER(gulong, mem, offset)= value->v_ulong;
	    result = TRUE;
	    break;
	  case GI_TYPE_TAG_SSIZE:
	  case GI_TYPE_TAG_SIZE:
	  case GI_TYPE_TAG_GTYPE:
	    G_STRUCT_MEMBER(gsize, mem, offset) = value->v_size;
	    result = TRUE;
	    break;
	  case GI_TYPE_TAG_FLOAT:
	    G_STRUCT_MEMBER(gfloat, mem, offset) = value->v_float;
	    result = TRUE;
	    break;
	  case GI_TYPE_TAG_DOUBLE:
	    G_STRUCT_MEMBER(gdouble, mem, offset)= value->v_double;
	    result = TRUE;
	    break;
	  case GI_TYPE_TAG_TIME_T:
	    G_STRUCT_MEMBER(time_t, mem, offset) = value->v_long;
	    result = TRUE;
	    break;
	  case GI_TYPE_TAG_UTF8:
	  case GI_TYPE_TAG_FILENAME:
	  case GI_TYPE_TAG_ARRAY:
	  case GI_TYPE_TAG_GLIST:
	  case GI_TYPE_TAG_GSLIST:
	  case GI_TYPE_TAG_GHASH:
	    g_warning("Field %s: type %s should have is_pointer set",
		      g_base_info_get_name ((GIBaseInfo *)field_info),
		      g_type_tag_to_string (g_type_info_get_tag (type_info)));
	    break;
	  case GI_TYPE_TAG_ERROR:
	    /* Needs to be handled by the language binding directly */
	    break;
	  case GI_TYPE_TAG_INTERFACE:
	    {
	      GIBaseInfo *interface = g_type_info_get_interface (type_info);
	      switch (g_base_info_get_type (interface))
		{
		case GI_INFO_TYPE_STRUCT:
		case GI_INFO_TYPE_UNION:
		case GI_INFO_TYPE_BOXED:
		  /* Needs to be handled by the language binding directly */
		  break;
		case GI_INFO_TYPE_OBJECT:
		  break;
		case GI_INFO_TYPE_ENUM:
		case GI_INFO_TYPE_FLAGS:
		  {
		    /* See FIXME above
		     */
		    GITypeTag storage_type = g_enum_info_get_storage_type ((GIEnumInfo *)interface);
		    switch (storage_type)
		      {
		      case GI_TYPE_TAG_INT8:
		      case GI_TYPE_TAG_UINT8:
			G_STRUCT_MEMBER(guint8, mem, offset) = (guint8)value->v_int;
			result = TRUE;
			break;
		      case GI_TYPE_TAG_INT16:
		      case GI_TYPE_TAG_UINT16:
		      case GI_TYPE_TAG_SHORT:
		      case GI_TYPE_TAG_USHORT:
			G_STRUCT_MEMBER(guint16, mem, offset) = (guint16)value->v_int;
			result = TRUE;
			break;
		      case GI_TYPE_TAG_INT32:
		      case GI_TYPE_TAG_UINT32:
		      case GI_TYPE_TAG_INT:
		      case GI_TYPE_TAG_UINT:
			G_STRUCT_MEMBER(guint32, mem, offset) = (guint32)value->v_int;
			result = TRUE;
			break;
		      case GI_TYPE_TAG_INT64:
		      case GI_TYPE_TAG_UINT64:
			G_STRUCT_MEMBER(guint64, mem, offset) = (guint64)value->v_int;
			result = TRUE;
			break;
		      case GI_TYPE_TAG_LONG:
		      case GI_TYPE_TAG_ULONG:
			G_STRUCT_MEMBER(gulong, mem, offset) = (gulong)value->v_int;
			result = TRUE;
			break;
		      default:
			g_warning("Field %s: Unexpected enum storage type %s",
				  g_base_info_get_name ((GIBaseInfo *)field_info),
				  g_type_tag_to_string (storage_type));
			break;
		      }
		    break;
		  }
		  break;
		case GI_INFO_TYPE_VFUNC:
		case GI_INFO_TYPE_CALLBACK:
		  g_warning("Field%s: Interface type %d should have is_pointer set",
			    g_base_info_get_name ((GIBaseInfo *)field_info),
			    g_base_info_get_type (interface));
		  break;
		case GI_INFO_TYPE_INVALID:
		case GI_INFO_TYPE_INTERFACE:
		case GI_INFO_TYPE_FUNCTION:
		case GI_INFO_TYPE_CONSTANT:
		case GI_INFO_TYPE_ERROR_DOMAIN:
		case GI_INFO_TYPE_VALUE:
		case GI_INFO_TYPE_SIGNAL:
		case GI_INFO_TYPE_PROPERTY:
		case GI_INFO_TYPE_FIELD:
		case GI_INFO_TYPE_ARG:
		case GI_INFO_TYPE_TYPE:
		case GI_INFO_TYPE_UNRESOLVED:
		  g_warning("Field %s: Interface type %d not expected",
			    g_base_info_get_name ((GIBaseInfo *)field_info),
			    g_base_info_get_type (interface));
		  break;
		}

	      g_base_info_unref ((GIBaseInfo *)interface);
	      break;
	    }
	    break;
	  }
      }

    g_base_info_unref ((GIBaseInfo *)type_info);

    return result;
}
