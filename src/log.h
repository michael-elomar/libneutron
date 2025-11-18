#include <stdio.h>
#include <string.h>
#include <errno.h>

#define RESET "\033[0m"
#define BLACK "\033[30m"              /* Black */
#define RED "\033[31m"                /* Red */
#define GREEN "\033[32m"              /* Green */
#define YELLOW "\033[33m"             /* Yellow */
#define BLUE "\033[34m"               /* Blue */
#define MAGENTA "\033[35m"            /* Magenta */
#define CYAN "\033[36m"               /* Cyan */
#define WHITE "\033[37m"              /* White */
#define BOLDBLACK "\033[1m\033[30m"   /* Bold Black */
#define BOLDRED "\033[1m\033[31m"     /* Bold Red */
#define BOLDGREEN "\033[1m\033[32m"   /* Bold Green */
#define BOLDYELLOW "\033[1m\033[33m"  /* Bold Yellow */
#define BOLDBLUE "\033[1m\033[34m"    /* Bold Blue */
#define BOLDMAGENTA "\033[1m\033[35m" /* Bold Magenta */
#define BOLDCYAN "\033[1m\033[36m"    /* Bold Cyan */
#define BOLDWHITE "\033[1m\033[37m"   /* Bold White */

#define LOG(_fmt, ...) printf(_fmt "\n", ##__VA_ARGS__)

#define LOGE(_fmt, ...) LOG(BOLDRED "[E]: " _fmt RESET, ##__VA_ARGS__)
#define LOGW(_fmt, ...) LOG(YELLOW "[W]: " _fmt RESET, ##__VA_ARGS__)
#define LOGI(_fmt, ...) LOG(GREEN "[I]: " _fmt RESET, ##__VA_ARGS__)
#define LOGD(_fmt, ...) LOG("[D]: " _fmt, ##__VA_ARGS__)

#define LOG_ERRNO(_func) \
	LOGE("%s - %s:%d err=%d (%s)", \
	     _func, \
	     __func__, \
	     __LINE__, \
	     errno, \
	     strerror(errno))
