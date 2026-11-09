#ifndef KOI7_H
#define KOI7_H

/*
 * TO_KOI7(L"...") expands to a static const byte string at compile time.
 * The UKNC console path is 7-bit here; SO/SI switch Russian/Latin glyph sets.
 */
#ifndef KOI7_MAX_CHARS
#define KOI7_MAX_CHARS 128
#endif

#define _K7_LEN(wstr) (sizeof(wstr) / sizeof((wstr)[0]) - 1)
#define _K7_ASSERT_LEN(wstr) \
	((void)sizeof(char[(_K7_LEN(wstr) <= KOI7_MAX_CHARS) ? 1 : -1]))
#define _K7_POS(wstr, i) (((i) < _K7_LEN(wstr)) ? ((i) * 2) : (_K7_LEN(wstr) * 2))
#define _K7_WCHAR(wstr, i) (((i) < _K7_LEN(wstr)) ? (wstr)[i] : L'\0')
#define _K7_IS_RUS(wc) \
	(((wc) >= L'А' && (wc) <= L'Я') || ((wc) >= L'а' && (wc) <= L'я') || \
	 (wc) == L'Ё' || (wc) == L'ё')
#define _K7_MODE(wc) (_K7_IS_RUS(wc) ? '\016' : '\017')
#define _K7_BYTE(wc) \
	((wc) < 128 ? (wc) : \
	 (wc) == L'А' ? 0x61 : (wc) == L'Б' ? 0x62 : (wc) == L'В' ? 0x77 : \
	 (wc) == L'Г' ? 0x67 : (wc) == L'Д' ? 0x64 : (wc) == L'Е' ? 0x65 : \
	 (wc) == L'Ё' ? 0x65 : (wc) == L'Ж' ? 0x76 : (wc) == L'З' ? 0x7a : \
	 (wc) == L'И' ? 0x69 : (wc) == L'Й' ? 0x6a : (wc) == L'К' ? 0x6b : \
	 (wc) == L'Л' ? 0x6c : (wc) == L'М' ? 0x6d : (wc) == L'Н' ? 0x6e : \
	 (wc) == L'О' ? 0x6f : (wc) == L'П' ? 0x70 : (wc) == L'Р' ? 0x72 : \
	 (wc) == L'С' ? 0x73 : (wc) == L'Т' ? 0x74 : (wc) == L'У' ? 0x75 : \
	 (wc) == L'Ф' ? 0x66 : (wc) == L'Х' ? 0x68 : (wc) == L'Ц' ? 0x63 : \
	 (wc) == L'Ч' ? 0x7e : (wc) == L'Ш' ? 0x7b : (wc) == L'Щ' ? 0x7d : \
	 (wc) == L'Ъ' ? 0x7f : (wc) == L'Ы' ? 0x79 : (wc) == L'Ь' ? 0x78 : \
	 (wc) == L'Э' ? 0x7c : (wc) == L'Ю' ? 0x60 : (wc) == L'Я' ? 0x71 : \
	 (wc) == L'а' ? 0x41 : (wc) == L'б' ? 0x42 : (wc) == L'в' ? 0x57 : \
	 (wc) == L'г' ? 0x47 : (wc) == L'д' ? 0x44 : (wc) == L'е' ? 0x45 : \
	 (wc) == L'ё' ? 0x45 : (wc) == L'ж' ? 0x56 : (wc) == L'з' ? 0x5a : \
	 (wc) == L'и' ? 0x49 : (wc) == L'й' ? 0x4a : (wc) == L'к' ? 0x4b : \
	 (wc) == L'л' ? 0x4c : (wc) == L'м' ? 0x4d : (wc) == L'н' ? 0x4e : \
	 (wc) == L'о' ? 0x4f : (wc) == L'п' ? 0x50 : (wc) == L'р' ? 0x52 : \
	 (wc) == L'с' ? 0x53 : (wc) == L'т' ? 0x54 : (wc) == L'у' ? 0x55 : \
	 (wc) == L'ф' ? 0x46 : (wc) == L'х' ? 0x48 : (wc) == L'ц' ? 0x43 : \
	 (wc) == L'ч' ? 0x5e : (wc) == L'ш' ? 0x5b : (wc) == L'щ' ? 0x5d : \
	 (wc) == L'ъ' ? 0x5f : (wc) == L'ы' ? 0x59 : (wc) == L'ь' ? 0x58 : \
	 (wc) == L'э' ? 0x5c : (wc) == L'ю' ? 0x40 : (wc) == L'я' ? 0x51 : '?')

#define _K7_STEP(wstr, i) \
	[_K7_POS(wstr, i)] = (((i) < _K7_LEN(wstr)) ? _K7_MODE(_K7_WCHAR(wstr, i)) : '\0'), \
	[_K7_POS(wstr, i) + (((i) < _K7_LEN(wstr)) ? 1 : 0)] = \
		(((i) < _K7_LEN(wstr)) ? _K7_BYTE(_K7_WCHAR(wstr, i)) : '\0')

#define _K7_BLOCK16(wstr, off) \
	_K7_STEP(wstr, (off) + 0), _K7_STEP(wstr, (off) + 1), \
	_K7_STEP(wstr, (off) + 2), _K7_STEP(wstr, (off) + 3), \
	_K7_STEP(wstr, (off) + 4), _K7_STEP(wstr, (off) + 5), \
	_K7_STEP(wstr, (off) + 6), _K7_STEP(wstr, (off) + 7), \
	_K7_STEP(wstr, (off) + 8), _K7_STEP(wstr, (off) + 9), \
	_K7_STEP(wstr, (off) + 10), _K7_STEP(wstr, (off) + 11), \
	_K7_STEP(wstr, (off) + 12), _K7_STEP(wstr, (off) + 13), \
	_K7_STEP(wstr, (off) + 14), _K7_STEP(wstr, (off) + 15)

#define _K7_BLOCK128(wstr, off) \
	_K7_BLOCK16(wstr, (off) + 0), _K7_BLOCK16(wstr, (off) + 16), \
	_K7_BLOCK16(wstr, (off) + 32), _K7_BLOCK16(wstr, (off) + 48), \
	_K7_BLOCK16(wstr, (off) + 64), _K7_BLOCK16(wstr, (off) + 80), \
	_K7_BLOCK16(wstr, (off) + 96), _K7_BLOCK16(wstr, (off) + 112)

#define _K7_BLOCK1K(wstr, off) \
	_K7_BLOCK128(wstr, (off) + 0), _K7_BLOCK128(wstr, (off) + 128), \
	_K7_BLOCK128(wstr, (off) + 256), _K7_BLOCK128(wstr, (off) + 384), \
	_K7_BLOCK128(wstr, (off) + 512), _K7_BLOCK128(wstr, (off) + 640), \
	_K7_BLOCK128(wstr, (off) + 768), _K7_BLOCK128(wstr, (off) + 896)

#define _K7_BLOCK4K(wstr, off) \
	_K7_BLOCK1K(wstr, (off) + 0), _K7_BLOCK1K(wstr, (off) + 1024), \
	_K7_BLOCK1K(wstr, (off) + 2048), _K7_BLOCK1K(wstr, (off) + 3072)

#if KOI7_MAX_CHARS <= 128
#define _K7_BLOCK(wstr) _K7_BLOCK128(wstr, 0)
#elif KOI7_MAX_CHARS <= 1024
#define _K7_BLOCK(wstr) _K7_BLOCK1K(wstr, 0)
#elif KOI7_MAX_CHARS <= 4096
#define _K7_BLOCK(wstr) _K7_BLOCK4K(wstr, 0)
#else
#error "KOI7_MAX_CHARS must be 4096 or less"
#endif

#define TO_KOI7(wstr) \
	(_K7_ASSERT_LEN(wstr), \
	 ({ \
		_Pragma("GCC diagnostic push") \
		_Pragma("GCC diagnostic ignored \"-Woverride-init\"") \
		static const char _koi7[] = { _K7_BLOCK(wstr), [_K7_LEN(wstr) * 2] = '\0' }; \
		_Pragma("GCC diagnostic pop") \
		_koi7; \
	 }))

#endif  // KOI7_H
