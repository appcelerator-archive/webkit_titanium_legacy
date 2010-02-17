/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef StringImpl_h
#define StringImpl_h

#include <limits.h>
#include <wtf/ASCIICType.h>
#include <wtf/CrossThreadRefCounted.h>
#include <wtf/Noncopyable.h>
#include <wtf/OwnFastMallocPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/StringHashFunctions.h>
#include <wtf/Vector.h>
#include <wtf/unicode/Unicode.h>

#if PLATFORM(CF)
typedef const struct __CFString * CFStringRef;
#endif

#ifdef __OBJC__
@class NSString;
#endif

namespace JSC {
class UString;
}

namespace WebCore {

class StringBuffer;

struct CStringTranslator;
struct HashAndCharactersTranslator;
struct StringHash;
struct UCharBufferTranslator;

enum TextCaseSensitivity { TextCaseSensitive, TextCaseInsensitive };

typedef bool (*CharacterMatchFunctionPtr)(UChar);

class StringImpl : public Noncopyable {
    friend struct CStringTranslator;
    friend struct HashAndCharactersTranslator;
    friend struct UCharBufferTranslator;
private:
    friend class ThreadGlobalData;

    enum BufferOwnership {
        BufferInternal,
        BufferOwned,
        BufferShared,
    };

    typedef CrossThreadRefCounted<OwnFastMallocPtr<UChar> > SharedUChar;

    // Used to create the empty string (""), automatically hashes.
    StringImpl();
    // Create a StringImpl with internal storage (BufferInternal)
    StringImpl(unsigned length);
    // Create a StringImpl adopting ownership of the provided buffer (BufferOwned)
    StringImpl(const UChar*, unsigned length);
    // Create a StringImpl using a shared buffer (BufferShared)
    StringImpl(const UChar*, unsigned length, PassRefPtr<SharedUChar>);

    // For use only by AtomicString's XXXTranslator helpers.
    void setHash(unsigned hash) { ASSERT(!m_hash); m_hash = hash; }
    
public:
    ~StringImpl();

    static PassRefPtr<StringImpl> create(const UChar*, unsigned length);
    static PassRefPtr<StringImpl> create(const char*, unsigned length);
    static PassRefPtr<StringImpl> create(const char*);
    static PassRefPtr<StringImpl> createUninitialized(unsigned length, UChar*& data);

    static PassRefPtr<StringImpl> createWithTerminatingNullCharacter(const StringImpl&);

    static PassRefPtr<StringImpl> createStrippingNullCharacters(const UChar*, unsigned length);
    static PassRefPtr<StringImpl> adopt(StringBuffer&);
    static PassRefPtr<StringImpl> adopt(Vector<UChar>&);
#if USE(JSC)
    static PassRefPtr<StringImpl> create(const JSC::UString&);
    JSC::UString ustring();
#endif

    SharedUChar* sharedBuffer();
    const UChar* characters() { return m_data; }
    unsigned length() { return m_length; }

    bool hasTerminatingNullCharacter() const { return m_refCountAndFlags & s_refCountFlagHasTerminatingNullCharacter; }

    bool inTable() const { return m_refCountAndFlags & s_refCountFlagInTable; }
    void setInTable() { m_refCountAndFlags |= s_refCountFlagInTable; }

    unsigned hash() { if (m_hash == 0) m_hash = computeHash(m_data, m_length); return m_hash; }
    unsigned existingHash() const { ASSERT(m_hash); return m_hash; }
    inline static unsigned computeHash(const UChar* data, unsigned length) { return WTF::stringHash(data, length); }
    inline static unsigned computeHash(const char* data) { return WTF::stringHash(data); }

    StringImpl* ref() { m_refCountAndFlags += s_refCountIncrement; return this; }
    ALWAYS_INLINE void deref() { m_refCountAndFlags -= s_refCountIncrement; if (!(m_refCountAndFlags & s_refCountMask)) delete this; }
    ALWAYS_INLINE bool hasOneRef() const { return (m_refCountAndFlags & s_refCountMask) == s_refCountIncrement; }

    // Returns a StringImpl suitable for use on another thread.
    PassRefPtr<StringImpl> crossThreadString();
    // Makes a deep copy. Helpful only if you need to use a String on another thread
    // (use crossThreadString if the method call doesn't need to be threadsafe).
    // Since StringImpl objects are immutable, there's no other reason to make a copy.
    PassRefPtr<StringImpl> threadsafeCopy() const;

    PassRefPtr<StringImpl> substring(unsigned pos, unsigned len = UINT_MAX);

    UChar operator[](unsigned i) { ASSERT(i < m_length); return m_data[i]; }
    UChar32 characterStartingAt(unsigned);

    bool containsOnlyWhitespace();

    int toIntStrict(bool* ok = 0, int base = 10);
    unsigned toUIntStrict(bool* ok = 0, int base = 10);
    int64_t toInt64Strict(bool* ok = 0, int base = 10);
    uint64_t toUInt64Strict(bool* ok = 0, int base = 10);
    intptr_t toIntPtrStrict(bool* ok = 0, int base = 10);

    int toInt(bool* ok = 0); // ignores trailing garbage
    unsigned toUInt(bool* ok = 0); // ignores trailing garbage
    int64_t toInt64(bool* ok = 0); // ignores trailing garbage
    uint64_t toUInt64(bool* ok = 0); // ignores trailing garbage
    intptr_t toIntPtr(bool* ok = 0); // ignores trailing garbage

    double toDouble(bool* ok = 0);
    float toFloat(bool* ok = 0);

    PassRefPtr<StringImpl> lower();
    PassRefPtr<StringImpl> upper();
    PassRefPtr<StringImpl> secure(UChar aChar);
    PassRefPtr<StringImpl> capitalize(UChar previousCharacter);
    PassRefPtr<StringImpl> foldCase();

    PassRefPtr<StringImpl> stripWhiteSpace();
    PassRefPtr<StringImpl> simplifyWhiteSpace();

    PassRefPtr<StringImpl> removeCharacters(CharacterMatchFunctionPtr);

    int find(const char*, int index = 0, bool caseSensitive = true);
    int find(UChar, int index = 0);
    int find(CharacterMatchFunctionPtr, int index = 0);
    int find(StringImpl*, int index, bool caseSensitive = true);

    int reverseFind(UChar, int index);
    int reverseFind(StringImpl*, int index, bool caseSensitive = true);
    
    bool startsWith(StringImpl* str, bool caseSensitive = true) { return reverseFind(str, 0, caseSensitive) == 0; }
    bool endsWith(StringImpl*, bool caseSensitive = true);

    PassRefPtr<StringImpl> replace(UChar, UChar);
    PassRefPtr<StringImpl> replace(UChar, StringImpl*);
    PassRefPtr<StringImpl> replace(StringImpl*, StringImpl*);
    PassRefPtr<StringImpl> replace(unsigned index, unsigned len, StringImpl*);

    static StringImpl* empty();

    Vector<char> ascii();

    WTF::Unicode::Direction defaultWritingDirection();

#if PLATFORM(CF)
    CFStringRef createCFString();
#endif
#ifdef __OBJC__
    operator NSString*();
#endif

private:
    using Noncopyable::operator new;
    void* operator new(size_t, void* inPlace) { ASSERT(inPlace); return inPlace; }

    static PassRefPtr<StringImpl> createStrippingNullCharactersSlowCase(const UChar*, unsigned length);
    
    // The StringImpl struct and its data may be allocated within a single heap block.
    // In this case, the m_data pointer is an "internal buffer", and does not need to be deallocated.
    BufferOwnership bufferOwnership() const { return static_cast<BufferOwnership>(m_refCountAndFlags & s_refCountMaskBufferOwnership); }

    static const unsigned s_refCountMask = 0xFFFFFFF0;
    static const unsigned s_refCountIncrement = 0x10;
    static const unsigned s_refCountFlagHasTerminatingNullCharacter = 0x8;
    static const unsigned s_refCountFlagInTable = 0x4;
    static const unsigned s_refCountMaskBufferOwnership = 0x3;

    const UChar* m_data;
    SharedUChar* m_sharedBuffer;
    unsigned m_length;
    unsigned m_refCountAndFlags;
    mutable unsigned m_hash;
};

bool equal(StringImpl*, StringImpl*);
bool equal(StringImpl*, const char*);
inline bool equal(const char* a, StringImpl* b) { return equal(b, a); }

bool equalIgnoringCase(StringImpl*, StringImpl*);
bool equalIgnoringCase(StringImpl*, const char*);
inline bool equalIgnoringCase(const char* a, StringImpl* b) { return equalIgnoringCase(b, a); }
bool equalIgnoringCase(const UChar* a, const char* b, unsigned length);
inline bool equalIgnoringCase(const char* a, const UChar* b, unsigned length) { return equalIgnoringCase(b, a, length); }

bool equalIgnoringNullity(StringImpl*, StringImpl*);

static inline bool isSpaceOrNewline(UChar c)
{
    // Use isASCIISpace() for basic Latin-1.
    // This will include newlines, which aren't included in Unicode DirWS.
    return c <= 0x7F ? WTF::isASCIISpace(c) : WTF::Unicode::direction(c) == WTF::Unicode::WhiteSpaceNeutral;
}

// This is a hot function because it's used when parsing HTML.
inline PassRefPtr<StringImpl> StringImpl::createStrippingNullCharacters(const UChar* characters, unsigned length)
{
    ASSERT(characters);
    ASSERT(length);

    // Optimize for the case where there are no Null characters by quickly
    // searching for nulls, and then using StringImpl::create, which will
    // memcpy the whole buffer.  This is faster than assigning character by
    // character during the loop. 

    // Fast case.
    int foundNull = 0;
    for (unsigned i = 0; !foundNull && i < length; i++) {
        int c = characters[i]; // more efficient than using UChar here (at least on Intel Mac OS)
        foundNull |= !c;
    }
    if (!foundNull)
        return StringImpl::create(characters, length);

    return StringImpl::createStrippingNullCharactersSlowCase(characters, length);
}

}

namespace WTF {

    // WebCore::StringHash is the default hash for StringImpl* and RefPtr<StringImpl>
    template<typename T> struct DefaultHash;
    template<> struct DefaultHash<WebCore::StringImpl*> {
        typedef WebCore::StringHash Hash;
    };
    template<> struct DefaultHash<RefPtr<WebCore::StringImpl> > {
        typedef WebCore::StringHash Hash;
    };

}

#endif
