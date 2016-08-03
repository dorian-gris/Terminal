//
//    Copyright (C) Microsoft.  All rights reserved.
//
//
// Debug squirty functions
//

#include "precomp.h"

#define SZ_DEBUGINI     "cicero.ini"
#define SZ_DEBUGSECTION "CONIME"
#define SZ_MODULE       "CONIME"
#define DECLARE_DEBUG


#define NONTOSPINTERLOCK
//#include <ntosp.h>
//#include <ntrtl.h>
//#include <nturtl.h>

//#include "proj.h"
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include "debug.h"

#include "assert.h"
#pragma  hdrstop

#  ifndef LINE_SEPARATOR_STR
#  define LINE_SEPARATOR_STR       "\r\n"
#  endif
//#include <platform.h> // LINE_SEPARATOR_STR and friends
#include <winbase.h> // for GetModuleFileNameA

#include <strsafe.h>

#include "globals.h"





#define DM_DEBUG              0

#define WINCAPI __cdecl
#define DATASEG_READONLY    ".text"	        // don't use this, compiler does this for you


// Flags for StrToIntEx
#define STIF_DEFAULT        0x00000000L
#define STIF_SUPPORT_HEX    0x00000001L


#if defined(DEBUG) || defined(PRODUCT_PROF)
// (c_szCcshellIniFile and c_szCcshellIniSecDebug are declared in debug.h)
extern CHAR const FAR c_szCcshellIniFile[];
extern CHAR const FAR c_szCcshellIniSecDebug[];
HANDLE g_hDebugOutputFile = INVALID_HANDLE_VALUE;


void ShellDebugAppendToDebugFileA(LPCSTR pszOutputString)
{
    if (g_hDebugOutputFile != INVALID_HANDLE_VALUE)
    {
        DWORD cbWrite = (pszOutputString == NULL) ? 0 : strlen(pszOutputString);
        WriteFile(g_hDebugOutputFile, pszOutputString, cbWrite, &cbWrite, NULL);
    }
}

void ShellDebugAppendToDebugFileW(LPCWSTR pszOutputString)
{
    if (g_hDebugOutputFile != INVALID_HANDLE_VALUE)
    {
        char szBuf[500];

        DWORD cbWrite = WideCharToMultiByte(CP_ACP, 0, pszOutputString, -1, szBuf, ARRAYSIZE(szBuf), NULL, NULL);

        WriteFile(g_hDebugOutputFile, szBuf, cbWrite, &cbWrite, NULL);
    }
}

#if 1 // Looking at the assertW stuff, it delegates already to assertA -- I'm not sure
      // why I really need these wrappers!  (What was broken on my build?  I don't know,
      // but obviously the stuff below was half baked -- there are still problems.)
      // So I'm removing this for now so as to not bother anyone else...  [mikesh]
      //
      // Fixed a few problems and it worked for me. (edwardp)
      //
//
// We cannot link to shlwapi, because comctl32 cannot link to shlwapi.
// Duplicate some functions here so unicode stuff can run on Win95 platforms.
//
VOID MyOutputDebugStringWrapW(LPCWSTR lpOutputString)
{
    OutputDebugStringW(lpOutputString);
    ShellDebugAppendToDebugFileW(lpOutputString);
}
#define OutputDebugStringW MyOutputDebugStringWrapW

VOID MyOutputDebugStringWrapA(LPCSTR lpOutputString)
{
    OutputDebugStringA(lpOutputString);
    ShellDebugAppendToDebugFileA(lpOutputString);
}

#define OutputDebugStringA MyOutputDebugStringWrapA

LPWSTR MyCharPrevWrapW(LPCWSTR lpszStart, LPCWSTR lpszCurrent)
{
    if (lpszCurrent == lpszStart)
    {
        return (LPWSTR) lpszStart;
    }
    else
    {
        return (LPWSTR) lpszCurrent - 1;
    }
}
#define CharPrevW MyCharPrevWrapW

#endif


/*----------------------------------------------------------
Purpose: Special verion of atoi.  Supports hexadecimal too.

         If this function returns FALSE, *piRet is set to 0.

Returns: TRUE if the string is a number, or contains a partial number
         FALSE if the string is not a number

Cond:    --
*/
static
BOOL
MyStrToIntExA(
    LPCSTR    pszString,
    DWORD     dwFlags,          // STIF_ bitfield
    int FAR * piRet)
    {
    #define IS_DIGIT(ch)    InRange(ch, '0', '9')

    BOOL bRet;
    int n;
    BOOL bNeg = FALSE;
    LPCSTR psz;
    LPCSTR pszAdj;

    // Skip leading whitespace
    //
    for (psz = pszString; *psz == ' ' || *psz == '\n' || *psz == '\t'; psz = CharNextA(psz))
        ;

    // Determine possible explicit signage
    //
    if (*psz == '+' || *psz == '-')
        {
        bNeg = (*psz == '+') ? FALSE : TRUE;
        psz++;
        }

    // Or is this hexadecimal?
    //
    pszAdj = CharNextA(psz);
    if ((STIF_SUPPORT_HEX & dwFlags) &&
        *psz == '0' && (*pszAdj == 'x' || *pszAdj == 'X'))
        {
        // Yes

        // (Never allow negative sign with hexadecimal numbers)
        bNeg = FALSE;
        psz = CharNextA(pszAdj);

        pszAdj = psz;

        // Do the conversion
        //
        for (n = 0; ; psz = CharNextA(psz))
            {
            if (IS_DIGIT(*psz))
                n = 0x10 * n + *psz - '0';
            else
                {
                CHAR ch = *psz;
                int n2;

                if (ch >= 'a')
                    ch -= 'a' - 'A';

                n2 = ch - 'A' + 0xA;
                if (n2 >= 0xA && n2 <= 0xF)
                    n = 0x10 * n + n2;
                else
                    break;
                }
            }

        // Return TRUE if there was at least one digit
        bRet = (psz != pszAdj);
        }
    else
        {
        // No
        pszAdj = psz;

        // Do the conversion
        for (n = 0; IS_DIGIT(*psz); psz = CharNextA(psz))
            n = 10 * n + *psz - '0';

        // Return TRUE if there was at least one digit
        bRet = (psz != pszAdj);
        }

    *piRet = bNeg ? -n : n;

    return bRet;
    }

#endif

#ifdef DEBUG

EXTERN_C BOOL g_bUseNewLeakDetection = FALSE;

DWORD g_dwDumpFlags     = 0;        // DF_*

#ifdef FULL_DEBUG
DWORD g_dwTraceFlags    = 0;        // TF_*
#ifndef BREAK_ON_ASSERTS
#define BREAK_ON_ASSERTS
#endif
#else
DWORD g_dwTraceFlags    = 0;        // TF_*
#endif

#ifdef BREAK_ON_ASSERTS
DWORD g_dwBreakFlags    = BF_ASSERT;// BF_*
#else
DWORD g_dwBreakFlags    = 0;        // BF_*
#endif

DWORD g_dwPrototype     = 0;
DWORD g_dwFuncTraceFlags = 0;       // FTF_*

// TLS slot used to store depth for CcshellFuncMsg indentation

static DWORD g_tlsStackDepth = TLS_OUT_OF_INDEXES;

// Hack stack depth counter used when g_tlsStackDepth is not available

static DWORD g_dwHackStackDepth = 0;

static char g_szIndentLeader[] = "                                                                                ";

static WCHAR g_wszIndentLeader[] = L"                                                                                ";


static CHAR const FAR c_szNewline[] = LINE_SEPARATOR_STR;   // (Deliberately CHAR)
static WCHAR const FAR c_wszNewline[] = TEXTW(LINE_SEPARATOR_STR);

extern CHAR const FAR c_szTrace[];              // (Deliberately CHAR)
extern CHAR const FAR c_szErrorDbg[];           // (Deliberately CHAR)
extern CHAR const FAR c_szWarningDbg[];         // (Deliberately CHAR)
extern CHAR const FAR c_szGeneralDbg[];         // (Deliberately CHAR)
extern CHAR const FAR c_szFuncDbg[];            // (Deliberately CHAR)
extern CHAR const FAR c_szMemLeakDbg[];
extern WCHAR const FAR c_wszTrace[];
extern WCHAR const FAR c_wszErrorDbg[];
extern WCHAR const FAR c_wszWarningDbg[];
extern WCHAR const FAR c_wszGeneralDbg[];
extern WCHAR const FAR c_wszFuncDbg[];
extern WCHAR const FAR c_wszMemLeakDbg[];

extern const CHAR  FAR c_szAssertMsg[];
extern CHAR const FAR c_szAssertFailed[];
extern const WCHAR  FAR c_wszAssertMsg[];
extern WCHAR const FAR c_wszAssertFailed[];

extern CHAR const FAR c_szRip[];
extern CHAR const FAR c_szRipNoFn[];
extern WCHAR const FAR c_wszRip[];
extern WCHAR const FAR c_wszRipNoFn[];

extern int GetProcessInformationA(__out_ecount(dwszBufLen) LPSTR pszBuf, IN DWORD dwszBufLen);
extern int GetProcessInformationW(__out_ecount(dwszBufLen) LPWSTR pszBuf, IN DWORD dwszBufLen);

/*-------------------------------------------------------------------------
Purpose: Adds one of the following prefix strings to pszBuf:
           "t   MODULE  "
           "err MODULE  "
           "wrn MODULE  "

         Returns the count of characters written.
*/
int
SetPrefixStringA(
    __out_ecount(dwszBufLen) LPSTR pszBuf,
    IN  DWORD dwszBufLen,
    IN  DWORD dwFlags)
{
    GetProcessInformationA(pszBuf, dwszBufLen);

    if (TF_ALWAYS == dwFlags)
        StringCchCatA(pszBuf, dwszBufLen, c_szTrace);
    else if (DbgIsFlagSet(dwFlags, TF_WARNING))
        StringCchCatA(pszBuf, dwszBufLen, c_szWarningDbg);
    else if (DbgIsFlagSet(dwFlags, TF_ERROR))
        StringCchCatA(pszBuf, dwszBufLen, c_szErrorDbg);
    else if (DbgIsFlagSet(dwFlags, TF_GENERAL))
        StringCchCatA(pszBuf, dwszBufLen, c_szGeneralDbg);
    else if (DbgIsFlagSet(dwFlags, TF_FUNC))
        StringCchCatA(pszBuf, dwszBufLen, c_szFuncDbg);
    else if (DbgIsFlagSet(dwFlags, TF_MEMORY_LEAK))
        StringCchCatA(pszBuf, dwszBufLen, c_szMemLeakDbg);
    else
        StringCchCatA(pszBuf, dwszBufLen, c_szTrace);
    return strlen(pszBuf);
}


int
SetPrefixStringW(
    __out_ecount(dwszBufLen) LPWSTR pszBuf,
    IN  DWORD  dwszBufLen,
    IN  DWORD  dwFlags)
{
    GetProcessInformationW(pszBuf, dwszBufLen);

    if (TF_ALWAYS == dwFlags)
        StringCchCatW(pszBuf, dwszBufLen, c_wszTrace);
    else if (DbgIsFlagSet(dwFlags, TF_WARNING))
        StringCchCatW(pszBuf, dwszBufLen, c_wszWarningDbg);
    else if (DbgIsFlagSet(dwFlags, TF_ERROR))
        StringCchCatW(pszBuf, dwszBufLen, c_wszErrorDbg);
    else if (DbgIsFlagSet(dwFlags, TF_GENERAL))
        StringCchCatW(pszBuf, dwszBufLen, c_wszGeneralDbg);
    else if (DbgIsFlagSet(dwFlags, TF_FUNC))
        StringCchCatW(pszBuf, dwszBufLen, c_wszFuncDbg);
    else if (DbgIsFlagSet(dwFlags, TF_MEMORY_LEAK))
        StringCchCatW(pszBuf, dwszBufLen, c_wszMemLeakDbg);
    else
        StringCchCatW(pszBuf, dwszBufLen, c_wszTrace);
    return wcslen(pszBuf);
}


static
LPCSTR
_PathFindFileNameA(
    LPCSTR pPath)
{
    LPCSTR pT;

    for (pT = pPath; *pPath; pPath = CharNextA(pPath)) {
        if ((pPath[0] == '\\' || pPath[0] == ':' || pPath[0] == '/')
            && pPath[1] &&  pPath[1] != '\\'  &&   pPath[1] != '/')
            pT = pPath + 1;
    }

    return pT;
}


static
LPCWSTR
_PathFindFileNameW(
    LPCWSTR pPath)
{
    LPCWSTR pT;

    for (pT = pPath; *pPath; pPath++) {
        if ((pPath[0] == TEXTW('\\') || pPath[0] == TEXTW(':') || pPath[0] == TEXTW('/'))
            && pPath[1] &&  pPath[1] != TEXTW('\\')  &&   pPath[1] != TEXTW('/'))
            pT = pPath + 1;
    }

    return pT;
}



// Issue (scotth): Use the Ccshell functions.  _AssertMsg and
// _DebugMsg are obsolete.  They will be removed once all the
// components don't have TEXT() wrapping their debug strings anymore.


void
WINCAPI
_AssertMsgA(
    BOOL f,
    LPCSTR pszMsg, ...)
{
    CHAR ach[1024+40];
    va_list vArgs;

    if (!f)
    {
        int cch;

        StringCchCopyA(ach, ARRAYSIZE(ach), c_szAssertMsg);
        cch = strlen(ach);
        va_start(vArgs, pszMsg);

        StringCchVPrintfA(&ach[cch], ARRAYSIZE(ach) - cch, pszMsg, vArgs);

        va_end(vArgs);
        OutputDebugStringA(ach);

        OutputDebugStringA(c_szNewline);

        if (DbgIsFlagSet(g_dwBreakFlags, BF_ASSERT))
        {
            // MSDEV USERS:  This is not the real assert.  Hit
            //               Shift-F11 to jump back to the caller.
            DEBUG_BREAK;                                                            // ASSERT
        }
    }
}

void
WINCAPI
_AssertMsgW(
    BOOL f,
    LPCWSTR pszMsg, ...)
{
    WCHAR ach[1024+40];
    va_list vArgs;

    if (!f)
    {
        int cch;

        StringCchCopyW(ach, ARRAYSIZE(ach), c_wszAssertMsg);
        cch = wcslen(ach);
        va_start(vArgs, pszMsg);

        StringCchVPrintfW(&ach[cch], ARRAYSIZE(ach) - cch, pszMsg, vArgs);

        va_end(vArgs);
        OutputDebugStringW(ach);

        OutputDebugStringW(c_wszNewline);

        if (DbgIsFlagSet(g_dwBreakFlags, BF_ASSERT))
        {
            // MSDEV USERS:  This is not the real assert.  Hit
            //               Shift-F11 to jump back to the caller.
            DEBUG_BREAK;                                                            // ASSERT
        }
    }
}

void
_AssertStrLenW(
    LPCWSTR pszStr,
    int iLen)
{
    if (pszStr && iLen < wcslen(pszStr))
    {
        // MSDEV USERS:  This is not the real assert.  Hit
        //               Shift-F11 to jump back to the caller.
        DEBUG_BREAK;                                                            // ASSERT
    }
}

void
_AssertStrLenA(
    LPCSTR pszStr,
    int iLen)
{
    if (pszStr && iLen < strlen(pszStr))
    {
        // MSDEV USERS:  This is not the real assert.  Hit
        //               Shift-F11 to jump back to the caller.
        DEBUG_BREAK;                                                            // ASSERT
    }
}

void
WINCAPI
_DebugMsgA(
    DWORD flag,
    LPCSTR pszMsg, ...)
{
    CHAR ach[5*MAX_PATH+40];  // Handles 5*largest path + slop for message
    va_list vArgs;

    if (TF_ALWAYS == flag || (DbgIsFlagSet(g_dwTraceFlags, flag) && flag))
    {
        int cch;

        cch = SetPrefixStringA(ach, ARRAYSIZE(ach), flag);
        va_start(vArgs, pszMsg);

        try
        {
            StringCchVPrintfA(&ach[cch], ARRAYSIZE(ach) - cch, pszMsg, vArgs);
        }
        except(EXCEPTION_EXECUTE_HANDLER)
        {
            OutputDebugString(TEXT("CCSHELL: DebugMsg exception: "));
            OutputDebugStringA(pszMsg);
        }
        __endexcept

        va_end(vArgs);
        OutputDebugStringA(ach);
        OutputDebugStringA(c_szNewline);

        if (TF_ALWAYS != flag &&
            ((flag & TF_ERROR) && DbgIsFlagSet(g_dwBreakFlags, BF_ONERRORMSG) ||
             (flag & TF_WARNING) && DbgIsFlagSet(g_dwBreakFlags, BF_ONWARNMSG)))
        {
            // MSDEV USERS:  This is not the real assert.  Hit
            //               Shift-F11 to jump back to the caller.
            DEBUG_BREAK;                                                            // ASSERT
        }
    }
}

void
WINCAPI
_DebugMsgW(
    DWORD flag,
    LPCWSTR pszMsg, ...)
{
    WCHAR ach[5*MAX_PATH+40];  // Handles 5*largest path + slop for message
    va_list vArgs;

    if (TF_ALWAYS == flag || (DbgIsFlagSet(g_dwTraceFlags, flag) && flag))
    {
        int cch;

        SetPrefixStringW(ach, ARRAYSIZE(ach), flag);
        cch = wcslen(ach);
        va_start(vArgs, pszMsg);

        try
        {
            StringCchVPrintfW(&ach[cch], ARRAYSIZE(ach) - cch, pszMsg, vArgs);
        }
        except(EXCEPTION_EXECUTE_HANDLER)
        {
            OutputDebugString(TEXT("CCSHELL: DebugMsg exception: "));
            OutputDebugStringW(pszMsg);
        }
        __endexcept

        va_end(vArgs);
        OutputDebugStringW(ach);
        OutputDebugStringW(c_wszNewline);

        if (TF_ALWAYS != flag &&
            ((flag & TF_ERROR) && DbgIsFlagSet(g_dwBreakFlags, BF_ONERRORMSG) ||
             (flag & TF_WARNING) && DbgIsFlagSet(g_dwBreakFlags, BF_ONWARNMSG)))
        {
            // MSDEV USERS:  This is not the real assert.  Hit
            //               Shift-F11 to jump back to the caller.
            DEBUG_BREAK;                                                        // ASSERT
        }
    }
}


//
//  Smart debug functions
//



/*----------------------------------------------------------
Purpose: Displays assertion string.

Returns: TRUE to debugbreak
*/
BOOL
CcshellAssertFailedA(
    LPCSTR pszFile,
    int line,
    LPCSTR pszEval,
    BOOL bBreakInside,
    BOOL bPopupAssert)
{
    BOOL bRet = FALSE;
    LPCSTR psz;
    CHAR ach[256];
    int  len;

    len = GetProcessInformationA(ach, ARRAYSIZE(ach));

    psz = _PathFindFileNameA(pszFile);
    StringCchPrintfA(ach+len, ARRAYSIZE(ach) - len, c_szAssertFailed, psz, line, pszEval);
    OutputDebugStringA(ach);

    if (DbgIsFlagSet(g_dwBreakFlags, BF_ASSERT))
    {
        if (bBreakInside)
        {
            // !!!  ASSERT  !!!!  ASSERT  !!!!  ASSERT !!!

            // MSDEV USERS:  This is not the real assert.  Hit
            //               Shift-F11 to jump back to the caller.
            DEBUG_BREAK;                                                // ASSERT

            // !!!  ASSERT  !!!!  ASSERT  !!!!  ASSERT !!!
        }
        else
            bRet = TRUE;
    }

    return bRet;
}


/*----------------------------------------------------------
Purpose: Displays assertion string.

*/
BOOL
CcshellAssertFailedW(
    LPCWSTR pszFile,
    int line,
    LPCWSTR pszEval,
    BOOL bBreakInside,
    BOOL bPopupAssert)
{
    BOOL bRet = FALSE;
    LPCWSTR psz;
    WCHAR ach[1024];    // Some callers use more than 256
    int  len;

    len = GetProcessInformationW(ach, ARRAYSIZE(ach));

    psz = _PathFindFileNameW(pszFile);

    // If psz == NULL, CharPrevW failed which implies we are running on Win95.  We can get this
    // if we get an assert in some of the W functions in shlwapi...  Call the A version of assert...
    if (!psz)
    {
        char szFile[MAX_PATH];
        char szEval[256];   // since the total output is thhis size should be enough...

        WideCharToMultiByte(CP_ACP, 0, pszFile, -1, szFile, ARRAYSIZE(szFile), NULL, NULL);
        WideCharToMultiByte(CP_ACP, 0, pszEval, -1, szEval, ARRAYSIZE(szEval), NULL, NULL);
        return CcshellAssertFailedA(szFile, line, szEval, bBreakInside, bPopupAssert);
    }

    StringCchPrintfW(ach+len, ARRAYSIZE(ach) - len, c_wszAssertFailed, psz, line, pszEval);
    OutputDebugStringW(ach);

    if (DbgIsFlagSet(g_dwBreakFlags, BF_ASSERT))
    {
        if (bBreakInside)
        {
            // !!!  ASSERT  !!!!  ASSERT  !!!!  ASSERT !!!

            // MSDEV USERS:  This is not the real assert.  Hit
            //               Shift-F11 to jump back to the caller.
            DEBUG_BREAK;                                                // ASSERT

            // !!!  ASSERT  !!!!  ASSERT  !!!!  ASSERT !!!
        }
        else
            bRet = TRUE;
    }

    return bRet;
}


/*----------------------------------------------------------
Purpose: Keep track of the stack depth for function call trace
         messages.

*/
void
CcshellStackEnter(void)
    {
    if (TLS_OUT_OF_INDEXES != g_tlsStackDepth)
        {
        DWORD dwDepth;

        dwDepth = PtrToUlong(TlsGetValue(g_tlsStackDepth));

        TlsSetValue(g_tlsStackDepth, (LPVOID)(ULONG_PTR)(dwDepth + 1));
        }
    else
        {
        g_dwHackStackDepth++;
        }
    }


/*----------------------------------------------------------
Purpose: Keep track of the stack depth for functionc all trace
         messages.

*/
void
CcshellStackLeave(void)
    {
    if (TLS_OUT_OF_INDEXES != g_tlsStackDepth)
        {
        DWORD dwDepth;

        dwDepth = PtrToUlong(TlsGetValue(g_tlsStackDepth));

        if (EVAL(0 < dwDepth))
            {
            EVAL(TlsSetValue(g_tlsStackDepth, (LPVOID)(ULONG_PTR)(dwDepth - 1)));
            }
        }
    else
        {
        if (EVAL(0 < g_dwHackStackDepth))
            {
            g_dwHackStackDepth--;
            }
        }
    }


/*----------------------------------------------------------
Purpose: Return the stack depth.

*/
static
DWORD
CcshellGetStackDepth(void)
    {
    DWORD dwDepth;

    if (TLS_OUT_OF_INDEXES != g_tlsStackDepth)
        {
        dwDepth = PtrToUlong(TlsGetValue(g_tlsStackDepth));
        }
    else
        {
        dwDepth = g_dwHackStackDepth;
        }

    return dwDepth;
    }


/*----------------------------------------------------------
Purpose: This function converts a multi-byte string to a
         wide-char string.

         If pszBuf is non-NULL and the converted string can fit in
         pszBuf, then *ppszWide will point to the given buffer.
         Otherwise, this function will allocate a buffer that can
         hold the converted string.

         If pszAnsi is NULL, then *ppszWide will be freed.  Note
         that pszBuf must be the same pointer between the call
         that converted the string and the call that frees the
         string.

Returns: TRUE
         FALSE (if out of memory)

*/
BOOL
UnicodeFromAnsi(
    __out LPWSTR * ppwszWide,
    __in_opt LPCSTR pszAnsi,           // NULL to clean up
    __out_ecount_opt(cchBuf) LPWSTR pwszBuf,
    int cchBuf)
    {
    BOOL bRet;

    // Convert the string?
    if (pszAnsi)
        {
        // Yes; determine the converted string length
        int cch;
        LPWSTR pwsz;
        int cchAnsi = strlen(pszAnsi)+1;

        cch = MultiByteToWideChar(CP_ACP, 0, pszAnsi, cchAnsi, NULL, 0);

        // String too big, or is there no buffer?
        if (cch > cchBuf || NULL == pwszBuf)
            {
            // Yes; allocate space
            cchBuf = cch + 1;
            pwsz = (LPWSTR)LocalAlloc(LPTR, CbFromCchW(cchBuf));
            }
        else
            {
            // No; use the provided buffer
            pwsz = pwszBuf;
            }

        if (pwsz)
            {
            // Convert the string
            cch = MultiByteToWideChar(CP_ACP, 0, pszAnsi, cchAnsi, pwsz, cchBuf);
            bRet = (0 < cch);
            }
        else
            {
            bRet = FALSE;
            }

        *ppwszWide = pwsz;
        }
    else
        {
        // No; was this buffer allocated?
        if (*ppwszWide && pwszBuf != *ppwszWide)
            {
            // Yes; clean up
            LocalFree((HLOCAL)*ppwszWide);
            *ppwszWide = NULL;
            }
        bRet = TRUE;
        }

    return bRet;
    }


/*----------------------------------------------------------
Purpose: Wide-char version of CcshellAssertMsgA
*/
void
CDECL
CcshellAssertMsgW(
    BOOL f,
    LPCSTR pszMsg, ...)
{
    WCHAR ach[1024+40];    // Largest path plus extra
    va_list vArgs;

    if (!f)
        {
        int cch;
        WCHAR wszBuf[1024];
        LPWSTR pwsz;

        StringCchCopyW(ach, ARRAYSIZE(ach), c_wszAssertMsg);
        cch = wcslen(ach);
        va_start(vArgs, pszMsg);

        // (We convert the string, rather than simply input an
        // LPCWSTR parameter, so the caller doesn't have to wrap
        // all the string constants with the TEXT() macro.)

        if (UnicodeFromAnsi(&pwsz, pszMsg, wszBuf, SIZECHARS(wszBuf)))
            {
            StringCchVPrintfW(&ach[cch], ARRAYSIZE(ach) - cch, pwsz, vArgs);
            UnicodeFromAnsi(&pwsz, NULL, wszBuf, 0);
            }

        va_end(vArgs);
        OutputDebugStringW(ach);
        OutputDebugStringW(c_wszNewline);

        if (DbgIsFlagSet(g_dwBreakFlags, BF_ASSERT))
        {
            // !!!  ASSERT  !!!!  ASSERT  !!!!  ASSERT !!!

            // MSDEV USERS:  This is not the real assert.  Hit
            //               Shift-F11 to jump back to the caller.
            DEBUG_BREAK;                                                // ASSERT

            // !!!  ASSERT  !!!!  ASSERT  !!!!  ASSERT !!!
        }
    }
}


/*----------------------------------------------------------
Purpose: Wide-char version of CcshellDebugMsgA.  Note this
         function deliberately takes an ANSI format string
         so our trace messages don't all need to be wrapped
         in TEXT().

*/
void
CDECL
CcshellDebugMsgW(
    DWORD flag,
    LPCSTR pszMsg, ...)         // (this is deliberately CHAR)
{
    WCHAR ach[1024+40];    // Largest path plus extra
    va_list vArgs;

    if (TF_ALWAYS == flag || (DbgIsFlagSet(g_dwTraceFlags, flag) && flag))
    {
        int cch;
        WCHAR wszBuf[1024];
        LPWSTR pwsz;

        SetPrefixStringW(ach, ARRAYSIZE(ach), flag);
        cch = wcslen(ach);
        va_start(vArgs, pszMsg);

        // (We convert the string, rather than simply input an
        // LPCWSTR parameter, so the caller doesn't have to wrap
        // all the string constants with the TEXT() macro.)

        if (UnicodeFromAnsi(&pwsz, pszMsg, wszBuf, SIZECHARS(wszBuf)))
        {
            StringCchVPrintfW(&ach[cch], ARRAYSIZE(ach) - cch, pwsz, vArgs);
            UnicodeFromAnsi(&pwsz, NULL, wszBuf, 0);
        }

        va_end(vArgs);
        OutputDebugStringW(ach);
        OutputDebugStringW(c_wszNewline);

        if (TF_ALWAYS != flag &&
            ((flag & TF_ERROR) && DbgIsFlagSet(g_dwBreakFlags, BF_ONERRORMSG) ||
             (flag & TF_WARNING) && DbgIsFlagSet(g_dwBreakFlags, BF_ONWARNMSG)))
        {
            // MSDEV USERS:  This is not the real assert.  Hit
            //               Shift-F11 to jump back to the caller.
            DEBUG_BREAK;                                                // ASSERT
        }
    }
}


/*-------------------------------------------------------------------------
Purpose: Since the ATL code does not pass in a flag parameter,
         we'll hardcode and check for TF_ATL.
*/
void CDECL ShellAtlTraceW(LPCWSTR pszMsg, ...)
{
    WCHAR ach[1024+40];    // Largest path plus extra
    va_list vArgs;

    if (g_dwTraceFlags & TF_ATL)
    {
        int cch;

        SetPrefixStringW(ach, ARRAYSIZE(ach), TF_ATL);
        StringCchCatW(ach, ARRAYSIZE(ach), L"(ATL) ");
        cch = wcslen(ach);
        va_start(vArgs, pszMsg);
        StringCchVPrintfW(&ach[cch], ARRAYSIZE(ach) - cch, pszMsg, vArgs);
        va_end(vArgs);
        OutputDebugStringW(ach);
    }
}


/*----------------------------------------------------------
Purpose: Wide-char version of CcshellFuncMsgA.  Note this
         function deliberately takes an ANSI format string
         so our trace messages don't all need to be wrapped
         in TEXT().

*/
void
CDECL
CcshellFuncMsgW(
    DWORD flag,
    LPCSTR pszMsg, ...)         // (this is deliberately CHAR)
    {
    WCHAR ach[1024+40];    // Largest path plus extra
    va_list vArgs;

    if (DbgIsFlagSet(g_dwTraceFlags, TF_FUNC) &&
        DbgIsFlagSet(g_dwFuncTraceFlags, flag))
        {
        int cch;
        WCHAR wszBuf[1024];
        LPWSTR pwsz;
        DWORD dwStackDepth;
        LPWSTR pszLeaderEnd;
        WCHAR chSave;

        // Determine the indentation for trace message based on
        // stack depth.

        dwStackDepth = CcshellGetStackDepth();

        if (dwStackDepth < SIZECHARS(g_szIndentLeader))
            {
            pszLeaderEnd = &g_wszIndentLeader[dwStackDepth];
            }
        else
            {
            pszLeaderEnd = &g_wszIndentLeader[SIZECHARS(g_wszIndentLeader)-1];
            }

        chSave = *pszLeaderEnd;
        *pszLeaderEnd = '\0';

        StringCchPrintfW(ach, ARRAYSIZE(ach), L"%s %s", c_wszTrace, g_wszIndentLeader);
        *pszLeaderEnd = chSave;

        // Compose remaining string

        cch = wcslen(ach);
        va_start(vArgs, pszMsg);

        // (We convert the string, rather than simply input an
        // LPCWSTR parameter, so the caller doesn't have to wrap
        // all the string constants with the TEXT() macro.)

        if (UnicodeFromAnsi(&pwsz, pszMsg, wszBuf, SIZECHARS(wszBuf)))
            {
            StringCchVPrintfW(&ach[cch], ARRAYSIZE(ach) - cch, pwsz, vArgs);
            UnicodeFromAnsi(&pwsz, NULL, wszBuf, 0);
            }

        va_end(vArgs);
        OutputDebugStringW(ach);
        OutputDebugStringW(c_wszNewline);
        }
    }


/*----------------------------------------------------------
Purpose: Assert failed message only
*/
void
CDECL
CcshellAssertMsgA(
    BOOL f,
    LPCSTR pszMsg, ...)
{
    CHAR ach[1024+40];    // Largest path plus extra
    va_list vArgs;

    if (!f)
    {
        int cch;

        StringCchCopyA(ach, ARRAYSIZE(ach), c_szAssertMsg);
        cch = strlen(ach);
        va_start(vArgs, pszMsg);
        StringCchVPrintfA(&ach[cch], ARRAYSIZE(ach) - cch, pszMsg, vArgs);
        va_end(vArgs);
        OutputDebugStringA(ach);
        OutputDebugStringA(c_szNewline);

        if (DbgIsFlagSet(g_dwBreakFlags, BF_ASSERT))
        {
            // !!!  ASSERT  !!!!  ASSERT  !!!!  ASSERT !!!

            // MSDEV USERS:  This is not the real assert.  Hit
            //               Shift-F11 to jump back to the caller.
            DEBUG_BREAK;                                                // ASSERT

            // !!!  ASSERT  !!!!  ASSERT  !!!!  ASSERT !!!
        }
    }
}


/*----------------------------------------------------------
Purpose: Debug spew
*/
void
CDECL
CcshellDebugMsgA(
    DWORD flag,
    LPCSTR pszMsg, ...)
{
    CHAR ach[1024+40];    // Largest path plus extra
    va_list vArgs;

    if (TF_ALWAYS == flag || (DbgIsFlagSet(g_dwTraceFlags, flag) && flag))
    {
        int cch;

        cch = SetPrefixStringA(ach, ARRAYSIZE(ach), flag);
        va_start(vArgs, pszMsg);
        StringCchVPrintfA(&ach[cch], ARRAYSIZE(ach) - cch, pszMsg, vArgs);
        va_end(vArgs);
        OutputDebugStringA(ach);
        OutputDebugStringA(c_szNewline);

        if (TF_ALWAYS != flag &&
            ((flag & TF_ERROR) && DbgIsFlagSet(g_dwBreakFlags, BF_ONERRORMSG) ||
             (flag & TF_WARNING) && DbgIsFlagSet(g_dwBreakFlags, BF_ONWARNMSG)))
        {
            // MSDEV USERS:  This is not the real assert.  Hit
            //               Shift-F11 to jump back to the caller.
            DEBUG_BREAK;                                                // ASSERT
        }
    }
}


/*-------------------------------------------------------------------------
Purpose: Since the ATL code does not pass in a flag parameter,
         we'll hardcode and check for TF_ATL.
*/
void CDECL ShellAtlTraceA(LPCSTR pszMsg, ...)
{
    CHAR ach[1024+40];    // Largest path plus extra
    va_list vArgs;

    if (g_dwTraceFlags & TF_ATL)
    {
        int cch;

        SetPrefixStringA(ach, ARRAYSIZE(ach), TF_ATL);
        StringCchCatA(ach, ARRAYSIZE(ach), "(ATL) ");
        cch = strlen(ach);
        va_start(vArgs, pszMsg);
        StringCchVPrintfA(&ach[cch], ARRAYSIZE(ach) - cch, pszMsg, vArgs);
        va_end(vArgs);
        OutputDebugStringA(ach);
    }
}


/*----------------------------------------------------------
Purpose: Debug spew for function trace calls
*/
void
CDECL
CcshellFuncMsgA(
    DWORD flag,
    LPCSTR pszMsg, ...)
    {
    CHAR ach[1024+40];    // Largest path plus extra
    va_list vArgs;

    if (DbgIsFlagSet(g_dwTraceFlags, TF_FUNC) &&
        DbgIsFlagSet(g_dwFuncTraceFlags, flag))
        {
        int cch;
        DWORD dwStackDepth;
        LPSTR pszLeaderEnd;
        CHAR chSave;

        // Determine the indentation for trace message based on
        // stack depth.

        dwStackDepth = CcshellGetStackDepth();

        if (dwStackDepth < sizeof(g_szIndentLeader))
            {
            pszLeaderEnd = &g_szIndentLeader[dwStackDepth];
            }
        else
            {
            pszLeaderEnd = &g_szIndentLeader[sizeof(g_szIndentLeader)-1];
            }

        chSave = *pszLeaderEnd;
        *pszLeaderEnd = '\0';

        StringCchPrintfA(ach, ARRAYSIZE(ach), "%s %s", c_szTrace, g_szIndentLeader);
        *pszLeaderEnd = chSave;

        // Compose remaining string

        cch = strlen(ach);
        va_start(vArgs, pszMsg);
        StringCchVPrintfA(&ach[cch], ARRAYSIZE(ach) - cch, pszMsg, vArgs);
        va_end(vArgs);
        OutputDebugStringA(ach);
        OutputDebugStringA(c_szNewline);
        }
    }


/*-------------------------------------------------------------------------
Purpose: Spews a trace message if hrTest is a failure code.
*/
HRESULT
TraceHR(
    HRESULT hrTest,
    LPCSTR pszExpr,
    LPCSTR pszFile,
    int iLine)
{
    CHAR ach[1024+40];    // Largest path plus extra

    if (g_dwTraceFlags & TF_WARNING &&
        FAILED(hrTest))
    {
        int cch;

        cch = SetPrefixStringA(ach, ARRAYSIZE(ach), TF_WARNING);
        StringCchPrintfA(&ach[cch], ARRAYSIZE(ach) - cch, "THR: Failure of \"%s\" at %s, line %d (%#08lx)",
                         pszExpr, _PathFindFileNameA(pszFile), iLine, hrTest);
        OutputDebugStringA(ach);
        OutputDebugStringA(c_szNewline);

        if (DbgIsFlagSet(g_dwBreakFlags, BF_THR))
        {
            // !!!  THR  !!!!  THR  !!!!  THR !!!

            // MSDEV USERS:  This is not the real assert.  Hit
            //               Shift-F11 to jump back to the caller.
            DEBUG_BREAK;                                                // ASSERT

            // !!!  THR  !!!!  THR  !!!!  THR !!!
        }
    }
    return hrTest;
}


/*-------------------------------------------------------------------------
Purpose: Spews a trace message if bTest is false.
*/
BOOL
TraceBool(
    BOOL bTest,
    LPCSTR pszExpr,
    LPCSTR pszFile,
    int iLine)
{
    CHAR ach[1024+40];    // Largest path plus extra

    if (g_dwTraceFlags & TF_WARNING && !bTest)
    {
        int cch;

        cch = SetPrefixStringA(ach, ARRAYSIZE(ach),  TF_WARNING);
        StringCchPrintfA(&ach[cch], ARRAYSIZE(ach) - cch, "TBOOL: Failure of \"%s\" at %s, line %d",
                         pszExpr, _PathFindFileNameA(pszFile), iLine);
        OutputDebugStringA(ach);
        OutputDebugStringA(c_szNewline);

        if (DbgIsFlagSet(g_dwBreakFlags, BF_THR))
        {
            // !!!  TBOOL  !!!!  TBOOL  !!!!  TBOOL !!!

            // MSDEV USERS:  This is not the real assert.  Hit
            //               Shift-F11 to jump back to the caller.
            DEBUG_BREAK;                                                // ASSERT

            // !!!  TBOOL  !!!!  TBOOL  !!!!  TBOOL !!!
        }
    }
    return bTest;
}


/*-------------------------------------------------------------------------
Purpose: Spews a trace message if iTest is -1.
*/
int
TraceInt(
    int iTest,
    LPCSTR pszExpr,
    LPCSTR pszFile,
    int iLine)
{
    CHAR ach[1024+40];    // Largest path plus extra

    if (g_dwTraceFlags & TF_WARNING && -1 == iTest)
    {
        int cch;

        cch = SetPrefixStringA(ach, ARRAYSIZE(ach), TF_WARNING);
        StringCchPrintfA(&ach[cch], ARRAYSIZE(ach) - cch, "TINT: Failure of \"%s\" at %s, line %d",
                         pszExpr, _PathFindFileNameA(pszFile), iLine);
        OutputDebugStringA(ach);
        OutputDebugStringA(c_szNewline);

        if (DbgIsFlagSet(g_dwBreakFlags, BF_THR))
        {
            // !!!  TINT  !!!!  TINT  !!!!  TINT !!!

            // MSDEV USERS:  This is not the real assert.  Hit
            //               Shift-F11 to jump back to the caller.
            DEBUG_BREAK;                                                // ASSERT

            // !!!  TINT  !!!!  TINT  !!!!  TINT !!!
        }
    }
    return iTest;
}


/*-------------------------------------------------------------------------
Purpose: Spews a trace message if pvTest is NULL.
*/
LPVOID
TracePtr(
    LPVOID pvTest,
    LPCSTR pszExpr,
    LPCSTR pszFile,
    int iLine)
{
    CHAR ach[1024+40];    // Largest path plus extra

    if (g_dwTraceFlags & TF_WARNING && NULL == pvTest)
    {
        int cch;

        cch = SetPrefixStringA(ach, ARRAYSIZE(ach), TF_WARNING);
        StringCchPrintfA(&ach[cch], ARRAYSIZE(ach) - cch, "TPTR: Failure of \"%s\" at %s, line %d",
                         pszExpr, _PathFindFileNameA(pszFile), iLine);
        OutputDebugStringA(ach);
        OutputDebugStringA(c_szNewline);

        if (DbgIsFlagSet(g_dwBreakFlags, BF_THR))
        {
            // !!!  TPTR  !!!!  TPTR  !!!!  TPTR !!!

            // MSDEV USERS:  This is not the real assert.  Hit
            //               Shift-F11 to jump back to the caller.
            DEBUG_BREAK;                                                // ASSERT

            // !!!  TPTR  !!!!  TPTR  !!!!  TPTR !!!
        }
    }
    return pvTest;
}


/*-------------------------------------------------------------------------
Purpose: Spews a trace message if dwTest is a Win32 failure code.
*/
DWORD
TraceWin32(
    DWORD dwTest,
    LPCSTR pszExpr,
    LPCSTR pszFile,
    int iLine)
{
    CHAR ach[1024+40];    // Largest path plus extra

    if (g_dwTraceFlags & TF_WARNING &&
        ERROR_SUCCESS != dwTest)
    {
        int cch;

        cch = SetPrefixStringA(ach, ARRAYSIZE(ach), TF_WARNING);
        StringCchPrintfA(&ach[cch], ARRAYSIZE(ach) - cch, "TW32: Failure of \"%s\" at %s, line %d (%#08lx)",
                         pszExpr, _PathFindFileNameA(pszFile), iLine, dwTest);
        OutputDebugStringA(ach);
        OutputDebugStringA(c_szNewline);

        if (DbgIsFlagSet(g_dwBreakFlags, BF_THR))
        {
            // !!!  THR  !!!!  THR  !!!!  THR !!!

            // MSDEV USERS:  This is not the real assert.  Hit
            //               Shift-F11 to jump back to the caller.
            DEBUG_BREAK;                                                // ASSERT

            // !!!  THR  !!!!  THR  !!!!  THR !!!
        }
    }
    return dwTest;
}


/*-------------------------------------------------------------------------
Purpose:
*/
int GetProcessInformationA(__out_ecount(dwszBufLen) LPSTR pszBuf, DWORD dwszBufLen)
{
    WCHAR buf[1024];
    int result;

    result = GetProcessInformationW(buf, ARRAYSIZE(buf));

    result = WideCharToMultiByte(CP_ACP,     // code page
                                 0,          // performance and mapping flags
                                 buf,        // address of wide-char string
                                 (int)result,    // number of char string
                                 pszBuf,         // address of buffer for new string
                                 dwszBufLen,     // size of buffer
                                 NULL,     // default for unmappable char
                                 NULL);    // flag set when default char
    pszBuf[result] = '\0';

    return result;
}

int GetProcessInformationW(__out_ecount(dwszBufLen) LPWSTR pszBuf, IN DWORD dwszBufLen)
{
    static CONST WCHAR *wszUnknown = L"???";
    WCHAR              *pwszImage;
    ULONG              ulLenImage;

    DWORD       dwT;
    DWORD       dwP;
    DWORD       dwSession;

    PTEB        pteb;
    PPEB        ppeb;

    if (pteb = NtCurrentTeb()) {
        dwT = HandleToUlong(pteb->ClientId.UniqueThread);
        dwP = HandleToUlong(pteb->ClientId.UniqueProcess);
    } else {
        dwT = dwP = 0;
    }

    if ((ppeb = NtCurrentPeb()) && ppeb->ProcessParameters != NULL) {
        /*
         * Get Pointers to the image path
         */
        pwszImage = ppeb->ProcessParameters->ImagePathName.Buffer;
        ulLenImage = (ppeb->ProcessParameters->ImagePathName.Length) / sizeof(WCHAR);

        /*
         * If the ProcessParameters haven't been normalized yet, then do it.
         */
        if (pwszImage != NULL && !(ppeb->ProcessParameters->Flags & RTL_USER_PROC_PARAMS_NORMALIZED)) {
            pwszImage = (PWSTR)((PCHAR)(pwszImage) + (ULONG_PTR)(ppeb->ProcessParameters));
        }

        /*
         * Munge out the path part.
         */
        if (pwszImage != NULL && ulLenImage != 0) {
            PWSTR pwszT = pwszImage + (ulLenImage - 1);
            ULONG ulLenT = 1;

            while (ulLenT != ulLenImage && *(pwszT-1) != L'\\') {
                pwszT--;
                ulLenT++;
            }

            pwszImage = pwszT;
            ulLenImage = ulLenT;
        }

    } else {
        pwszImage = (PWSTR)wszUnknown;
        ulLenImage = 3;
    }

    {
        PPEB ppeb = NtCurrentPeb();

        dwSession = (ppeb != NULL ? ppeb->SessionId : 0);
    }

    StringCchPrintfW(pszBuf, dwszBufLen, L"(s: %d %#lx.%lx %s) ", dwSession, dwP, dwT, pwszImage);

    return wcslen(pszBuf);
}

//
//  Debug .ini functions
//


#pragma data_seg(DATASEG_READONLY)

// (These are deliberately CHAR)
CHAR const FAR c_szNull[] = "";
CHAR const FAR c_szZero[] = "0";
CHAR const FAR c_szIniKeyBreakFlags[] = "BreakFlags";
CHAR const FAR c_szIniKeyTraceFlags[] = "TraceFlags";
CHAR const FAR c_szIniKeyFuncTraceFlags[] = "FuncTraceFlags";
CHAR const FAR c_szIniKeyDumpFlags[] = "DumpFlags";
CHAR const FAR c_szIniKeyProtoFlags[] = "Prototype";

#pragma data_seg()


// Some of the .ini processing code was pimped from the sync engine.
//

typedef struct _INIKEYHEADER
    {
    LPCTSTR pszSectionName;
    LPCTSTR pszKeyName;
    LPCTSTR pszDefaultRHS;
    } INIKEYHEADER;

typedef struct _BOOLINIKEY
    {
    INIKEYHEADER ikh;
    LPDWORD puStorage;
    DWORD dwFlag;
    } BOOLINIKEY;

typedef struct _INTINIKEY
    {
    INIKEYHEADER ikh;
    LPDWORD puStorage;
    } INTINIKEY;


#define PutIniIntCmp(idsSection, idsKey, nNewValue, nSave) \
    if ((nNewValue) != (nSave)) PutIniInt(idsSection, idsKey, nNewValue)

#define WritePrivateProfileInt(szApp, szKey, i, lpFileName) \
    {CHAR sz[7]; \
    WritePrivateProfileString(szApp, szKey, SzFromInt(sz, i), lpFileName);}


#ifdef BOOL_INI_VALUES
/* Boolean TRUE strings used by IsIniYes() (comparison is case-insensitive) */

static LPCTSTR s_rgpszTrue[] =
    {
    TEXT("1"),
    TEXT("On"),
    TEXT("True"),
    TEXT("Y"),
    TEXT("Yes")
    };

/* Boolean FALSE strings used by IsIniYes() (comparison is case-insensitive) */

static LPCTSTR s_rgpszFalse[] =
    {
    TEXT("0"),
    TEXT("Off"),
    TEXT("False"),
    TEXT("N"),
    TEXT("No")
    };
#endif


#ifdef BOOL_INI_VALUES
/*----------------------------------------------------------
Purpose: Determines whether a string corresponds to a boolean
          TRUE value.
Returns: The boolean value (TRUE or FALSE)
*/
BOOL
PRIVATE
IsIniYes(
    LPCTSTR psz)
    {
    int i;
    BOOL bNotFound = TRUE;
    BOOL bResult;

    Assert(psz);

    /* Is the value TRUE? */

    for (i = 0; i < ARRAYSIZE(s_rgpszTrue); i++)
        {
        if (IsSzEqual(psz, s_rgpszTrue[i]))
            {
            bResult = TRUE;
            bNotFound = FALSE;
            break;
            }
        }

    /* Is the value FALSE? */

    if (bNotFound)
        {
        for (i = 0; i < ARRAYSIZE(s_rgpszFalse); i++)
            {
            if (IsSzEqual(psz, s_rgpszFalse[i]))
                {
                bResult = FALSE;
                bNotFound = FALSE;
                break;
                }
            }

        /* Is the value a known string? */

        if (bNotFound)
            {
            /* No.  Whine about it. */

            TraceMsg(TF_WARNING, "IsIniYes() called on unknown Boolean RHS '%s'.", psz);
            bResult = FALSE;
            }
        }

    return bResult;
    }


/*----------------------------------------------------------
Purpose: Process keys with boolean RHSs.
*/
void
PRIVATE
ProcessBooleans(void)
    {
    int i;

    for (i = 0; i < ARRAYSIZE(s_rgbik); i++)
        {
        DWORD dwcbKeyLen;
        TCHAR szRHS[MAX_BUF];
        BOOLINIKEY * pbik = &(s_rgbik[i]);
        LPCTSTR lpcszRHS;

        /* Look for key. */

        dwcbKeyLen = GetPrivateProfileString(pbik->ikh.pszSectionName,
                                   pbik->ikh.pszKeyName, TEXT(""), szRHS,
                                   SIZECHARS(szRHS), c_szCcshellIniFile);

        if (dwcbKeyLen)
            lpcszRHS = szRHS;
        else
            lpcszRHS = pbik->ikh.pszDefaultRHS;

        if (IsIniYes(lpcszRHS))
            {
            if (IsFlagClear(*(pbik->puStorage), pbik->dwFlag))
                TraceMsg(TF_GENERAL, "ProcessIniFile(): %s set in %s![%s].",
                         pbik->ikh.pszKeyName,
                         c_szCcshellIniFile,
                         pbik->ikh.pszSectionName);

            SetFlag(*(pbik->puStorage), pbik->dwFlag);
            }
        else
            {
            if (IsFlagSet(*(pbik->puStorage), pbik->dwFlag))
                TraceMsg(TF_GENERAL, "ProcessIniFile(): %s cleared in %s![%s].",
                         pbik->ikh.pszKeyName,
                         c_szCcshellIniFile,
                         pbik->ikh.pszSectionName);

            ClearFlag(*(pbik->puStorage), pbik->dwFlag);
            }
        }
    }
#endif



/*----------------------------------------------------------
Purpose: This function converts a wide-char string to a multi-byte
         string.

         If pszBuf is non-NULL and the converted string can fit in
         pszBuf, then *ppszAnsi will point to the given buffer.
         Otherwise, this function will allocate a buffer that can
         hold the converted string.

         If pszWide is NULL, then *ppszAnsi will be freed.  Note
         that pszBuf must be the same pointer between the call
         that converted the string and the call that frees the
         string.

Returns: TRUE
         FALSE (if out of memory)

*/
static
BOOL
MyAnsiFromUnicode(
    __deref_out LPSTR * ppszAnsi,
    LPCWSTR pwszWide,        // NULL to clean up
    __out_ecount(cchBuf) LPSTR pszBuf,
    int cchBuf)
    {
    BOOL bRet;

    // Convert the string?
    if (pwszWide)
        {
        // Yes; determine the converted string length
        int cch;
        LPSTR psz;

        cch = WideCharToMultiByte(CP_ACP, 0, pwszWide, -1, NULL, 0, NULL, NULL);

        // String too big, or is there no buffer?
        if (cch > cchBuf || NULL == pszBuf)
            {
            // Yes; allocate space
            cchBuf = cch + 1;
            psz = (LPSTR)LocalAlloc(LPTR, CbFromCchA(cchBuf));
            }
        else
            {
            // No; use the provided buffer
            Assert(pszBuf);
            psz = pszBuf;
            }

        if (psz)
            {
            // Convert the string
            cch = WideCharToMultiByte(CP_ACP, 0, pwszWide, -1, psz, cchBuf, NULL, NULL);
            bRet = (0 < cch);
            }
        else
            {
            bRet = FALSE;
            }

        *ppszAnsi = psz;
        }
    else
        {
        // No; was this buffer allocated?
        if (*ppszAnsi && pszBuf != *ppszAnsi)
            {
            // Yes; clean up
            LocalFree((HLOCAL)*ppszAnsi);
            *ppszAnsi = NULL;
            }
        bRet = TRUE;
        }

    return bRet;
    }


#ifdef UNICODE

/*----------------------------------------------------------
Purpose: Wide-char wrapper for StrToIntExA.

Returns: see StrToIntExA
*/
static
BOOL
MyStrToIntExW(
    LPCWSTR   pwszString,
    DWORD     dwFlags,          // STIF_ bitfield
    int FAR * piRet)
    {
    // Most strings will simply use this temporary buffer, but AnsiFromUnicode
    // will allocate a buffer if the supplied string is bigger.
    CHAR szBuf[MAX_PATH];

    LPSTR pszString;
    BOOL bRet = MyAnsiFromUnicode(&pszString, pwszString, szBuf, SIZECHARS(szBuf));

    if (bRet)
        {
        bRet = MyStrToIntExA(pszString, dwFlags, piRet);
        MyAnsiFromUnicode(&pszString, NULL, szBuf, 0);
        }
    return bRet;
    }
#endif // UNICODE


#ifdef UNICODE
#define MyStrToIntEx        MyStrToIntExW
#else
#define MyStrToIntEx        MyStrToIntExA
#endif


const TCHAR c_szDimmWrpKey[] = TEXT("SOFTWARE\\Microsoft\\Cicero\\DebugFlag\\");

DWORD GetGlobalDebugFlag(const char *p)
{
    HKEY hKey;
    DWORD dwType;
    DWORD dwSize;
    DWORD dw = 0;

    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, c_szDimmWrpKey, 0,
                     KEY_READ, &hKey) != ERROR_SUCCESS)
    {
        return dw;
    }


    dwType = REG_DWORD;
    dwSize = sizeof(DWORD);

    if (RegQueryValueExA(hKey, p, 0, &dwType,
                         (LPBYTE)&dw, &dwSize) != ERROR_SUCCESS)
        dw = 0;

    RegCloseKey(hKey);
    return dw;
}


/*----------------------------------------------------------
Purpose: This function reads a .ini file to determine the debug
         flags to set.  The .ini file and section are specified
         by the following manifest constants:

                SZ_DEBUGINI
                SZ_DEBUGSECTION

         The debug variables that are set by this function are
         g_dwDumpFlags, g_dwTraceFlags, g_dwBreakFlags, and
         g_dwFuncTraceFlags, g_dwPrototype.

Returns: TRUE if initialization is successful
*/
BOOL
PUBLIC
CcshellGetDebugFlags(void)
    {
    CHAR szRHS[MAX_PATH];
    int val;

    // Issue (scotth): Yes, COMCTL32 exports StrToIntEx, but I
    //  don't want to cause a dependency delta and force everyone
    //  to get a new comctl32 just because they built debug.
    //  So use a local version of StrToIntEx.

    // Trace Flags

    GetPrivateProfileStringA(c_szCcshellIniSecDebug,
                            c_szIniKeyTraceFlags,
                            c_szNull,
                            szRHS,
                            SIZECHARS(szRHS),
                            c_szCcshellIniFile);

    if (MyStrToIntExA(szRHS, STIF_SUPPORT_HEX, &val))
        g_dwTraceFlags = (DWORD)val;
#ifdef FULL_DEBUG
    else
        g_dwTraceFlags = 3; // default to TF_ERROR and TF_WARNING trace messages
#endif

    g_dwTraceFlags |= GetGlobalDebugFlag(c_szIniKeyTraceFlags);

    TraceMsgA(DM_DEBUG, "CcshellGetDebugFlags(): %s set to %#08x.",
             c_szIniKeyTraceFlags, g_dwTraceFlags);

    // Function trace Flags

    GetPrivateProfileStringA(c_szCcshellIniSecDebug,
                            c_szIniKeyFuncTraceFlags,
                            c_szNull,
                            szRHS,
                            SIZECHARS(szRHS),
                            c_szCcshellIniFile);

    if (MyStrToIntExA(szRHS, STIF_SUPPORT_HEX, &val))
        g_dwFuncTraceFlags = (DWORD)val;

    g_dwFuncTraceFlags |= GetGlobalDebugFlag(c_szIniKeyFuncTraceFlags);

    TraceMsgA(DM_DEBUG, "CcshellGetDebugFlags(): %s set to %#08x.",
             c_szIniKeyFuncTraceFlags, g_dwFuncTraceFlags);

    // Dump Flags

    GetPrivateProfileStringA(c_szCcshellIniSecDebug,
                            c_szIniKeyDumpFlags,
                            c_szNull,
                            szRHS,
                            SIZECHARS(szRHS),
                            c_szCcshellIniFile);

    if (MyStrToIntExA(szRHS, STIF_SUPPORT_HEX, &val))
        g_dwDumpFlags = (DWORD)val;

    g_dwDumpFlags |= GetGlobalDebugFlag(c_szIniKeyDumpFlags);

    TraceMsgA(DM_DEBUG, "CcshellGetDebugFlags(): %s set to %#08x.",
             c_szIniKeyDumpFlags, g_dwDumpFlags);

    // Break Flags

    GetPrivateProfileStringA(c_szCcshellIniSecDebug,
                            c_szIniKeyBreakFlags,
                            c_szNull,
                            szRHS,
                            SIZECHARS(szRHS),
                            c_szCcshellIniFile);

    if (MyStrToIntExA(szRHS, STIF_SUPPORT_HEX, &val))
        g_dwBreakFlags = (DWORD)val;
#ifdef FULL_DEBUG
    else
        g_dwBreakFlags = 5; // default to break on ASSERT and TF_ERROR
#endif

    g_dwBreakFlags |= GetGlobalDebugFlag(c_szIniKeyBreakFlags);

    TraceMsgA(DM_DEBUG, "CcshellGetDebugFlags(): %s set to %#08x.",
             c_szIniKeyBreakFlags, g_dwBreakFlags);

    // Prototype Flags

    GetPrivateProfileStringA(c_szCcshellIniSecDebug,
                            c_szIniKeyProtoFlags,
                            c_szNull,
                            szRHS,
                            SIZECHARS(szRHS),
                            c_szCcshellIniFile);

    if (MyStrToIntExA(szRHS, STIF_SUPPORT_HEX, &val))
        g_dwPrototype = (DWORD)val;

    // Are we using the new leak detection from shelldbg.dll?
    GetPrivateProfileStringA("ShellDbg",
                            "NewLeakDetection",
                            c_szNull,
                            szRHS,
                            SIZECHARS(szRHS),
                            c_szCcshellIniFile);

    if (MyStrToIntExA(szRHS, STIF_SUPPORT_HEX, &val))
        g_bUseNewLeakDetection = BOOLIFY(val);

    TraceMsgA(DM_DEBUG, "CcshellGetDebugFlags(): %s set to %#08x.",
             c_szIniKeyProtoFlags, g_dwPrototype);

    GetPrivateProfileStringA(c_szCcshellIniSecDebug,
                            "DebugOutputFile",
                            c_szNull,
                            szRHS,
                            SIZECHARS(szRHS),
                            c_szCcshellIniFile);
    if (szRHS != TEXT('\0'))
    {
        g_hDebugOutputFile = CreateFileA(szRHS, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    }

    return TRUE;
    }

#endif // DEBUG

#ifdef PRODUCT_PROF

DWORD g_dwProfileCAP = 0;

BOOL PUBLIC CcshellGetDebugFlags(void)
{
    CHAR szRHS[MAX_PATH];
    int val;

    GetPrivateProfileStringA(c_szCcshellIniSecDebug,
                            "Profile",
                            "",
                            szRHS,
                            SIZECHARS(szRHS),
                            c_szCcshellIniFile);

    if (MyStrToIntExA(szRHS, STIF_SUPPORT_HEX, &val))
        g_dwProfileCAP = (DWORD)val;

    return TRUE;
}
#endif // PRODUCT_PROF
