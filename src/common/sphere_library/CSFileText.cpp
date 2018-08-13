#include "../../sphere/threads.h"
#include "../CLog.h"
#include "CSFileText.h"

#ifdef _WIN32
    #include <io.h> // for _get_osfhandle (used by STDFUNC_FILENO)
#endif

// CSFileText:: Constructors, Destructor, Asign operator.

CSFileText::CSFileText()
{
    _pStream = nullptr;
#ifdef _WIN32
    _fNoBuffer = false;
#endif
}

CSFileText::~CSFileText()
{
    Close();
}

// CSFileText:: File management.

bool CSFileText::_IsFileOpen() const
{
    return (_pStream != nullptr);
}
bool CSFileText::IsFileOpen() const
{
    THREAD_SHARED_LOCK_RETURN(_pStream != nullptr);
}

bool CSFileText::_Open(lpctstr ptcFilename, uint uiModeFlags)
{
    ADDTOCALLSTACK_INTENSIVE("CSFileText::_Open");

    // Open a file.
    _strFileName = ptcFilename;
    _uiMode = uiModeFlags;
    lpctstr ptcModeStr = _GetModeStr();
  
    _pStream = fopen( ptcFilename, ptcModeStr );
    if ( _pStream == nullptr )
        return false;

    // Get the file descriptor for it.
    _fileDescriptor = (file_descriptor_t)STDFUNC_FILENO(_pStream);

    return true;
}
bool CSFileText::Open(lpctstr ptcFilename, uint uiModeFlags)
{
    ADDTOCALLSTACK_INTENSIVE("CSFileText::Open");
    THREAD_UNIQUE_LOCK_RETURN(CSFileText::_Open(ptcFilename, uiModeFlags));
}

void CSFileText::_Close()
{
    ADDTOCALLSTACK_INTENSIVE("CSFileText::_Close");

    // CacheableScriptFile opens the file, reads and closes it. It should never be opened, so pStream should be always nullptr.
    if ((_pStream != nullptr) /*&& (m_llFile != _kInvalidFD)*/)
    {
        if (_IsWriteMode())
        {
            fflush(_pStream);
        }

        fclose(_pStream);
        _pStream = nullptr;
        _fileDescriptor = _kInvalidFD;
    }
}
void CSFileText::Close()
{
    ADDTOCALLSTACK_INTENSIVE("CSFileText::Close");
    THREAD_UNIQUE_LOCK_SET;
    _Close();
}

// CSFileText:: Content management.
int CSFileText::_Seek( int iOffset, int iOrigin )
{
    // RETURN:
    //  true = success
    ADDTOCALLSTACK_INTENSIVE("CSFileText::_Seek");

    if ( !_IsFileOpen() )
        return 0;
    if ( iOffset < 0 )
        return 0;

    if ( fseek(_pStream, iOffset, iOrigin) != 0 )
        return 0;

    long iPos = ftell(_pStream);
    if ( iPos < 0 )
    {
        return 0;
    }
    else if (iPos > INT_MAX)   // be consistent between windows and linux: support on both platforms at maximum an int (long has 4 or 8 bytes width, depending on the os)
    {
        _NotifyIOError("CSFileText::Seek (length)");
        return INT_MAX;
    }
    return (int)iPos;
}
int CSFileText::Seek( int iOffset, int iOrigin )
{
    // RETURN:
    //  true = success
    ADDTOCALLSTACK_INTENSIVE("CSFileText::Seek");
    THREAD_UNIQUE_LOCK_RETURN(CSFileText::_Seek(iOffset, iOrigin));
}

void CSFileText::_Flush() const
{
    ADDTOCALLSTACK_INTENSIVE("CSFileText::_Flush");

    if ( !_IsFileOpen() )
        return;

    ASSERT(_pStream);
    fflush(_pStream);
}
void CSFileText::Flush() const
{
    ADDTOCALLSTACK_INTENSIVE("CSFileText::Flush");
    THREAD_UNIQUE_LOCK_SET;
    CSFileText::_Flush();
}

bool CSFileText::_IsEOF() const
{
    ADDTOCALLSTACK("CSFileText::_IsEOF");

    if ( !_IsFileOpen() )
        return true;

    return (feof(_pStream) ? true : false);
}
bool CSFileText::IsEOF() const
{
    ADDTOCALLSTACK("CSFileText::IsEOF");
    THREAD_SHARED_LOCK_RETURN(CSFileText::_IsEOF());
}

int _cdecl CSFileText::Printf( lpctstr pFormat, ... )
{
    ADDTOCALLSTACK_INTENSIVE("CSFileText::Printf");
    ASSERT(pFormat);

    va_list vargs;
    va_start( vargs, pFormat );
    int iRet = VPrintf( pFormat, vargs );
    va_end( vargs );
    return iRet;
}

int CSFileText::Read( void * pBuffer, int sizemax ) const
{
    // This can return: EOF(-1) constant.
    // returns the number of full items actually read
    ADDTOCALLSTACK_INTENSIVE("CSFileText::Read");
    ASSERT(pBuffer);

    if ( IsEOF() )
        return 0;	// LINUX will ASSERT if we read past end.

    THREAD_UNIQUE_LOCK_SET;
    size_t ret = fread( pBuffer, 1, sizemax, _pStream );
    if (ret > INT_MAX)
    {
        _NotifyIOError("CSFileText::Read (length)");
        return 0;
    }
    return (int)ret;
}

tchar * CSFileText::_ReadString( tchar * pBuffer, int sizemax )
{
    // Read a line of text. NULL/nullptr = EOF
    ADDTOCALLSTACK_INTENSIVE("CSFileText::_ReadString");
    ASSERT(pBuffer);

    if ( IsEOF() )
        return nullptr;	// LINUX will ASSERT if we read past end.

    return fgets( pBuffer, sizemax, _pStream );
}

tchar * CSFileText::ReadString( tchar * pBuffer, int sizemax )
{
    ADDTOCALLSTACK_INTENSIVE("CSFileText::ReadString");
    THREAD_UNIQUE_LOCK_RETURN(_ReadString(pBuffer, sizemax));
}

int CSFileText::VPrintf( lpctstr pFormat, va_list args )
{
    ADDTOCALLSTACK_INTENSIVE("CSFileText::VPrintf");
    ASSERT(pFormat);

    if ( !IsFileOpen() )
        return -1;

    THREAD_UNIQUE_LOCK_SET;
    int lenret = vfprintf( _pStream, pFormat, args );
    return lenret;
}

bool CSFileText::Write( const void * pData, int iLen )
{
    // RETURN: 1 = success else fail.
    ADDTOCALLSTACK_INTENSIVE("CSFileText::Write");
    ASSERT(pData);

    THREAD_UNIQUE_LOCK_SET;
    if ( !_IsFileOpen() )
        return false;

#ifdef _WIN32 // Windows flushing, the only safe mode to cancel it ;)
    if ( !_fNoBuffer )
    {
        setvbuf(_pStream, NULL, _IONBF, 0);
        _fNoBuffer = true;
    }
#endif
    size_t uiStatus = fwrite( pData, iLen, 1, _pStream );
#ifndef _WIN32	// However, in unix, it works
    fflush( m_pStream );
#endif
    return ( uiStatus == 1 );
}

bool CSFileText::WriteString( lpctstr pStr )
{
    // RETURN: < 0 = failed.
    ADDTOCALLSTACK_INTENSIVE("CSFileText::WriteString");
    ASSERT(pStr);

    return Write( pStr, (int)strlen( pStr ) );
}

// CSFileText:: Mode operations.

lpctstr CSFileText::_GetModeStr() const
{
    ADDTOCALLSTACK_INTENSIVE("CSFileText::GetModeStr");
    // end of line translation is crap. ftell and fseek don't work correctly when you use it.
    // fopen() args
    if ( IsBinaryMode())
        return ( _IsWriteMode() ? "wb" : "rb" );
    if ( _GetMode() & OF_READWRITE )
        return "a+b";
    if ( _GetMode() & OF_CREATE )
        return "w";
    if ( _IsWriteMode() )
        return "w";
    else
        return "rb";	// don't parse out the \n\r
}
