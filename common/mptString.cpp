/*
 * mptString.cpp
 * -------------
 * Purpose: A wrapper around std::string implemeting the CString.
 * Notes  : Should be removed somewhen in the future when all uses of CString have been converted to std::string.
 * Authors: OpenMPT Devs
 * The OpenMPT source code is released under the BSD license. Read LICENSE for more details.
 */

#include "stdafx.h"
#include "mptString.h"

#include <stdexcept>
#include <vector>
#include <cstdarg>

#if !defined(WIN32)
#include <iconv.h>
#endif // !WIN32


namespace mpt { namespace String {


std::string Format(const char *format, ...)
{
	#if MPT_COMPILER_MSVC
		va_list argList;
		va_start(argList, format);

		// Count the needed array size.
		const size_t nCount = _vscprintf(format, argList); // null character not included.
		std::vector<char> buf(nCount + 1); // + 1 is for null terminator.
		vsprintf_s(&(buf[0]), buf.size(), format, argList);

		va_end(argList);
		return &(buf[0]);
	#else
		va_list argList;
		va_start(argList, format);
		int size = vsnprintf(NULL, 0, format, argList); // get required size, requires c99 compliant vsnprintf which msvc does not have
		va_end(argList);
		std::vector<char> temp(size + 1);
		va_start(argList, format);
		vsnprintf(&(temp[0]), size + 1, format, argList);
		va_end(argList);
		return &(temp[0]);
	#endif
}


#if defined(WIN32)
static UINT CharsetToCodepage(Charset charset)
{
	switch(charset)
	{
		case CharsetLocale:      return CP_ACP;  break;
		case CharsetUTF8:        return CP_UTF8; break;
		case CharsetUS_ASCII:    return 20127;   break;
		case CharsetISO8859_1:   return 28591;   break;
		case CharsetISO8859_15:  return 28605;   break;
		case CharsetCP437:       return 437;     break;
		case CharsetWindows1252: return 1252;    break;
	}
	return 0;
}
#else // !WIN32
static const char * CharsetToString(Charset charset)
{
	switch(charset)
	{
		case CharsetLocale:      return "";            break; // "char" breaks with glibc when no locale is set
		case CharsetUTF8:        return "UTF-8";       break;
		case CharsetUS_ASCII:    return "ASCII";       break;
		case CharsetISO8859_1:   return "ISO-8859-1";  break;
		case CharsetISO8859_15:  return "ISO-8859-15"; break;
		case CharsetCP437:       return "CP437";       break;
		case CharsetWindows1252: return "CP1252";      break;
	}
	return 0;
}
static const char * CharsetToStringTranslit(Charset charset)
{
	switch(charset)
	{
		case CharsetLocale:      return "//TRANSLIT";            break; // "char" breaks with glibc when no locale is set
		case CharsetUTF8:        return "UTF-8//TRANSLIT";       break;
		case CharsetUS_ASCII:    return "ASCII//TRANSLIT";       break;
		case CharsetISO8859_1:   return "ISO-8859-1//TRANSLIT";  break;
		case CharsetISO8859_15:  return "ISO-8859-15//TRANSLIT"; break;
		case CharsetCP437:       return "CP437//TRANSLIT";       break;
		case CharsetWindows1252: return "CP1252//TRANSLIT";      break;
	}
	return 0;
}
#endif // WIN32


std::string Encode(const std::wstring &src, Charset charset)
{
	#if defined(WIN32)
		const UINT codepage = CharsetToCodepage(charset);
		int required_size = WideCharToMultiByte(codepage, 0, src.c_str(), -1, nullptr, 0, nullptr, nullptr);
		if(required_size <= 0)
		{
			return std::string();
		}
		std::vector<CHAR> encoded_string(required_size);
		WideCharToMultiByte(codepage, 0, src.c_str(), -1, &encoded_string[0], required_size, nullptr, nullptr);
		return &encoded_string[0];
	#else // !WIN32
		iconv_t conv = iconv_t();
		conv = iconv_open(CharsetToStringTranslit(charset), "wchar_t");
		if(!conv)
		{
			conv = iconv_open(CharsetToString(charset), "wchar_t");
			if(!conv)
			{
				throw std::runtime_error("iconv conversion not working");
			}
		}
		std::vector<wchar_t> wide_string(src.c_str(), src.c_str() + src.length() + 1);
		std::vector<char> encoded_string(wide_string.size() * 8); // large enough
		char * inbuf = (char*)&wide_string[0];
		size_t inbytesleft = wide_string.size() * sizeof(wchar_t);
		char * outbuf = &encoded_string[0];
		size_t outbytesleft = encoded_string.size();
		while(iconv(conv, &inbuf, &inbytesleft, &outbuf, &outbytesleft) == (size_t)-1)
		{
			if(errno == EILSEQ || errno == EILSEQ)
			{
				inbuf += sizeof(wchar_t);
				inbytesleft -= sizeof(wchar_t);
				outbuf[0] = '?';
				outbuf++;
				outbytesleft--;
				iconv(conv, NULL, NULL, NULL, NULL); // reset state
			} else
			{
				iconv_close(conv);
				conv = iconv_t();
				return std::string();
			}
		}
		iconv_close(conv);
		conv = iconv_t();
		return &encoded_string[0];
	#endif // WIN32
}


std::wstring Decode(const std::string &src, Charset charset)
{
	#if defined(WIN32)
		const UINT codepage = CharsetToCodepage(charset);
		int required_size = MultiByteToWideChar(codepage, 0, src.c_str(), -1, nullptr, 0);
		if(required_size <= 0)
		{
			return std::wstring();
		}
		std::vector<WCHAR> decoded_string(required_size);
		MultiByteToWideChar(codepage, 0, src.c_str(), -1, &decoded_string[0], required_size);
		return &decoded_string[0];
	#else // !WIN32
		iconv_t conv = iconv_t();
		conv = iconv_open("wchar_t", CharsetToString(charset));
		if(!conv)
		{
			throw std::runtime_error("iconv conversion not working");
		}
		std::vector<char> encoded_string(src.c_str(), src.c_str() + src.length() + 1);
		std::vector<wchar_t> wide_string(encoded_string.size() * 8); // large enough
		char * inbuf = &encoded_string[0];
		size_t inbytesleft = encoded_string.size();
		char * outbuf = (char*)&wide_string[0];
		size_t outbytesleft = wide_string.size() * sizeof(wchar_t);
		while(iconv(conv, &inbuf, &inbytesleft, &outbuf, &outbytesleft) == (size_t)-1)
		{
			if(errno == EILSEQ || errno == EILSEQ)
			{
				inbuf++;
				inbytesleft--;
				for(std::size_t i = 0; i < sizeof(wchar_t); ++i)
				{
					outbuf[i] = 0;
				}
				#ifdef MPT_PLATFORM_BIG_ENDIAN
					outbuf[sizeof(wchar_t)-1 - 1] = 0xff; outbuf[sizeof(wchar_t)-1 - 0] = 0xfd;
				#else
					outbuf[1] = 0xff; outbuf[0] = 0xfd;
				#endif
				outbuf += sizeof(wchar_t);
				outbytesleft -= sizeof(wchar_t);
				iconv(conv, NULL, NULL, NULL, NULL); // reset state
			} else
			{
				iconv_close(conv);
				conv = iconv_t();
				return std::wstring();
			}
		}
		iconv_close(conv);
		conv = iconv_t();
		return &wide_string[0];
	#endif // WIN32
}


std::string Convert(const std::string &src, Charset from, Charset to)
{
	#if defined(WIN32)
		return Encode(Decode(src, from), to);
	#else // !WIN32
		iconv_t conv = iconv_t();
		conv = iconv_open(CharsetToStringTranslit(to), CharsetToString(from));
		if(!conv)
		{
			conv = iconv_open(CharsetToString(to), CharsetToString(from));
			if(!conv)
			{
				throw std::runtime_error("iconv conversion not working");
			}
		}
		std::vector<char> src_string(src.c_str(), src.c_str() + src.length() + 1);
		std::vector<char> dst_string(src_string.size() * 8); // large enough
		char * inbuf = &src_string[0];
		size_t inbytesleft = src_string.size();
		char * outbuf = &dst_string[0];
		size_t outbytesleft = dst_string.size();
		while(iconv(conv, &inbuf, &inbytesleft, &outbuf, &outbytesleft) == (size_t)-1)
		{
			if(errno == EILSEQ || errno == EILSEQ)
			{
				inbuf++;
				inbytesleft--;
				outbuf[0] = '?';
				outbuf++;
				outbytesleft--;
				iconv(conv, NULL, NULL, NULL, NULL); // reset state
			} else
			{
				iconv_close(conv);
				conv = iconv_t();
				return std::string();
			}
		}
		iconv_close(conv);
		conv = iconv_t();
		return &dst_string[0];
	#endif // WIN32
}


#if defined(_MFC_VER)

CString ToCString(const std::string &src, Charset charset)
{
	#ifdef UNICODE
		return mpt::String::Decode(src, charset).c_str();
	#else
		return mpt::String::Convert(src, charset, mpt::CharsetLocale).c_str();
	#endif
}

CString ToCString(const std::wstring &src)
{
	#ifdef UNICODE
		return src.c_str();
	#else
		return mpt::String::Encode(src, mpt::CharsetLocale).c_str();
	#endif
}

std::string FromCString(const CString &src, Charset charset)
{
	#ifdef UNICODE
		return mpt::String::Encode(src.GetString(), charset);
	#else
		return mpt::String::Convert(src.GetString(), mpt::CharsetLocale, charset);
	#endif
}

std::wstring FromCString(const CString &src)
{
	#ifdef UNICODE
		return src.GetString();
	#else
		return mpt::String::Decode(src.GetString(), mpt::CharsetLocale);
	#endif
}

#endif


} } // namespace mpt::String

