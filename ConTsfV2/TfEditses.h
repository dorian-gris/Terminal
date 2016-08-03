/*++

Copyright (c) 2003, Microsoft Corporation

Module Name:

    TfEditses.h

Abstract:

    This file defines the CEditSessionObject Class.

Author:

Revision History:

Notes:

--*/

#pragma once

class CicCategoryMgr;
class CicDisplayAttributeMgr;

/* 183C627A-B46C-44ad-B797-82F6BEC82131 */
const GUID GUID_PROP_CONIME_TRACKCOMPOSITION = {
    0x183c627a,
    0xb46c,
    0x44ad,
    {0xb7, 0x97, 0x82, 0xf6, 0xbe, 0xc8, 0x21, 0x31}
};

//+---------------------------------------------------------------------------
//
// CEditSessionObject
//
//----------------------------------------------------------------------------

class CEditSessionObject : public ITfEditSession
{
public:
    CEditSessionObject() : m_cRef(1) {}
    virtual ~CEditSessionObject() { };

public:
    //
    // IUnknown methods
    //
    STDMETHODIMP QueryInterface(REFIID riid, void **ppvObj);
    STDMETHODIMP_(ULONG) AddRef(void);
    STDMETHODIMP_(ULONG) Release(void);

    //
    // ITfEditSession method
    //
    STDMETHODIMP DoEditSession(TfEditCookie ec)
    {
        HRESULT hr = _DoEditSession(ec);
        Release();        // Release reference count for asynchronous edit session.
        return hr;
    }

    //
    // ImmIfSessionObject methods
    //
protected:
    virtual HRESULT _DoEditSession(TfEditCookie ec) = 0;

    //
    // EditSession methods.
    //
public:
    static HRESULT GetAllTextRange(TfEditCookie ec, ITfContext* ic, ITfRange** range, LONG* lpTextLength, TF_HALTCOND* lpHaltCond=NULL);

protected:
    HRESULT SetTextInRange(TfEditCookie ec, ITfRange* range, __in_ecount_opt(len) LPWSTR psz, DWORD len);
    HRESULT ClearTextInRange(TfEditCookie ec, ITfRange* range);

    HRESULT _GetTextAndAttribute(TfEditCookie ec, ITfRange* range,
                                 CCompString& CompStr, CCompTfGuidAtom CompGuid,
                                 BOOL bInWriteSession,
                                 CicCategoryMgr* pCicCatMgr, CicDisplayAttributeMgr* pCicDispAttr)
    {
        CCompString ResultStr;
        return _GetTextAndAttribute(ec, range,
                                    CompStr, CompGuid, ResultStr,
                                    bInWriteSession,
                                    pCicCatMgr, pCicDispAttr);
    }

    HRESULT _GetTextAndAttribute(TfEditCookie ec, ITfRange* range,
                                 CCompString& CompStr, CCompTfGuidAtom& CompGuid, CCompString& ResultStr,
                                 BOOL bInWriteSession,
                                 CicCategoryMgr* pCicCatMgr, CicDisplayAttributeMgr* pCicDispAttr);


    HRESULT _GetTextAndAttributeGapRange(TfEditCookie ec, ITfRange* gap_range, LONG result_comp,
                                         CCompString& CompStr, CCompTfGuidAtom& CompGuid, CCompString& ResultStr);

    HRESULT _GetTextAndAttributePropertyRange(TfEditCookie ec, ITfRange* pPropRange,
                                              BOOL fDispAttribute,
                                              LONG result_comp,
                                              BOOL bInWriteSession,
                                              TF_DISPLAYATTRIBUTE da,
                                              TfGuidAtom guidatom,
                                              CCompString& CompStr, CCompTfGuidAtom& CompGuid, CCompString& ResultStr);

    HRESULT _GetNoDisplayAttributeRange(TfEditCookie ec, ITfRange* range,
                                        const GUID** guids,
                                        const int guid_size,
                                        ITfRange* no_display_attribute_range);

    HRESULT _GetCursorPosition(TfEditCookie ec,
                               CCompCursorPos& CompCursorPos);

private:
    int                       m_cRef;
};

//+---------------------------------------------------------------------------
//
// CEditSessionCompositionComplete
//
//----------------------------------------------------------------------------

class CEditSessionCompositionComplete : public CEditSessionObject
{
public:
    CEditSessionCompositionComplete() { }

    HRESULT _DoEditSession(TfEditCookie ec)
    {
        return CompComplete(ec);
    }

    HRESULT CompComplete(TfEditCookie ec);
};


//+---------------------------------------------------------------------------
//
// CEditSessionCompositionCleanup
//
//----------------------------------------------------------------------------

class CEditSessionCompositionCleanup : public CEditSessionObject
{
public:
    CEditSessionCompositionCleanup() { }

    HRESULT _DoEditSession(TfEditCookie ec)
    {
        return EmptyCompositionRange(ec);
    }

    HRESULT EmptyCompositionRange(TfEditCookie ec);
};


//+---------------------------------------------------------------------------
//
// CEditSessionUpdateCompositionString
//
//----------------------------------------------------------------------------

class CEditSessionUpdateCompositionString : public CEditSessionObject
{
public:
    CEditSessionUpdateCompositionString() {}

    HRESULT _DoEditSession(TfEditCookie ec)
    {
        return UpdateCompositionString(ec);
    }

    HRESULT UpdateCompositionString(TfEditCookie ec);

private:
    HRESULT _IsInterimSelection(TfEditCookie ec, ITfRange** pInterimRange, BOOL *pfInterim);

    HRESULT _MakeCompositionString(TfEditCookie ec, ITfRange* FullTextRange, BOOL bInWriteSession,
                                   CicCategoryMgr* pCicCatMgr, CicDisplayAttributeMgr* pCicDispAttr);

    HRESULT _MakeInterimString(TfEditCookie ec, ITfRange* FullTextRange, ITfRange* InterimRange, LONG lTextLength, BOOL bInWriteSession,
                               CicCategoryMgr* pCicCatMgr, CicDisplayAttributeMgr* pCicDispAttr);

    HRESULT _CreateCategoryAndDisplayAttributeManager(CicCategoryMgr** pCicCatMgr, CicDisplayAttributeMgr** pCicDispAttr);
};
