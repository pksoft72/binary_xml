#pragma once

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
#define ASSERT_NO_RET(x,condition)        ASSERT_NO_(x,condition,return)
#define ASSERT_NO_EXIT_1(x,condition)        ASSERT_NO_(x,condition,_exit(1))
#define ASSERT_NO_DO_NOTHING(x,condition)        ASSERT_NO_(x,condition,{})


#define STRCPY(dst,src) do {strncpy(dst,src,sizeof(dst));dst[sizeof(dst)-1] = '\0';} while(0)

#define LOG_ERROR(fmt,...) fprintf(stderr,ANSI_RED_BRIGHT "Error:" fmt ANSI_RESET_LF,__VA_ARGS__)
#define LOG(fmt,...) fprintf(stderr,ANSI_BLACK_BRIGHT "%s\t" ANSI_RESET fmt ANSI_RESET_LF,__FUNCTION__,__VA_ARGS__)


#define MINIMIZE(x,y) do{if ((y) < (x)) (x) = (y);}while(0)
#define MAXIMIZE(x,y) do{if ((y) > (x)) (x) = (y);}while(0)
#define MAX(x,y) ((x) > (y) ? (x) : (y))
#define MIN(x,y) ((x) < (y) ? (x) : (y))

#define MAX(x,y) ((x) > (y) ? (x) : (y))
#define MIN(x,y) ((x) < (y) ? (x) : (y))

#ifdef __cplusplus
    #define BEGIN_EXTERN_C extern "C" {
    #define END_EXTERN_C }
#else
    #define BEGIN_EXTERN_C
    #define END_EXTERN_C
#endif


//#define exit _exit
