#if DONT_USE_ALLOCA
/* heap versions */
# define ALLOCA(type, array, nmemb, maxnmemb) ((array) = (type *)getbytes((nmemb) * sizeof(type)))
# define FREEA(type, array, nmemb, maxnmemb) (freebytes((array), (nmemb) * sizeof(type)))

#else /* !DONT_USE_ALLOCA */
/* stack version (unless <nmemb> exceeds <maxnmemb>) */

# ifdef HAVE_ALLOCA_H
#  include <alloca.h> /* linux, mac, mingw, cygwin,... */
# elif defined _WIN32
#  include <malloc.h> /* MSVC or mingw on windows */
# else
#  include <stdlib.h> /* BSDs for example */
# endif

#define MAX_ALLOCA_BYTES 128

# define ALLOCA(type, nmemb) (type *)((nmemb) < (MAX_ALLOCA_BYTES) ? \
            alloca((nmemb) * sizeof(type)) : getbytes((nmemb) * sizeof(type)))
# define FREEA(array, type, nmemb) (                                 \
        ((nmemb) >= (MAX_ALLOCA_BYTES)) ? (freebytes(array, (sizeof(type) * nmemb))) : 0)
#endif /* !DONT_USE_ALLOCA */

#define CALLOCA()
