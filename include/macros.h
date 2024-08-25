#ifndef _MACROS_H_
#define _MACROS_H_


#ifdef __cplusplus
    #include <iostream>
#endif
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <binary_xml/ANSI.h>

#ifndef nullptr
    #define nullptr NULL
#endif
#define INT_NULL_VALUE -0x80000000

#define ERRNO_SHOW(code,command,params) do {int __e = errno;std::cerr << ANSI_RED_BRIGHT "[" << (code) << "] Error: " command "(" << params << ") - " << __e << ": " << strerror(__e) << ANSI_RESET_LF;} while(0)
#define ERRNO_SHOW_EXPL(code,command,params,__e) do {std::cerr << ANSI_RED_BRIGHT "[" << (code) << "] Error: " command "(" << params << ") - " << __e << ": " << strerror(__e) << ANSI_RESET_LF;} while(0)
#define ERRNO_SHOW_MSG(code,command,params,msg) do {std::cerr << ANSI_RED_BRIGHT "[" << (code) << "] Error: " command "(" << params << ") - : " << msg << ANSI_RESET_LF;} while(0)



#ifdef __cplusplus
    #ifndef NDEBUG
        #define ASSERT_NO_(code,condition,action) do {if (!(condition)) { std::cerr << ANSI_RED_BRIGHT "[" << (code) << "] " << __FUNCTION__ << ":" ANSI_RED_DARK " Assertion " << #condition << " failed!" ANSI_RESET_LF;action;}} while(0)
    #else
        #define ASSERT_NO_(code,condition,action) do {if (!(condition)) { std::cerr << ANSI_RED_BRIGHT "[" << (code) << "] "  << __FUNCTION__ << ":" ANSI_RED_DARK " Assertion failed!" ANSI_RESET_LF;action;}} while(0)
    #endif
#endif

#define ASSERT_NO_RET_FALSE(code,condition) ASSERT_NO_(code,condition,return false)
#define ASSERT_NO_RET_NULL(x,condition)    ASSERT_NO_(x,condition,return NULL)
#define ASSERT_NO_RET_0(x,condition)        ASSERT_NO_(x,condition,return 0)
#define ASSERT_NO_RET_1(x,condition)        ASSERT_NO_(x,condition,return 1)
#define ASSERT_NO_RET_N1(x,condition)        ASSERT_NO_(x,condition,return -1)
#define ASSERT_NO_RET_(x,condition,retvalue)        ASSERT_NO_((x),(condition),return (retvalue))
#define ASSERT_NO_RET_TIP_(x,condition,retvalue,tip)        ASSERT_NO_((x),(condition),std::cerr << ANSI_BLUE_BRIGHT << tip << ANSI_RESET_LF;return (retvalue))
#define ASSERT_NO_RET(x,condition)        ASSERT_NO_(x,condition,return)
#define ASSERT_NO_EXIT_1(x,condition)        ASSERT_NO_(x,condition,_exit(1))
#define ASSERT_NO_DO_NOTHING(x,condition)        ASSERT_NO_(x,condition,{})
#define NOT_IMPLEMENTED false // used in assert
#define NOT_FINISHED false // used in assert

#ifndef STRCPY
    #define STRCPY(dst,src) do {if ((src) != nullptr) {strncpy(dst,src,sizeof(dst));dst[sizeof(dst)-1] = '\0';} else dst[0] = '\0';} while(0)
#endif
#define STRCPY_LINE(dst,src) do {const char *_NL = strchr((src),'\n');int _size = sizeof(dst);if (_NL != nullptr) _size = _NL - (src);strncpy(dst,src,_size);dst[sizeof(dst)-1] = '\0';} while(0)
#define MEMSET(dst,value) memset(dst,value,sizeof(dst))

#define LOG_ERROR(fmt,...) do { fflush(stdout);fprintf(stderr,"%s: " ANSI_RED_BRIGHT "Error: " ANSI_RED_DARK fmt ANSI_RESET_LF,__FUNCTION__,__VA_ARGS__);fflush(stderr);} while(0)
#define LOG(fmt,...) do {fflush(stdout);fprintf(stderr,ANSI_BLACK_BRIGHT "%s\t" ANSI_RESET fmt ANSI_RESET_LF,__FUNCTION__,__VA_ARGS__);fflush(stderr);} while (0)


#ifndef MINIMIZE
    #define MINIMIZE(x,y) do{if ((y) < (x)) (x) = (y);}while(0)
#endif
#ifndef MAXIMIZE
    #define MAXIMIZE(x,y) do{if ((y) > (x)) (x) = (y);}while(0)
#endif
#ifndef MAX
    #define MAX(x,y) ((x) > (y) ? (x) : (y))
#endif
#ifndef MIN
    #define MIN(x,y) ((x) < (y) ? (x) : (y))
#endif

#define LIMITE(v,min,max) do{if ((v) < (min)) (v) = (min);if ((v) > (max)) (v) = (max);}while(0)

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

#ifdef __cplusplus
    #define BEGIN_EXTERN_C extern "C" {
    #define END_EXTERN_C }
#else
    #define BEGIN_EXTERN_C
    #define END_EXTERN_C
#endif

#define elementsof(X) (sizeof(X) / sizeof(X[0]))

#define HEX2VALUE(x) ((x) >= '0' && (x) <= '9' ? (x) - '0' :\
                        ((x) >= 'a' && (x) <= 'f' ? (x) - 'a' + 10 :\
                        ((x) >= 'A' && (x) <= 'F' ? (x) - 'A' + 10 :\
                        -1)))
                       

//#define exit _exit
#endif
