/*
* @file CSAssoc.h
* @brief Simple shared useful base classes.
*/

#ifndef _INC_CSASSOC_H
#define _INC_CSASSOC_H

#include "CSObjList.h"
#include "CSFile.h"

#ifndef PT_REG_STRMAX
	#define PT_REG_STRMAX		128
#endif
#ifndef PT_REG_ROOTKEY
	#define PT_REG_ROOTKEY		HKEY_LOCAL_MACHINE
#endif
#ifndef OFFSETOF			// stddef.h ?
	#define OFFSETOF(s,m)   	(int)( (byte *)&(((s *)0)->m) - (byte *)0 )
#endif

// -----------------------------
//	CValStr
// -----------------------------

struct CValStr
{
	// Associate a val with a string.
	// Assume sorted values from min to max.
public:
	lpctstr m_pszName;
	int m_iVal;
public:
	void SetValues( int iVal, lpctstr pszName )
	{
		m_iVal = iVal;
		m_pszName = pszName;
	}
	lpctstr FindName( int iVal ) const;
	void SetValue( int iVal )
	{
		m_iVal = iVal;
	}
};

enum ELEM_TYPE	// define types of structure/record elements.
{
	ELEM_VOID = 0,	// unknown what this might be. (or just 'other') (must be handled manually)
	ELEM_CSTRING,	// Size prefix.
	ELEM_STRING,	// Assume max size of REG_SIZE. nullptr TERM string.
	ELEM_BOOL,		// bool = just 1 byte i guess.
	ELEM_BYTE,		// 1 byte.
	ELEM_MASK_BYTE,	// bits in a byte
	ELEM_WORD,		// 2 bytes
	ELEM_MASK_WORD,	// bits in a word
	ELEM_INT,		// Whatever the int size is. 4 i assume
	ELEM_MASK_INT,
	ELEM_DWORD,		// 4 bytes.
	ELEM_MASK_DWORD,	// bits in a dword

	ELEM_QTY
};


struct CElementDef
{
	static const int sm_Lengths[ELEM_QTY];
	ELEM_TYPE m_type;
	uint	m_offset;	// The offset into the class instance for this item.
	// ELEM_STRING = max size.
	// ELEM_MASK_WORD etc. = Extra masking info if needed. 
	dword   m_extra;

private:
	CElementDef& operator=(const CElementDef& other);

public:
	// get structure value.
	void * GetValPtr( const void * pBaseInst ) const;
	int GetValLength() const;

	bool GetValStr( const void * pBase, CSString & sVal ) const;
	bool SetValStr( void * pBase, lpctstr pszVal ) const;
};

class CAssocReg	// associate members of some class/structure with entries in the registry.
{
	// LAST = { nullptr, 0, ELEM_VOID }
public:
	lpctstr m_pszKey;	// A single key identifier to be cat to a base key. nullptr=last
	CElementDef m_elem;
public:
	static const char *m_sClassName;
private:
	CAssocReg& operator=(const CAssocReg& other);
public:
	operator lpctstr() const
	{
		return( m_pszKey );
	}
	// get structure value.
	void * GetValPtr( const void * pBaseInst ) const
	{
		return( m_elem.GetValPtr( pBaseInst ));
	}
};

////////////////////////////////////////////////////////////////////////

class CSStringListRec : public CSObjListRec, public CSString
{
	friend class CSStringList;
public:
	static const char *m_sClassName;
	explicit CSStringListRec( lpctstr pszVal ) : CSString( pszVal )
	{
	}
private:
	CSStringListRec(const CSStringListRec& copy);
	CSStringListRec& operator=(const CSStringListRec& other);
public:
	CSStringListRec * GetNext() const
	{
		return static_cast<CSStringListRec *>( CSObjListRec::GetNext() );
	}
};

class CSStringList : public CSObjList 	// obviously a list of strings.
{
public:
	static const char *m_sClassName;
	CSStringList() { };
	virtual ~CSStringList() { };
private:
	CSStringList(const CSStringList& copy);
	CSStringList& operator=(const CSStringList& other);
public:
	CSStringListRec * GetHead() const
	{
		return static_cast<CSStringListRec *>( CSObjList::GetHead() );
	}
	void AddHead( lpctstr pszVal )
	{
		InsertHead( new CSStringListRec( pszVal ));
	}
	void AddTail( lpctstr pszVal )
	{
		InsertTail( new CSStringListRec( pszVal ));
	}
};

#endif // _INC_CSASSOC_H
