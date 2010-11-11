#ifndef __CMPH_TYPES_H__
#define __CMPH_TYPES_H__

typedef char cmph_int8;
typedef unsigned char cmph_uint8;

typedef short cmph_int16;
typedef unsigned short cmph_uint16;

typedef int cmph_int32;
typedef unsigned int cmph_uint32;

#if defined(__ia64) || defined(__x86_64__)
  /** \typedef long cmph_int64;
   *  \brief 64-bit integer for a 64-bit achitecture.
   */
  typedef long cmph_int64;

  /** \typedef unsigned long cmph_uint64;
   *  \brief Unsigned 64-bit integer for a 64-bit achitecture.
   */
  typedef unsigned long cmph_uint64;
#else
  /** \typedef long long cmph_int64;
   *  \brief 64-bit integer for a 32-bit achitecture.
   */
  typedef long long cmph_int64;

  /** \typedef unsigned long long cmph_uint64;
   *  \brief Unsigned 64-bit integer for a 32-bit achitecture.
   */
  typedef unsigned long long cmph_uint64;
#endif

typedef enum { CMPH_HASH_JENKINS, CMPH_HASH_COUNT } CMPH_HASH;
extern const char *cmph_hash_names[];
typedef enum { CMPH_BMZ, CMPH_BMZ8, CMPH_CHM, CMPH_BRZ, CMPH_FCH,
               CMPH_BDZ, CMPH_BDZ_PH,
               CMPH_CHD_PH, CMPH_CHD, CMPH_COUNT } CMPH_ALGO;
extern const char *cmph_names[];

#endif
