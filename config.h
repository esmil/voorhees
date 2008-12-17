#if defined(_WIN32) || defined(__WIN32__)
# ifndef WIN32
#  define WIN32
# endif
#endif
#if defined(_WIN64)
# ifndef WIN64
#  define WIN64
# endif
#endif
#if defined(_WINDOWS) || defined(WIN32) || defined(WIN64)
# ifndef WINDOWS
#  define WINDOWS
# endif
#endif
