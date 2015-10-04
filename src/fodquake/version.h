// version.h

#if defined(__GNUC__) && !defined(__llvm__)
#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ != 6) || (__GNUC__ == 4 && __GNUC_MINOR__ == 6 && __GNUC_PATCHLEVEL__ < 2) || (__GNUC__ == 3 && __GNUC_MINOR__ < 3) || (__GNUC__ == 3 && __GNUC_MINOR__ == 3 && __GNUC_PATCHLEVEL__ < 6) || (__GNUC__ == 3 && __GNUC_MINOR__ == 4 && __GNUC_PATCHLEVEL__ < 6)
#define COMPILERVERSIONSTRINGAPPEND " (broken GCC)"
#endif
#endif

#ifndef COMPILERVERSIONSTRINGAPPEND
#define COMPILERVERSIONSTRINGAPPEND
#endif

#define	QW_VERSION              2.40
#define FODQUAKE_VERSION        "0.4 dev" COMPILERVERSIONSTRINGAPPEND

#ifdef _WIN32
#define QW_PLATFORM     "Win32"
#elif defined(linux)
#define QW_PLATFORM     "Linux"
#elif defined(__MORPHOS__)
#define QW_PLATFORM     "MorphOS"
#elif defined(__CYGWIN__)
#define QW_PLATFORM     "Cygwin"
#elif defined(__MACOSX__)
#define QW_PLATFORM     "MacOS X"
#elif defined(__FreeBSD__)
#define QW_PLATFORM     "FreeBSD"
#elif defined(__NetBSD__)
#define QW_PLATFORM     "NetBSD"
#elif defined(__OpenBSD__)
#define QW_PLATFORM     "OpenBSD"
#elif defined(GEKKO)
/* Not entirely true, but close enough for now. */
#define QW_PLATFORM     "Wii"
#elif defined(AROS)
#define QW_PLATFORM     "AROS"
#endif

#ifdef GLQUAKE
#define QW_RENDERER "GL"
#else
#define QW_RENDERER "Soft"
#endif

void CL_Version_f (void);
char *VersionString (void);
