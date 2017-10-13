//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#ifdef DEBUG

// TODO: add Android logcat support
// TODO: use this file PAL wide

#define QUOTE(s) #s
#define STRINGIZE(s) QUOTE(s)

#define Assert(exp)   \
do { \
if (!(exp)) \
{ \
    fprintf(stderr, "ASSERTION (%s, line %d) %s\n", __FILE__, __LINE__, STRINGIZE(exp)); \
    fflush(stderr); \
    abort(); \
} \
} while (0)

#define AssertMsg(exp, comment)   \
do { \
if (!(exp)) \
{ \
    fprintf(stderr, "ASSERTION (%s, line %d) %s %s\n", __FILE__, __LINE__, STRINGIZE(exp), comment); \
    fflush(stderr); \
    abort(); \
} \
} while (0)

#else // ! DEBUG

#define Assert(condition)
#define AssertMsg(condition, msg)

#endif
