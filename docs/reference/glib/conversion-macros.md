Title: Conversion Macros

# Conversion Macros

## Type Conversion

Many times GLib, GTK, and other libraries allow you to pass "user data" to a
callback, in the form of a void pointer. From time to time you want to pass
an integer instead of a pointer. You could allocate an integer, with
something like:

```c
int *ip = g_new (int, 1);
*ip = 42;
```

But this is inconvenient, and it's annoying to have to free the memory at
some later time.

Pointers are always at least 32 bits in size (on all platforms GLib intends
to support). Thus you can store at least 32-bit integer values in a pointer
value. Naively, you might try this, but it's incorrect:

```c
gpointer p;
int i;
p = (void*) 42;
i = (int) p;
```

Again, that example was not correct, don't copy it.

The problem is that on some systems you need to do this:

```c
gpointer p;
int i;
p = (void*) (long) 42;
i = (int) (long) p;
```

The GLib macros `GPOINTER_TO_INT()`, `GINT_TO_POINTER()`, etc. take care to
do the right thing on every platform.

**Warning**: You may not store pointers in integers. This is not portable in
any way, shape or form. These macros only allow storing integers in
pointers, and only preserve 32 bits of the integer; values outside the range
of a 32-bit integer will be mangled.


``GINT_TO_POINTER(value)``, ``GPOINTER_TO_INT(value)``
: Stuffs an integer into a pointer type, and vice versa. Remember, you may not
  store pointers in integers. This is not portable in any way, shape or form.
  These macros only allow storing integers in pointers, and only preserve 32
  bits of the integer; values outside the range of a 32-bit integer will be
  mangled.

``GUINT_TO_POINTER(value)``, ``GPOINTER_TO_UINT(value)``
: Stuffs an unsigned integer into a pointer type, and vice versa.

``GSIZE_TO_POINTER(value)``, ``GPOINTER_TO_SIZE(value)``
: Stuffs a `size_t` into a pointer type, and vice versa.

## Byte Order Conversion

These macros provide a portable way to determine the host byte order and to
convert values between different byte orders.

The byte order is the order in which bytes are stored to create larger data
types such as the #gint and #glong values.  The host byte order is the byte
order used on the current machine.

Some processors store the most significant bytes (i.e. the bytes that hold
the largest part of the value) first. These are known as big-endian
processors. Other processors (notably the x86 family) store the most
significant byte last. These are known as little-endian processors.

Finally, to complicate matters, some other processors store the bytes in a
rather curious order known as PDP-endian. For a 4-byte word, the 3rd most
significant byte is stored first, then the 4th, then the 1st and finally the
2nd.

Obviously there is a problem when these different processors communicate
with each other, for example over networks or by using binary file formats.
This is where these macros come in. They are typically used to convert
values into a byte order which has been agreed on for use when communicating
between different processors. The Internet uses what is known as 'network
byte order' as the standard byte order (which is in fact the big-endian byte
order).

Note that the byte order conversion macros may evaluate their arguments
multiple times, thus you should not use them with arguments which have
side-effects.

`G_BYTE_ORDER`
: The host byte order. This can be either `G_LITTLE_ENDIAN` or `G_BIG_ENDIAN`.

`G_LITTLE_ENDIAN`
: Specifies the little endian byte order.

`G_BIG_ENDIAN`
: Specifies the big endian byte order.

`G_PDP_ENDIAN`
: Specifies the PDP endian byte order.

### Signed

`GINT_FROM_BE(value)`
: Converts an `int` value from big-endian to host byte order.

`GINT_FROM_LE(value)`
: Converts an `int` value from little-endian to host byte order.

`GINT_TO_BE(value)`
: Converts an `int` value from host byte order to big-endian.

`GINT_TO_LE(value)`
: Converts an `int` value from host byte order to little-endian.

`GLONG_FROM_BE(value)`
: Converts a `long` value from big-endian to the host byte order.

`GLONG_FROM_LE(value)`
: Converts a `long` value from little-endian to host byte order.

`GLONG_TO_BE(value)`
: Converts a `long` value from host byte order to big-endian.

`GLONG_TO_LE(value)`
: Converts a `long` value from host byte order to little-endian.

`GSSIZE_FROM_BE(value)`
: Converts a `ssize_t` value from big-endian to host byte order.

`GSSIZE_FROM_LE(value)`
: Converts a `ssize_t` value from little-endian to host byte order.

`GSSIZE_TO_BE(value)`
: Converts a `ssize_t` value from host byte order to big-endian.

`GSSIZE_TO_LE(value)`
: Converts a `ssize_t` value from host byte order to little-endian.

`GINT16_FROM_BE(value)`
: Converts an `int16_t` value from big-endian to host byte order.

`GINT16_FROM_LE(value)`
: Converts an `int16_t` value from little-endian to host byte order.

`GINT16_TO_BE(value)`
: Converts an `int16_t` value from host byte order to big-endian.

`GINT16_TO_LE(value)`
: Converts an `int16_t` value from host byte order to little-endian.

`GINT32_FROM_BE(value)`
: Converts an `int32_t` value from big-endian to host byte order.

`GINT32_FROM_LE(value)`
: Converts an `int32_t` value from little-endian to host byte order.

`GINT32_TO_BE(value)`
: Converts an `int32_t` value from host byte order to big-endian.

`GINT32_TO_LE(value)`
: Converts an `int32_t` value from host byte order to little-endian.

`GINT64_FROM_BE(value)`
: Converts an `int64_t` value from big-endian to host byte order.

`GINT64_FROM_LE(value)`
: Converts an `int64_t` value from little-endian to host byte order.

`GINT64_TO_BE(value)`
: Converts an `int64_t` value from host byte order to big-endian.

`GINT64_TO_LE(value)`
: Converts an `int64_t` value from host byte order to little-endian.

### Unsigned

`GUINT_FROM_BE(value)`
: Converts an `unsigned int` value from big-endian to host byte order.

`GUINT_FROM_LE(value)`
: Converts an `unsigned int` value from little-endian to host byte order.

`GUINT_TO_BE(value)`
: Converts an `unsigned int` value from host byte order to big-endian.

`GUINT_TO_LE(value)`
: Converts an `unsigned int` value from host byte order to little-endian.

`GULONG_FROM_BE(value)`
: Converts an `unsigned long` value from big-endian to host byte order.

`GULONG_FROM_LE(value)`
: Converts an `unsigned long` value from little-endian to host byte order.

`GULONG_TO_BE(value)`
: Converts an `unsigned long` value from host byte order to big-endian.

`GULONG_TO_LE(value)`
: Converts an `unsigned long` value from host byte order to little-endian.

`GSIZE_FROM_BE(value)`
: Converts a `size_t` value from big-endian to the host byte order.

`GSIZE_FROM_LE(value)`
: Converts a `size_t` value from little-endian to host byte order.

`GSIZE_TO_BE(value)`
: Converts a `size_t` value from host byte order to big-endian.

`GSIZE_TO_LE(value)`
: Converts a `size_t` value from host byte order to little-endian.

`GUINT16_FROM_BE(value)`
: Converts an `uint16_t` value from big-endian to host byte order.

`GUINT16_FROM_LE(value)`
: Converts an `uint16_t` value from little-endian to host byte order.

`GUINT16_TO_BE(value)`
: Converts an `uint16_t` value from host byte order to big-endian.

`GUINT16_TO_LE(value)`
: Converts an `uint16_t` value from host byte order to little-endian.

`GUINT32_FROM_BE(value)`
: Converts an `uint32_t` value from big-endian to host byte order.

`GUINT32_FROM_LE(value)`
: Converts an `uint32_t` value from little-endian to host byte order.

`GUINT32_TO_BE(value)`
: Converts an `uint32_t` value from host byte order to big-endian.

`GUINT32_TO_LE(value)`
: Converts an `uint32_t` value from host byte order to little-endian.

`GUINT64_FROM_BE(value)`
: Converts an `uint64_t` value from big-endian to host byte order.

`GUINT64_FROM_LE(value)`
: Converts an `uint64_t` value from little-endian to host byte order.

`GUINT64_TO_BE(value)`
: Converts an `uint64_t` value from host byte order to big-endian.

`GUINT64_TO_LE(value)`
: Converts an `uint64_t` value from host byte order to little-endian.

`GUINT16_SWAP_BE_PDP(value)`
: Converts an `uint16_t` value between big-endian and pdp-endian byte order.
  The conversion is symmetric so it can be used both ways.

`GUINT16_SWAP_LE_BE(value)`
: Converts an `uint16_t` value between little-endian and big-endian byte order.
  The conversion is symmetric so it can be used both ways.

`GUINT16_SWAP_LE_PDP(value)`
: Converts an `uint16_t` value between little-endian and pdp-endian byte order.
  The conversion is symmetric so it can be used both ways.

`GUINT32_SWAP_BE_PDP(value)`
: Converts an `uint32_t` value between big-endian and pdp-endian byte order.
  The conversion is symmetric so it can be used both ways.

`GUINT32_SWAP_LE_BE(value)`
: Converts an `uint32_t` value between little-endian and big-endian byte order.
  The conversion is symmetric so it can be used both ways.

`GUINT32_SWAP_LE_PDP(value)`
: Converts an `uint32_t` value between little-endian and pdp-endian byte order.
  The conversion is symmetric so it can be used both ways.

`GUINT64_SWAP_LE_BE(value)`
: Converts a `uint64_t` value between little-endian and big-endian byte order.
  The conversion is symmetric so it can be used both ways.
