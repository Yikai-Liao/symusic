#pragma once

#ifdef _MSC_VER
#include <string.h>

#if !defined(_WIN32)
#include <strings.h>

static inline int abcmidi_overlay_stricmp(const char* lhs, const char* rhs) {
    return strcasecmp(lhs, rhs);
}

static inline int abcmidi_overlay_strnicmp(const char* lhs, const char* rhs, size_t count) {
    return strncasecmp(lhs, rhs, count);
}

#define _stricmp abcmidi_overlay_stricmp
#define _strnicmp abcmidi_overlay_strnicmp
#endif

#ifndef ANSILIBS
#define ANSILIBS 1
#endif

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS 1
#endif

#ifndef strcasecmp
#define strcasecmp _stricmp
#endif

#ifndef strncasecmp
#define strncasecmp _strnicmp
#endif

/*
 * Upstream uses old _MSC_VER branches that force K&R declarations and
 * obsolete CRT aliases. Hide _MSC_VER from the included translation unit
 * and provide the small compatibility surface from this overlay instead.
 */
#undef _MSC_VER
#endif
