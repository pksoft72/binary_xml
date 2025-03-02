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
#define INT16_NULL_VALUE -0x8000

#define MAX_FAILURES 16 // 0 is first failure, 1 second ... it should be enough for make some stack of failure

typedef struct 
{
    pid_t pid;
    int log_no;
    int log2_no;
    int failures[MAX_FAILURES];
    int fail_count;
} ProcessDebugStatus_t;

extern ProcessDebugStatus_t *debug_status; // automatically initialized to local variable, but it is better to redirect to shared memory

#define LOG_NO(x)   do { if (debug_status != nullptr) { debug_status->log_no = (x);debug_status->log2_no = 0;} } while(0)
#define LOG2_NO(x)  do { if (debug_status != nullptr) debug_status->log2_no = (x); } while(0)
#define FAIL(x)     do { if (debug_status != nullptr && debug_status->fail_count < MAX_FAILURES) debug_status->failures[debug_status->fail_count++] = (x); } while(0) 
                       

#define ERRNO_SHOW(code,command,params) do {FAIL(code);int __e = errno;std::cerr << ANSI_RED_BRIGHT "[" << (code) << "] Error: " command "(" << params << ") - " << __e << ": " << strerror(__e) << ANSI_RESET_LF;} while(0)
#define ERRNO_SHOW_EXPL(code,command,params,__e) do {FAIL(code);std::cerr << ANSI_RED_BRIGHT "[" << (code) << "] Error: " command "(" << params << ") - " << __e << ": " << strerror(__e) << ANSI_RESET_LF;} while(0)
#define ERRNO_SHOW_MSG(code,command,params,msg) do {FAIL(code);std::cerr << ANSI_RED_BRIGHT "[" << (code) << "] Error: " command "(" << params << ") - : " << msg << ANSI_RESET_LF;} while(0)

#define WORK_ID ""

#ifdef __cplusplus
    #ifndef NDEBUG
        #define ASSERT_NO_(code,condition,action) do {if (!(condition)) { FAIL(code);std::cerr << ANSI_RED_BRIGHT "[" << (code) << "] " << WORK_ID << ":" << __FUNCTION__ << ":" ANSI_RED_DARK " Assertion " << #condition << " failed!" ANSI_RESET_LF;action;}} while(0)
    #else
        #define ASSERT_NO_(code,condition,action) do {if (!(condition)) { FAIL(code);std::cerr << ANSI_RED_BRIGHT "[" << (code) << "] " << WORK_ID << ":" << __FUNCTION__ << ":" ANSI_RED_DARK " Assertion failed!" ANSI_RESET_LF;action;}} while(0)
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
    #define STRCPY(dst,src) do {if ((src) != nullptr) {strncpy(dst,src,sizeof(dst)-1);dst[sizeof(dst)-1] = '\0';} else dst[0] = '\0';} while(0)
#endif
#define STRCPY_LINE(dst,src) do {const char *_NL = strchr((src),'\n');int _size = sizeof(dst);if (_NL != nullptr) _size = _NL - (src);strncpy(dst,src,_size);dst[sizeof(dst)-1] = '\0';} while(0)
#define MEMSET(dst,value) memset(dst,value,sizeof(dst))

#define LOG_ERROR(fmt,...) do { fflush(stdout);fprintf(stderr,"%s: " ANSI_RED_BRIGHT "Error: " ANSI_RED_DARK fmt ANSI_RESET_LF,__FUNCTION__,__VA_ARGS__);fflush(stderr);} while(0)
#define LOG_WARNING(fmt,...) do { fflush(stdout);fprintf(stderr,"%s: " ANSI_MAGENTA_BRIGHT "Warning: " ANSI_RED_DARK fmt ANSI_RESET_LF,__FUNCTION__,__VA_ARGS__);fflush(stderr);} while(0)
#define LOG(fmt,...) do {fflush(stdout);fprintf(stderr,ANSI_BLACK_BRIGHT "%s\t" ANSI_RESET fmt ANSI_RESET_LF,__FUNCTION__,__VA_ARGS__);fflush(stderr);} while (0)

#define NSEC2MS_US(ns) (int)((ns) / 1000000),(int)((ns) / 1000 % 1000)

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


#ifndef ABS
    #define ABS(_x) ((_x) < 0 ? -(_x) : (_x))
#endif

#define LT_U32(a,b) ((int32_t)((uint32_t)(a) - (uint32_t)(b)) < 0)
#define GT_U32(a,b) ((int32_t)((uint32_t)(a) - (uint32_t)(b)) > 0)

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

#define BIT32(_b) ((uint32_t)1 << (_b))
#define BIT64(_b) ((uint64_t)1 << (_b))

//#define exit _exit
#endif
