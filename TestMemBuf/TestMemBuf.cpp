// TestMemBuf.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"


#ifdef _STATIC_MEM_BUF
#define MEM_BUF StaticMemBuf
#else
#define MEM_BUF DunamicMemBuf
#endif


inline void* AllocBuf(size_t size, size_t minSize)
{
    minSize = min(size, minSize);
    size_t allocSize = size;
    void * buf = nullptr;
    while (allocSize >= minSize)
    {
#if defined(_FILE_MAP_MEM_BUF) && defined(_STATIC_MEM_BUF)
        LARGE_INTEGER li;
        li.QuadPart = allocSize;
        buf = nullptr;
        HANDLE hMap;
        if (NULL == (hMap = CreateFileMapping(INVALID_HANDLE_VALUE, 0, PAGE_READWRITE, li.HighPart, li.LowPart, NULL))
            || NULL == (buf = (char*)MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, 0)))
        {
            if (hMap)
                CloseHandle(hMap);
        }
#else
        buf = malloc(allocSize);
#endif
        if (buf == nullptr)
            allocSize = (allocSize / 5) * 4 + 1;
        else
            break;
    }
    size = buf ? allocSize : 0;
    return buf;
}




class StaticMemBuf
{
public:
    StaticMemBuf(size_t maxBufSize)
    {
        ZeroMemory(this, sizeof(*this));
        if (!m_Initialized)
        {
            m_Initialized = true;
            InitializeCriticalSectionAndSpinCount(&m_cs, 0x00000400);
            m_bufSize = maxBufSize;
            m_buf = (char*)AllocBuf(m_bufSize, 512 * 1024 * 1024);
        }
    }

    void * Alloc(size_t size)
    {
        char* buf = nullptr;
        EnterCriticalSection(&m_cs);
        if (m_buf && m_curPos + size < m_bufSize)
        {
            buf = m_buf + m_curPos;
            m_curPos += size;
        }
        LeaveCriticalSection(&m_cs);
        return buf;
    }

    size_t UsedMemory() { return m_curPos; }

private:
    size_t m_curPos;
    static bool m_Initialized;
    static CRITICAL_SECTION m_cs;
    static size_t m_bufSize;
    static char* m_buf;
};

class DunamicMemBuf
{
public:
    DunamicMemBuf(size_t maxBufSize)
    {
        ZeroMemory(this, sizeof(*this));
        m_allocSize = 128 * 1024 * 1024;
        InitializeCriticalSectionAndSpinCount(&m_cs, 0x00000400);
        size_t chankCount = max(1, (unsigned long)(maxBufSize / m_allocSize));
        m_MemBufChank = chankCount ? (MemBufChank*)calloc(chankCount, sizeof(MemBufChank)) : nullptr;
        m_chankCount = m_MemBufChank ? chankCount : 0;
        m_chankEnd = m_MemBufChank ? (m_MemBufChank + m_chankCount) : nullptr;
        m_curChank = m_MemBufChank;
    }

    ~DunamicMemBuf()
    {
        MemBufChank * chank = m_MemBufChank;
        DWORD chanckCount = 0;
        while (chank <= m_curChank && chank < m_chankEnd)
        {
            free(chank->buf);
            chank++;
            chanckCount++;
        }
        DeleteCriticalSection(&m_cs);
    }

    void * Alloc(size_t size, bool zero = false)
    {
        void * buf = nullptr;
        EnterCriticalSection(&m_cs);
        if (m_curChank < m_chankEnd)
        {
            buf = _Alloc(size);
            if (buf == nullptr)
            {
                m_curChank++;
                if (m_curChank < m_chankEnd)
                {
                    buf = _Alloc(size);
                    if (buf)
                        m_usedMem += size;
                }
            }
        }
        LeaveCriticalSection(&m_cs);
        if (zero && buf)
            ZeroMemory(buf, size);
        return buf;
    }

    size_t UsedMemory() { return m_usedMem; }

private:
    void * _Alloc(size_t size)
    {
        if (m_curChank == m_chankEnd)
            return nullptr;

        if (m_curChank->buf == nullptr)
        {
            size_t allocSize = max(m_allocSize, size);
            if (m_curChank->buf = AllocBuf(allocSize, size))
            {
                m_curChank->size = allocSize;
            }
            else
            {
                m_chankEnd = m_curChank;
                return nullptr;
            }
        }

        if (size > (m_curChank->size - m_curChank->used))
        {
            return nullptr;
        }

        void * buf = (char*)m_curChank->buf + m_curChank->used;
        m_curChank->used += size;
        return buf;
    }

    struct MemBufChank
    {
        void * buf;
        size_t size;
        size_t used;
    };

    CRITICAL_SECTION m_cs;
    MemBufChank * m_MemBufChank;
    MemBufChank * m_chankEnd;
    MemBufChank * m_curChank;
    size_t m_chankCount;
    size_t m_allocSize;
    size_t m_usedMem;
};

template <typename Type> class PtrArray
{
public:
    PtrArray(MEM_BUF* pMemBuf)
    {
        ZeroMemory(this, sizeof(*this));
        m_pMemBuf = pMemBuf;
        //m_Array = (Type**)m_pMemBuf->Alloc(10000 * sizeof(void**));
    }

    const Type* Add(const Type* p)
    {
        if (m_IsFull)
            return nullptr;
        Pointers* pp = &m_Pointers;
        BYTE*  pc = m_c;
        for (int i = 0; i < 4; i++)
        {
            if (*pc == 0xFF)
            {
                m_IsFull = true;
                return nullptr;
            }
            if (pp->p == nullptr)
            {
                if (m_IsFull = (nullptr == (pp->p = (Pointers*)m_pMemBuf->Alloc(0xFF * sizeof(Pointers), true))))
                    return nullptr;
                (*pc)++;
            }
            pp = &(pp->p[*pc - 1]);
            pc++;
        }
        pp->p = (Pointers*)p;
        m_Count++;
        return p;
    }

    Type* Add(DWORD size, bool zero = true)
    {
        Type* p = (Type*)m_pMemBuf->Alloc(size, zero);
        if (p == nullptr)
            return nullptr;
        return (Type*)Add(p);
    }

    Type* Get(DWORD i)
    {
        if (i >= m_Count)
            return nullptr;
        Indexes ind;
        ind.dw = i;

        return (Type*)(m_Pointers.p[ind.c[3]].p[ind.c[2]].p[ind.c[1]].p[ind.c[0]].p);
    }

    unsigned long Count() { return m_Count; }

private:
#pragma pack(push,4)
    union Indexes {
        DWORD dw;
        BYTE c[4];
    };
    struct Pointers {
        Pointers* p;
    };
#pragma pack(pop)

    MEM_BUF * m_pMemBuf;
    bool m_IsFull;
    DWORD m_Count;
    BYTE m_c[4];
    Pointers m_Pointers;
};

///////////////////////////////////////////////////////////////////
int main()
{
    MEM_BUF buf(1024*1024);
    PtrArray<char> arr(&buf);

    arr.Add("123");

    char p1[10] = { '4',0 };
    arr.Add(p1);

    char* p2 = new char[10];
    strcpy_s(p2, 10, "56");
    arr.Add(p2);

    char* p3 = arr.Add(5);
    strcpy_s(p3, 5, "78");


    char* pp0 = arr.Get(0);
    char* pp1 = arr.Get(1);
    char* pp2 = arr.Get(2);
    char* pp3 = arr.Get(3);
    return 0;
}

//unsigned short short11 = 1024;
//bitset<16> bitset11{ short11 };
//cout << bitset11 << endl;     // 0000010000000000  
//
//unsigned short short12 = short11 >> 1;  // 512  
//bitset<16> bitset12{ short12 };
//cout << bitset12 << endl;     // 0000001000000000