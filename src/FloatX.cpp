/*
Copyright (C) 2017 - 2017 Evan Teran
                          evan.teran@gmail.com

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "FloatX.h"
#include "Util.h"

#include <iomanip>
#include <sstream>

#ifdef HAVE_DOUBLE_CONVERSION
#include <double-conversion/double-conversion.h>
#endif

template<typename T>
struct SpecialValues;

template<>
struct SpecialValues<double>
{
	static constexpr std::array<std::uint8_t,8> positiveInf {{0,0,0,0,0,0,0xf0,0x7f}};
	static constexpr std::array<std::uint8_t,8> negativeInf {{0,0,0,0,0,0,0xf0,0xff}};
	static constexpr std::array<std::uint8_t,8> positiveSNaN{{0,0,0,0,0,0,0xfc,0x7f}};
	static constexpr std::array<std::uint8_t,8> negativeSNaN{{0,0,0,0,0,0,0xfc,0xff}};
	static constexpr std::array<std::uint8_t,8> positiveQNaN{{0,0,0,0,0,0,0xf8,0x7f}};
	static constexpr std::array<std::uint8_t,8> negativeQNaN{{0,0,0,0,0,0,0xf8,0xff}};
};

template<>
struct SpecialValues<float>
{
	static constexpr std::array<std::uint8_t,4> positiveInf {{0,0,0x80,0x7f}};
	static constexpr std::array<std::uint8_t,4> negativeInf {{0,0,0x80,0xff}};
	static constexpr std::array<std::uint8_t,4> positiveSNaN{{0,0,0xe0,0x7f}};
	static constexpr std::array<std::uint8_t,4> negativeSNaN{{0,0,0xe0,0xff}};
	static constexpr std::array<std::uint8_t,4> positiveQNaN{{0,0,0xc0,0x7f}};
	static constexpr std::array<std::uint8_t,4> negativeQNaN{{0,0,0xc0,0xff}};
};


#ifndef _MSC_VER
#if defined(EDB_X86) || defined(EDB_X86_64)
template<>
struct SpecialValues<long double>
{
	static_assert(std::numeric_limits<long double>::digits==64 &&
				  std::numeric_limits<long double>::max_exponent==16384,
				  "Expected to have x87 80-bit long double");

	static constexpr std::array<std::uint8_t,10> positiveInf {{0,0,0,0,0,0,0,0x80,0xff,0x7f}};
	static constexpr std::array<std::uint8_t,10> negativeInf {{0,0,0,0,0,0,0,0x80,0xff,0xff}};
	static constexpr std::array<std::uint8_t,10> positiveSNaN{{0,0,0,0,0,0,0,0x90,0xff,0x7f}};
	static constexpr std::array<std::uint8_t,10> negativeSNaN{{0,0,0,0,0,0,0,0x90,0xff,0xff}};
	static constexpr std::array<std::uint8_t,10> positiveQNaN{{0,0,0,0,0,0,0,0xc0,0xff,0x7f}};
	static constexpr std::array<std::uint8_t,10> negativeQNaN{{0,0,0,0,0,0,0,0xc0,0xff,0xff}};
};
#endif
#endif

constexpr std::array<std::uint8_t,4> SpecialValues<float>::positiveInf;
constexpr std::array<std::uint8_t,4> SpecialValues<float>::negativeInf;
constexpr std::array<std::uint8_t,4> SpecialValues<float>::positiveSNaN;
constexpr std::array<std::uint8_t,4> SpecialValues<float>::negativeSNaN;
constexpr std::array<std::uint8_t,4> SpecialValues<float>::positiveQNaN;
constexpr std::array<std::uint8_t,4> SpecialValues<float>::negativeQNaN;

constexpr std::array<std::uint8_t,8> SpecialValues<double>::positiveInf;
constexpr std::array<std::uint8_t,8> SpecialValues<double>::negativeInf;
constexpr std::array<std::uint8_t,8> SpecialValues<double>::positiveSNaN;
constexpr std::array<std::uint8_t,8> SpecialValues<double>::negativeSNaN;
constexpr std::array<std::uint8_t,8> SpecialValues<double>::positiveQNaN;
constexpr std::array<std::uint8_t,8> SpecialValues<double>::negativeQNaN;

#ifndef _MSC_VER
#if defined(EDB_X86) || defined(EDB_X86_64)
constexpr std::array<std::uint8_t,10> SpecialValues<long double>::positiveInf;
constexpr std::array<std::uint8_t,10> SpecialValues<long double>::negativeInf;
constexpr std::array<std::uint8_t,10> SpecialValues<long double>::positiveSNaN;
constexpr std::array<std::uint8_t,10> SpecialValues<long double>::negativeSNaN;
constexpr std::array<std::uint8_t,10> SpecialValues<long double>::positiveQNaN;
constexpr std::array<std::uint8_t,10> SpecialValues<long double>::negativeQNaN;
#endif
#endif

template<typename Float>
Float readFloat(const QString& strInput,bool& ok)
{
	ok=false;
	const QString str(strInput.toLower().trimmed());
	if(const auto value=util::fullStringToFloat<Float>(str.toStdString()))
	{
		ok=true;
		return *value;
	}

	// OK, so either it is invalid/unfinished, or it's some special value
	// We still do want the user to be able to enter common special values
	Float value;
	if(str=="+snan"||str=="snan")
		std::memcpy(&value,&SpecialValues<Float>::positiveSNaN,sizeof value);
	else if(str=="-snan")
		std::memcpy(&value,&SpecialValues<Float>::negativeSNaN,sizeof value);
	else if(str=="+qnan"||str=="qnan"||str=="nan")
		std::memcpy(&value,&SpecialValues<Float>::positiveQNaN,sizeof value);
	else if(str=="-qnan")
		std::memcpy(&value,&SpecialValues<Float>::negativeQNaN,sizeof value);
	else if(str=="+inf"||str=="inf")
		std::memcpy(&value,&SpecialValues<Float>::positiveInf,sizeof value);
	else if(str=="-inf")
		std::memcpy(&value,&SpecialValues<Float>::negativeInf,sizeof value);
	else return 0;

	ok=true;
	return value;
}

template float readFloat<float>(const QString& strInput,bool& ok);
template double readFloat<double>(const QString& strInput,bool& ok);


#ifndef _MSC_VER
#if defined(EDB_X86) || defined(EDB_X86_64)
template long double readFloat<long double>(const QString& strInput,bool& ok);
#endif
#endif

namespace detail
{
template<unsigned mantissaLength,typename FloatHolder>
FloatValueClass ieeeClassify(FloatHolder value)
{
	static constexpr auto expLength=8*sizeof value-mantissaLength-1;
	static constexpr auto expMax=(1u<<expLength)-1;
	static constexpr auto QNaN_mask=1ull<<(mantissaLength-1);
	const auto mantissa=value&((1ull<<mantissaLength)-1);
	const auto exponent=(value>>mantissaLength)&expMax;
	if(exponent==expMax)
	{
		if(mantissa==0u)
			return FloatValueClass::Infinity; // |S|11..11|00..00|
		else if(mantissa & QNaN_mask)
			return FloatValueClass::QNaN; // |S|11..11|1XX..XX|
		else
			return FloatValueClass::SNaN; // |S|11..11|0XX..XX|
	}
	else if(exponent==0u)
	{
		if(mantissa==0u)
			return FloatValueClass::Zero; // |S|00..00|00..00|
		else
			return FloatValueClass::Denormal; // |S|00..00|XX..XX|
	}
	else return FloatValueClass::Normal;
}
}

FloatValueClass floatType(edb::value32 value)
{
	return detail::ieeeClassify<23>(value);
}

FloatValueClass floatType(edb::value64 value)
{
	return detail::ieeeClassify<52>(value);
}

FloatValueClass floatType(edb::value80 value)
{
	static constexpr auto mantissaLength=64;
	static constexpr auto expLength=8*sizeof value-mantissaLength-1;
	static constexpr auto integerBitOnly=1ull<<(mantissaLength-1);
	static constexpr auto QNaN_mask=3ull<<(mantissaLength-2);
	static constexpr auto expMax=(1u<<expLength)-1;

	const auto exponent=value.exponent();
	const auto mantissa=value.mantissa();
	const bool integerBitSet=mantissa & integerBitOnly;

	// This is almost as ieeeClassify, but also takes integer bit (not present in
	// IEEE754 format) into account to detect unsupported values
	if(exponent==expMax)
	{
		if(mantissa==integerBitOnly)
			return FloatValueClass::Infinity; // |S|11..11|1.000..0|
		else if((mantissa & QNaN_mask) == QNaN_mask)
			return FloatValueClass::QNaN;                 // |S|11..11|1.1XX..X|
		else if((mantissa & QNaN_mask) == integerBitOnly)
			return FloatValueClass::SNaN;                 // |S|11..11|1.0XX..X|
		else
			return FloatValueClass::Unsupported; // all exp bits set, but integer bit reset - pseudo-NaN/Inf
	}
	else if(exponent==0u)
	{
		if(mantissa==0u)
			return FloatValueClass::Zero; // |S|00..00|00..00|
		else
		{
			if(!integerBitSet)
				return FloatValueClass::Denormal;     // |S|00..00|0.XXXX..X|
			else
				return FloatValueClass::PseudoDenormal;// |S|00..00|1.XXXX..X|
		}
	}
	else
	{
		if(integerBitSet)
			return FloatValueClass::Normal;
		else
			return FloatValueClass::Unsupported; // integer bit reset but exp is as if normal - unnormal
	}

}

template<typename Float>
QValidator::State FloatXValidator<Float>::validate(QString& input, int&) const
{
	if(input.isEmpty()) return QValidator::Intermediate;

	// The input may be in hex format. std::istream doesn't support extraction
	// of hexfloat, but std::strto[f,d,ld] do. (see wg21.link/lwg2381)
	if(const auto v=util::fullStringToFloat<Float>(input.toStdString()))
		return QValidator::Acceptable;

	// OK, so we failed to read it or it is unfinished. Let's check whether it's intermediate or invalid.
	QRegExp basicFormat("[+-]?[0-9]*\\.?[0-9]*(e([+-]?[0-9]*)?)?");
	QRegExp specialFormat("[+-]?[sq]?nan|[+-]?inf",Qt::CaseInsensitive);
	QRegExp hexfloatFormat("[+-]?0x[0-9a-f]*\\.?[0-9a-f]*(p([+-]?[0-9]*)?)?",Qt::CaseInsensitive);
	QRegExp specialFormatUnfinished("[+-]?[sq]?(n(an?)?)?|[+-]?(i(nf?)?)?",Qt::CaseInsensitive);

	if(hexfloatFormat.exactMatch(input))
		return QValidator::Intermediate;
	if(basicFormat.exactMatch(input))
		return QValidator::Intermediate;
	if(specialFormat.exactMatch(input))
		return QValidator::Acceptable;
	if(specialFormatUnfinished.exactMatch(input))
		return QValidator::Intermediate;

	// All possible options are exhausted, so consider the input invalid
	return QValidator::Invalid;
}

template QValidator::State FloatXValidator<float>::validate(QString& input, int&) const;
template QValidator::State FloatXValidator<double>::validate(QString& input, int&) const;
template QValidator::State FloatXValidator<long double>::validate(QString& input, int&) const;

float toFloatValue(edb::value32 value)
{
	float result;
	std::memcpy(&result,&value,sizeof result);
	return result;
}

double toFloatValue(edb::value64 value)
{
	double result;
	std::memcpy(&result,&value,sizeof result);
	return result;
}

long double toFloatValue(edb::value80 value)
{
	return value.toFloatValue();
}

template<typename Float>
QString formatFloat(Float value)
{
	const auto type=floatType(value);
	QString specialStr="???? ";
	switch(type)
	{
	case FloatValueClass::Zero:
		return value.negative()?"-0.0":"0.0";
	case FloatValueClass::PseudoDenormal:
		{
			assert(sizeof value==10);

			// keeps compiler happy?
			if(sizeof(value) == 10) {
				// Convert to supported value as the CPU would. Otherwise glibc takes it wrong.
				const uint16_t exponent=value.negative()?0x8001:0x0001;
				std::memcpy(reinterpret_cast<char*>(&value)+8,&exponent,sizeof exponent);
			}
		}
		// fall through
	case FloatValueClass::Normal:
	case FloatValueClass::Denormal:
		{
#ifdef HAVE_DOUBLE_CONVERSION
			constexpr bool isDouble=std::is_same<Float, edb::value64>::value;
			constexpr bool isFloat=std::is_same<Float, edb::value32>::value;
			if(isDouble || isFloat)
			{
				using namespace double_conversion;
				char buffer[64];
				DoubleToStringConverter conv(DoubleToStringConverter::EMIT_POSITIVE_EXPONENT_SIGN, "inf", "nan", 'e', -4, isDouble ? 15 : 6, 0,0);
				StringBuilder builder(buffer, sizeof buffer);
				bool result=false;
				if(isDouble)
				{
					double d;
					std::memcpy(&d, &value, sizeof d);
					result=conv.ToShortest(d, &builder);
				}
				else // isFloat
				{
					float f;
					std::memcpy(&f, &value, sizeof f);
					result=conv.ToShortestSingle(f, &builder);
				}
				if(result && builder.Finalize())
				{
					const QString result=buffer;
					if(result.size()==1 && result[0].isDigit())
						return result+".0"; // avoid printing small whole numbers as integers
					return result;
				}
			}
#endif
			std::ostringstream ss;
			ss << std::setprecision(std::numeric_limits<decltype(toFloatValue(value))>::max_digits10) << toFloatValue(value);
			const auto result=QString::fromStdString(ss.str());
			if(result.size()==1 && result[0].isDigit())
				return result+".0"; // avoid printing small whole numbers as integers
			return result;
		}
	case FloatValueClass::Infinity:
		return QString(value.negative()?"-":"+")+"INF";
	case FloatValueClass::QNaN:
		specialStr=QString(value.negative()?"-":"+")+"QNAN ";
		break;
	case FloatValueClass::SNaN:
		specialStr=QString(value.negative()?"-":"+")+"SNAN ";
		break;
	case FloatValueClass::Unsupported:
		specialStr=QString(value.negative()?"-":"+")+"BAD ";
		break;
	}
	// If we are here, then the value is special
	auto hexStr=value.toHexString();
	const QChar groupSeparator(' ');
	if(hexStr.size()>8)
		hexStr.insert(hexStr.size()-8,groupSeparator);
	if(hexStr.size()>16+1) // +1 to take into account already inserted first separator
		hexStr.insert(hexStr.size()-16-1,groupSeparator);
	return specialStr+hexStr;
}
template QString formatFloat<edb::value32>(edb::value32);
template QString formatFloat<edb::value64>(edb::value64);
template QString formatFloat<edb::value80>(edb::value80);
