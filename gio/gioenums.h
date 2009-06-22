/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2006-2007 Red Hat, Inc.
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
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 */

#if !defined (__GIO_GIO_H_INSIDE__) && !defined (GIO_COMPILATION)
#error "Only <gio/gio.h> can be included directly."
#endif

#ifndef __GIO_ENUMS_H__
#define __GIO_ENUMS_H__

#include <glib-object.h>

G_BEGIN_DECLS


/**
 * GAppInfoCreateFlags:
 * @G_APP_INFO_CREATE_NONE: No flags.
 * @G_APP_INFO_CREATE_NEEDS_TERMINAL: Application opens in a terminal window.
 * @G_APP_INFO_CREATE_SUPPORTS_URIS: Application supports URI arguments.
 *
 * Flags used when creating a #GAppInfo.
 */
typedef enum {
  G_APP_INFO_CREATE_NONE           = 0,         /*< nick=none >*/
  G_APP_INFO_CREATE_NEEDS_TERMINAL = (1 << 0),  /*< nick=needs-terminal >*/
  G_APP_INFO_CREATE_SUPPORTS_URIS  = (1 << 1)   /*< nick=supports-uris >*/
} GAppInfoCreateFlags;


/**
 * GDataStreamByteOrder:
 * @G_DATA_STREAM_BYTE_ORDER_BIG_ENDIAN: Selects Big Endian byte order.
 * @G_DATA_STREAM_BYTE_ORDER_LITTLE_ENDIAN: Selects Little Endian byte order.
 * @G_DATA_STREAM_BYTE_ORDER_HOST_ENDIAN: Selects endianness based on host machine's architecture.
 *
 * #GDataStreamByteOrder is used to ensure proper endianness of streaming data sources
 * across various machine architectures.
 *
 **/
typedef enum {
  G_DATA_STREAM_BYTE_ORDER_BIG_ENDIAN,
  G_DATA_STREAM_BYTE_ORDER_LITTLE_ENDIAN,
  G_DATA_STREAM_BYTE_ORDER_HOST_ENDIAN
} GDataStreamByteOrder;


/**
 * GDataStreamNewlineType:
 * @G_DATA_STREAM_NEWLINE_TYPE_LF: Selects "LF" line endings, common on most modern UNIX platforms.
 * @G_DATA_STREAM_NEWLINE_TYPE_CR: Selects "CR" line endings.
 * @G_DATA_STREAM_NEWLINE_TYPE_CR_LF: Selects "CR, LF" line ending, common on Microsoft Windows.
 * @G_DATA_STREAM_NEWLINE_TYPE_ANY: Automatically try to handle any line ending type.
 *
 * #GDataStreamNewlineType is used when checking for or setting the line endings for a given file.
 **/
typedef enum {
  G_DATA_STREAM_NEWLINE_TYPE_LF,
  G_DATA_STREAM_NEWLINE_TYPE_CR,
  G_DATA_STREAM_NEWLINE_TYPE_CR_LF,
  G_DATA_STREAM_NEWLINE_TYPE_ANY
} GDataStreamNewlineType;


/**
 * GFileAttributeType:
 * @G_FILE_ATTRIBUTE_TYPE_INVALID: indicates an invalid or uninitalized type.
 * @G_FILE_ATTRIBUTE_TYPE_STRING: a null terminated UTF8 string.
 * @G_FILE_ATTRIBUTE_TYPE_BYTE_STRING: a zero terminated string of non-zero bytes.
 * @G_FILE_ATTRIBUTE_TYPE_BOOLEAN: a boolean value.
 * @G_FILE_ATTRIBUTE_TYPE_UINT32: an unsigned 4-byte/32-bit integer.
 * @G_FILE_ATTRIBUTE_TYPE_INT32: a signed 4-byte/32-bit integer.
 * @G_FILE_ATTRIBUTE_TYPE_UINT64: an unsigned 8-byte/64-bit integer.
 * @G_FILE_ATTRIBUTE_TYPE_INT64: a signed 8-byte/64-bit integer.
 * @G_FILE_ATTRIBUTE_TYPE_OBJECT: a #GObject.
 * @G_FILE_ATTRIBUTE_TYPE_STRINGV: a %NULL terminated char **. Since 2.22
 *
 * The data types for file attributes.
 **/
typedef enum {
  G_FILE_ATTRIBUTE_TYPE_INVALID = 0,
  G_FILE_ATTRIBUTE_TYPE_STRING,
  G_FILE_ATTRIBUTE_TYPE_BYTE_STRING, /* zero terminated string of non-zero bytes */
  G_FILE_ATTRIBUTE_TYPE_BOOLEAN,
  G_FILE_ATTRIBUTE_TYPE_UINT32,
  G_FILE_ATTRIBUTE_TYPE_INT32,
  G_FILE_ATTRIBUTE_TYPE_UINT64,
  G_FILE_ATTRIBUTE_TYPE_INT64,
  G_FILE_ATTRIBUTE_TYPE_OBJECT,
  G_FILE_ATTRIBUTE_TYPE_STRINGV
} GFileAttributeType;


/**
 * GFileAttributeInfoFlags:
 * @G_FILE_ATTRIBUTE_INFO_NONE: no flags set.
 * @G_FILE_ATTRIBUTE_INFO_COPY_WITH_FILE: copy the attribute values when the file is copied.
 * @G_FILE_ATTRIBUTE_INFO_COPY_WHEN_MOVED: copy the attribute values when the file is moved.
 *
 * Flags specifying the behaviour of an attribute.
 **/
typedef enum {
  G_FILE_ATTRIBUTE_INFO_NONE            = 0,
  G_FILE_ATTRIBUTE_INFO_COPY_WITH_FILE  = (1 << 0),
  G_FILE_ATTRIBUTE_INFO_COPY_WHEN_MOVED = (1 << 1)
} GFileAttributeInfoFlags;


/**
 * GFileAttributeStatus:
 * @G_FILE_ATTRIBUTE_STATUS_UNSET: Attribute value is unset (empty).
 * @G_FILE_ATTRIBUTE_STATUS_SET: Attribute value is set.
 * @G_FILE_ATTRIBUTE_STATUS_ERROR_SETTING: Indicates an error in setting the value.
 *
 * Used by g_file_set_attributes_from_info() when setting file attributes.
 **/
typedef enum {
  G_FILE_ATTRIBUTE_STATUS_UNSET = 0,
  G_FILE_ATTRIBUTE_STATUS_SET,
  G_FILE_ATTRIBUTE_STATUS_ERROR_SETTING
} GFileAttributeStatus;


/**
 * GFileQueryInfoFlags:
 * @G_FILE_QUERY_INFO_NONE: No flags set.
 * @G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS: Don't follow symlinks.
 *
 * Flags used when querying a #GFileInfo.
 */
typedef enum {
  G_FILE_QUERY_INFO_NONE              = 0,
  G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS = (1 << 0)   /*< nick=nofollow-symlinks >*/
} GFileQueryInfoFlags;


/**
 * GFileCreateFlags:
 * @G_FILE_CREATE_NONE: No flags set.
 * @G_FILE_CREATE_PRIVATE: Create a file that can only be
 *    accessed by the current user.
 * @G_FILE_CREATE_REPLACE_DESTINATION: Replace the destination
 *    as if it didn't exist before. Don't try to keep any old
 *    permissions, replace instead of following links. This
 *    is generally useful if you're doing a "copy over" 
 *    rather than a "save new version of" replace operation.
 *    You can think of it as "unlink destination" before
 *    writing to it, although the implementation may not
 *    be exactly like that. Since 2.20
 *
 * Flags used when an operation may create a file.
 */
typedef enum {
  G_FILE_CREATE_NONE    = 0,
  G_FILE_CREATE_PRIVATE = (1 << 0),
  G_FILE_CREATE_REPLACE_DESTINATION = (1 << 1)
} GFileCreateFlags;


/**
 * GMountMountFlags:
 * @G_MOUNT_MOUNT_NONE: No flags set.
 *
 * Flags used when mounting a mount.
 */
typedef enum {
  G_MOUNT_MOUNT_NONE = 0
} GMountMountFlags;


/**
 * GMountUnmountFlags:
 * @G_MOUNT_UNMOUNT_NONE: No flags set.
 * @G_MOUNT_UNMOUNT_FORCE: Unmount even if there are outstanding
 *  file operations on the mount.
 *
 * Flags used when an unmounting a mount.
 */
typedef enum {
  G_MOUNT_UNMOUNT_NONE  = 0,
  G_MOUNT_UNMOUNT_FORCE = (1 << 0)
} GMountUnmountFlags;

/**
 * GDriveStartFlags:
 * @G_DRIVE_START_NONE: No flags set.
 *
 * Flags used when starting a drive.
 *
 * Since: 2.22
 */
typedef enum {
  G_DRIVE_START_NONE = 0
} GDriveStartFlags;

/**
 * GDriveStartStopType:
 * @G_DRIVE_START_STOP_TYPE_UNKNOWN: Unknown or drive doesn't support
 *    start/stop.
 * @G_DRIVE_START_STOP_TYPE_SHUTDOWN: The stop method will physically
 *    shut down the drive and e.g. power down the port the drive is
 *    attached to.
 * @G_DRIVE_START_STOP_TYPE_NETWORK: The start/stop methods are used
 *    for connecting/disconnect to the drive over the network.
 * @G_DRIVE_START_STOP_TYPE_MULTIDISK: The start/stop methods will
 *    assemble/disassemble a virtual drive from several physical
 *    drives.
 * @G_DRIVE_START_STOP_TYPE_PASSWORD: The start/stop methods will
 *    unlock/lock the disk (for example using the ATA <quote>SECURITY
 *    UNLOCK DEVICE</quote> command)
 *
 * Enumeration describing how a drive can be started/stopped.
 *
 * Since: 2.22
 */
typedef enum {
  G_DRIVE_START_STOP_TYPE_UNKNOWN,
  G_DRIVE_START_STOP_TYPE_SHUTDOWN,
  G_DRIVE_START_STOP_TYPE_NETWORK,
  G_DRIVE_START_STOP_TYPE_MULTIDISK,
  G_DRIVE_START_STOP_TYPE_PASSWORD
} GDriveStartStopType;

/**
 * GFileCopyFlags:
 * @G_FILE_COPY_NONE: No flags set.
 * @G_FILE_COPY_OVERWRITE: Overwrite any existing files
 * @G_FILE_COPY_BACKUP: Make a backup of any existing files.
 * @G_FILE_COPY_NOFOLLOW_SYMLINKS: Don't follow symlinks.
 * @G_FILE_COPY_ALL_METADATA: Copy all file metadata instead of just default set used for copy (see #GFileInfo).
 * @G_FILE_COPY_NO_FALLBACK_FOR_MOVE: Don't use copy and delete fallback if native move not supported.
 * @G_FILE_COPY_TARGET_DEFAULT_PERMS: Leaves target file with default perms, instead of setting the source file perms.
 *
 * Flags used when copying or moving files.
 */
typedef enum {
  G_FILE_COPY_NONE                 = 0,          /*< nick=none >*/
  G_FILE_COPY_OVERWRITE            = (1 << 0),
  G_FILE_COPY_BACKUP               = (1 << 1),
  G_FILE_COPY_NOFOLLOW_SYMLINKS    = (1 << 2),
  G_FILE_COPY_ALL_METADATA         = (1 << 3),
  G_FILE_COPY_NO_FALLBACK_FOR_MOVE = (1 << 4),
  G_FILE_COPY_TARGET_DEFAULT_PERMS = (1 << 5)
} GFileCopyFlags;


/**
 * GFileMonitorFlags:
 * @G_FILE_MONITOR_NONE: No flags set.
 * @G_FILE_MONITOR_WATCH_MOUNTS: Watch for mount events.
 *
 * Flags used to set what a #GFileMonitor will watch for.
 */
typedef enum {
  G_FILE_MONITOR_NONE         = 0,
  G_FILE_MONITOR_WATCH_MOUNTS = (1 << 0)
} GFileMonitorFlags;


/**
 * GFileType:
 * @G_FILE_TYPE_UNKNOWN: File's type is unknown.
 * @G_FILE_TYPE_REGULAR: File handle represents a regular file.
 * @G_FILE_TYPE_DIRECTORY: File handle represents a directory.
 * @G_FILE_TYPE_SYMBOLIC_LINK: File handle represents a symbolic link
 *    (Unix systems).
 * @G_FILE_TYPE_SPECIAL: File is a "special" file, such as a socket, fifo,
 *    block device, or character device.
 * @G_FILE_TYPE_SHORTCUT: File is a shortcut (Windows systems).
 * @G_FILE_TYPE_MOUNTABLE: File is a mountable location.
 *
 * Indicates the file's on-disk type.
 **/
typedef enum {
  G_FILE_TYPE_UNKNOWN = 0,
  G_FILE_TYPE_REGULAR,
  G_FILE_TYPE_DIRECTORY,
  G_FILE_TYPE_SYMBOLIC_LINK,
  G_FILE_TYPE_SPECIAL, /* socket, fifo, blockdev, chardev */
  G_FILE_TYPE_SHORTCUT,
  G_FILE_TYPE_MOUNTABLE
} GFileType;


/**
 * GFilesystemPreviewType:
 * @G_FILESYSTEM_PREVIEW_TYPE_IF_ALWAYS: Only preview files if user has explicitly requested it.
 * @G_FILESYSTEM_PREVIEW_TYPE_IF_LOCAL: Preview files if user has requested preview of "local" files.
 * @G_FILESYSTEM_PREVIEW_TYPE_NEVER: Never preview files.
 *
 * Indicates a hint from the file system whether files should be
 * previewed in a file manager. Returned as the value of the key
 * #G_FILE_ATTRIBUTE_FILESYSTEM_USE_PREVIEW.
 **/
typedef enum {
  G_FILESYSTEM_PREVIEW_TYPE_IF_ALWAYS = 0,
  G_FILESYSTEM_PREVIEW_TYPE_IF_LOCAL,
  G_FILESYSTEM_PREVIEW_TYPE_NEVER
} GFilesystemPreviewType;


/**
 * GFileMonitorEvent:
 * @G_FILE_MONITOR_EVENT_CHANGED: a file changed.
 * @G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT: a hint that this was probably the last change in a set of changes.
 * @G_FILE_MONITOR_EVENT_DELETED: a file was deleted.
 * @G_FILE_MONITOR_EVENT_CREATED: a file was created.
 * @G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED: a file attribute was changed.
 * @G_FILE_MONITOR_EVENT_PRE_UNMOUNT: the file location will soon be unmounted.
 * @G_FILE_MONITOR_EVENT_UNMOUNTED: the file location was unmounted.
 *
 * Specifies what type of event a monitor event is.
 **/
typedef enum {
  G_FILE_MONITOR_EVENT_CHANGED,
  G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT,
  G_FILE_MONITOR_EVENT_DELETED,
  G_FILE_MONITOR_EVENT_CREATED,
  G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED,
  G_FILE_MONITOR_EVENT_PRE_UNMOUNT,
  G_FILE_MONITOR_EVENT_UNMOUNTED
} GFileMonitorEvent;


/* This enumeration conflicts with GIOError in giochannel.h. However,
 * that is only used as a return value in some deprecated functions.
 * So, we reuse the same prefix for the enumeration values, but call
 * the actual enumeration (which is rarely used) GIOErrorEnum.
 */
/**
 * GIOErrorEnum:
 * @G_IO_ERROR_FAILED: Generic error condition for when any operation fails.
 * @G_IO_ERROR_NOT_FOUND: File not found error.
 * @G_IO_ERROR_EXISTS: File already exists error.
 * @G_IO_ERROR_IS_DIRECTORY: File is a directory error.
 * @G_IO_ERROR_NOT_DIRECTORY: File is not a directory.
 * @G_IO_ERROR_NOT_EMPTY: File is a directory that isn't empty.
 * @G_IO_ERROR_NOT_REGULAR_FILE: File is not a regular file.
 * @G_IO_ERROR_NOT_SYMBOLIC_LINK: File is not a symbolic link.
 * @G_IO_ERROR_NOT_MOUNTABLE_FILE: File cannot be mounted.
 * @G_IO_ERROR_FILENAME_TOO_LONG: Filename is too many characters.
 * @G_IO_ERROR_INVALID_FILENAME: Filename is invalid or contains invalid characters.
 * @G_IO_ERROR_TOO_MANY_LINKS: File contains too many symbolic links.
 * @G_IO_ERROR_NO_SPACE: No space left on drive.
 * @G_IO_ERROR_INVALID_ARGUMENT: Invalid argument.
 * @G_IO_ERROR_PERMISSION_DENIED: Permission denied.
 * @G_IO_ERROR_NOT_SUPPORTED: Operation not supported for the current backend.
 * @G_IO_ERROR_NOT_MOUNTED: File isn't mounted.
 * @G_IO_ERROR_ALREADY_MOUNTED: File is already mounted.
 * @G_IO_ERROR_CLOSED: File was closed.
 * @G_IO_ERROR_CANCELLED: Operation was cancelled. See #GCancellable.
 * @G_IO_ERROR_PENDING: Operations are still pending.
 * @G_IO_ERROR_READ_ONLY: File is read only.
 * @G_IO_ERROR_CANT_CREATE_BACKUP: Backup couldn't be created.
 * @G_IO_ERROR_WRONG_ETAG: File's Entity Tag was incorrect.
 * @G_IO_ERROR_TIMED_OUT: Operation timed out.
 * @G_IO_ERROR_WOULD_RECURSE: Operation would be recursive.
 * @G_IO_ERROR_BUSY: File is busy.
 * @G_IO_ERROR_WOULD_BLOCK: Operation would block.
 * @G_IO_ERROR_HOST_NOT_FOUND: Host couldn't be found (remote operations).
 * @G_IO_ERROR_WOULD_MERGE: Operation would merge files.
 * @G_IO_ERROR_FAILED_HANDLED: Operation failed and a helper program has 
 *     already interacted with the user. Do not display any error dialog.
 * @G_IO_ERROR_TOO_MANY_OPEN_FILES: The current process has too many files 
 *     open and can't open any more. Duplicate descriptors do count toward 
 *     this limit. Since 2.20
 * @G_IO_ERROR_NOT_INITIALIZED: The object has not been initialized. Since 2.22
 * @G_IO_ERROR_ADDRESS_IN_USE: The requested address is already in use. Since 2.22
 *
 * Error codes returned by GIO functions.
 *
 **/
typedef enum {
  G_IO_ERROR_FAILED,
  G_IO_ERROR_NOT_FOUND,
  G_IO_ERROR_EXISTS,
  G_IO_ERROR_IS_DIRECTORY,
  G_IO_ERROR_NOT_DIRECTORY,
  G_IO_ERROR_NOT_EMPTY,
  G_IO_ERROR_NOT_REGULAR_FILE,
  G_IO_ERROR_NOT_SYMBOLIC_LINK,
  G_IO_ERROR_NOT_MOUNTABLE_FILE,
  G_IO_ERROR_FILENAME_TOO_LONG,
  G_IO_ERROR_INVALID_FILENAME,
  G_IO_ERROR_TOO_MANY_LINKS,
  G_IO_ERROR_NO_SPACE,
  G_IO_ERROR_INVALID_ARGUMENT,
  G_IO_ERROR_PERMISSION_DENIED,
  G_IO_ERROR_NOT_SUPPORTED,
  G_IO_ERROR_NOT_MOUNTED,
  G_IO_ERROR_ALREADY_MOUNTED,
  G_IO_ERROR_CLOSED,
  G_IO_ERROR_CANCELLED,
  G_IO_ERROR_PENDING,
  G_IO_ERROR_READ_ONLY,
  G_IO_ERROR_CANT_CREATE_BACKUP,
  G_IO_ERROR_WRONG_ETAG,
  G_IO_ERROR_TIMED_OUT,
  G_IO_ERROR_WOULD_RECURSE,
  G_IO_ERROR_BUSY,
  G_IO_ERROR_WOULD_BLOCK,
  G_IO_ERROR_HOST_NOT_FOUND,
  G_IO_ERROR_WOULD_MERGE,
  G_IO_ERROR_FAILED_HANDLED,
  G_IO_ERROR_TOO_MANY_OPEN_FILES,
  G_IO_ERROR_NOT_INITIALIZED,
  G_IO_ERROR_ADDRESS_IN_USE
} GIOErrorEnum;


/**
 * GAskPasswordFlags:
 * @G_ASK_PASSWORD_NEED_PASSWORD: operation requires a password.
 * @G_ASK_PASSWORD_NEED_USERNAME: operation requires a username.
 * @G_ASK_PASSWORD_NEED_DOMAIN: operation requires a domain.
 * @G_ASK_PASSWORD_SAVING_SUPPORTED: operation supports saving settings.
 * @G_ASK_PASSWORD_ANONYMOUS_SUPPORTED: operation supports anonymous users.
 *
 * #GAskPasswordFlags are used to request specific information from the
 * user, or to notify the user of their choices in an authentication
 * situation.
 **/
typedef enum {
  G_ASK_PASSWORD_NEED_PASSWORD       = (1 << 0),
  G_ASK_PASSWORD_NEED_USERNAME       = (1 << 1),
  G_ASK_PASSWORD_NEED_DOMAIN         = (1 << 2),
  G_ASK_PASSWORD_SAVING_SUPPORTED    = (1 << 3),
  G_ASK_PASSWORD_ANONYMOUS_SUPPORTED = (1 << 4)
} GAskPasswordFlags;


/**
 * GPasswordSave:
 * @G_PASSWORD_SAVE_NEVER: never save a password.
 * @G_PASSWORD_SAVE_FOR_SESSION: save a password for the session.
 * @G_PASSWORD_SAVE_PERMANENTLY: save a password permanently.
 *
 * #GPasswordSave is used to indicate the lifespan of a saved password.
 *
 * #Gvfs stores passwords in the Gnome keyring when this flag allows it
 * to, and later retrieves it again from there.
 **/
typedef enum {
  G_PASSWORD_SAVE_NEVER,
  G_PASSWORD_SAVE_FOR_SESSION,
  G_PASSWORD_SAVE_PERMANENTLY
} GPasswordSave;


/**
 * GMountOperationResult:
 * @G_MOUNT_OPERATION_HANDLED: The request was fulfilled and the
 *     user specified data is now available
 * @G_MOUNT_OPERATION_ABORTED: The user requested the mount operation
 *     to be aborted
 * @G_MOUNT_OPERATION_UNHANDLED: The request was unhandled (i.e. not
 *     implemented)
 *
 * #GMountOperationResult is returned as a result when a request for
 * information is send by the mounting operation.
 **/
typedef enum {
  G_MOUNT_OPERATION_HANDLED,
  G_MOUNT_OPERATION_ABORTED,
  G_MOUNT_OPERATION_UNHANDLED
} GMountOperationResult;


/**
 * GOutputStreamSpliceFlags:
 * @G_OUTPUT_STREAM_SPLICE_NONE: Do not close either stream.
 * @G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE: Close the source stream after
 *     the splice.
 * @G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET: Close the target stream after
 *     the splice.
 *
 * GOutputStreamSpliceFlags determine how streams should be spliced.
 **/
typedef enum {
  G_OUTPUT_STREAM_SPLICE_NONE         = 0,
  G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE = (1 << 0),
  G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET = (1 << 1)
} GOutputStreamSpliceFlags;


/**
 * GEmblemOrigin:
 * @G_EMBLEM_ORIGIN_UNKNOWN: Emblem of unknown origin
 * @G_EMBLEM_ORIGIN_DEVICE: Emblem adds device-specific information
 * @G_EMBLEM_ORIGIN_LIVEMETADATA: Emblem depicts live metadata, such as "readonly"
 * @G_EMBLEM_ORIGIN_TAG: Emblem comes from a user-defined tag, e.g. set by nautilus (in the future)
 *
 * GEmblemOrigin is used to add information about the origin of the emblem
 * to #GEmblem.
 *
 * Since: 2.18
 */
typedef enum  {
  G_EMBLEM_ORIGIN_UNKNOWN,
  G_EMBLEM_ORIGIN_DEVICE,
  G_EMBLEM_ORIGIN_LIVEMETADATA,
  G_EMBLEM_ORIGIN_TAG
} GEmblemOrigin;

/**
 * GResolverError:
 * @G_RESOLVER_ERROR_NOT_FOUND: the requested name/address/service was not
 *     found
 * @G_RESOLVER_ERROR_TEMPORARY_FAILURE: the requested information could not
 *     be looked up due to a network error or similar problem
 * @G_RESOLVER_ERROR_INTERNAL: unknown error
 *
 * An error code used with %G_RESOLVER_ERROR in a #GError returned
 * from a #GResolver routine.
 *
 * Since: 2.22
 */
typedef enum {
  G_RESOLVER_ERROR_NOT_FOUND,
  G_RESOLVER_ERROR_TEMPORARY_FAILURE,
  G_RESOLVER_ERROR_INTERNAL
} GResolverError;

/**
 * GSocketFamily:
 * @G_SOCKET_FAMILY_INVALID: no address family
 * @G_SOCKET_FAMILY_IPV4: the IPv4 family
 * @G_SOCKET_FAMILY_IPV6: the IPv6 family
 * @G_SOCKET_FAMILY_UNIX: the UNIX domain family
 *
 * The protocol family of a #GSocketAddress. (These values are
 * identical to the system defines %AF_INET, %AF_INET6 and %AF_UNIX,
 * if available.)
 *
 * Since: 2.22
 */
typedef enum {
  G_SOCKET_FAMILY_INVALID,
#ifdef GLIB_SYSDEF_AF_UNIX
  G_SOCKET_FAMILY_UNIX = GLIB_SYSDEF_AF_UNIX,
#endif
  G_SOCKET_FAMILY_IPV4 = GLIB_SYSDEF_AF_INET,
  G_SOCKET_FAMILY_IPV6 = GLIB_SYSDEF_AF_INET6
} GSocketFamily;

/**
 * GSocketType:
 * @G_SOCKET_TYPE_INVALID: Type unknown or wrong
 * @G_SOCKET_TYPE_STREAM: Reliable connection-based byte streams (e.g. TCP).
 * @G_SOCKET_TYPE_DATAGRAM: Connectionless, unreliable datagram passing.
 *     (e.g. UDP)
 * @G_SOCKET_TYPE_SEQPACKET: Reliable connection-based passing of datagrams
 *     of fixed maximum length (e.g. SCTP).
 *
 * Flags used when creating a #GSocket. Some protocols may not implement
 * all the socket types.
 *
 * Since: 2.22
 */
typedef enum
{
  G_SOCKET_TYPE_INVALID,
  G_SOCKET_TYPE_STREAM,
  G_SOCKET_TYPE_DATAGRAM,
  G_SOCKET_TYPE_SEQPACKET
} GSocketType;

/**
 * GSocketMsgFlags:
 * @G_SOCKET_MSG_NONE: No flags.
 * @G_SOCKET_MSG_OOB: Request to send/receive out of band data.
 * @G_SOCKET_MSG_PEEK: Read data from the socket without removing it from
 *     the queue.
 * @G_SOCKET_MSG_DONTROUTE: Don't use a gateway to send out the packet,
 *     only send to hosts on directly connected networks.
 *
 * Flags used in g_socket_receive_message() and g_socket_send_message().
 * The flags listed in the enum are some commonly available flags, but the
 * values used for them are the same as on the platform, and any other flags
 * are passed in/out as is. So to use a platform specific flag, just include
 * the right system header and pass in the flag.
 *
 * Since: 2.22
 */
typedef enum
{
  G_SOCKET_MSG_NONE,
  G_SOCKET_MSG_OOB = GLIB_SYSDEF_MSG_OOB,
  G_SOCKET_MSG_PEEK = GLIB_SYSDEF_MSG_PEEK,
  G_SOCKET_MSG_DONTROUTE = GLIB_SYSDEF_MSG_DONTROUTE
} GSocketMsgFlags;

/**
 * GSocketProtocol:
 * @G_SOCKET_PROTOCOL_UNKNOWN: The protocol type is unknown
 * @G_SOCKET_PROTOCOL_DEFAULT: The default protocol for the family/type
 * @G_SOCKET_PROTOCOL_TCP: TCP over IP
 * @G_SOCKET_PROTOCOL_UDP: UDP over IP
 * @G_SOCKET_PROTOCOL_SCTP: SCTP over IP
 *
 * A protocol identifier is specified when creating a #GSocket, which is a
 * family/type specific identifier, where 0 means the default protocol for
 * the particular family/type.
 *
 * This enum contains a set of commonly available and used protocols. You
 * can also pass any other identifiers handled by the platform in order to
 * use protocols not listed here.
 *
 * Since: 2.22
 */
typedef enum {
  G_SOCKET_PROTOCOL_UNKNOWN = -1,
  G_SOCKET_PROTOCOL_DEFAULT = 0,
  G_SOCKET_PROTOCOL_TCP     = 6,
  G_SOCKET_PROTOCOL_UDP     = 17,
  G_SOCKET_PROTOCOL_SCTP    = 132
} GSocketProtocol;

G_END_DECLS

#endif /* __GIO_ENUMS_H__ */
