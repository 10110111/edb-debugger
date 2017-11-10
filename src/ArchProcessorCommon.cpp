#include "ArchProcessorCommon.h"

//------------------------------------------------------------------------------
// Name: format_pointer
// Desc:
//------------------------------------------------------------------------------
QString format_pointer(int pointer_level, edb::reg_t arg, QChar type) {

	Q_UNUSED(type);
	Q_UNUSED(pointer_level);

	if(arg == 0) {
		return "NULL";
	} else {
		return edb::v1::format_pointer(arg);
	}
}

//------------------------------------------------------------------------------
// Name: format_integer
// Desc:
//------------------------------------------------------------------------------
QString format_integer(int pointer_level, edb::reg_t arg, QChar type) {
	if(pointer_level > 0) {
		return format_pointer(pointer_level, arg, type);
	}

	switch(type.toLatin1()) {
	case 'w': return "0x"+QString::number(static_cast<wchar_t>(arg), 16);
	case 'b': return arg ? "true" : "false";
	case 'c':
		if(arg < 0x80u && (std::isprint(arg) || std::isspace(arg))) {
			return QString("'%1'").arg(static_cast<char>(arg));
		} else {
			return QString("'\\x%1'").arg(static_cast<quint16>(arg),2,16);
		}
	case 'a': // signed char; since we're formatting as hex, we want to avoid sign
			  // extension done inside QString::number (happening due to the cast to
			  // qlonglong inside QString::setNum, which used in QString::number).
			  // Similarly for other shorter-than-long-long signed types.
	case 'h': return "0x"+QString::number(static_cast<unsigned char>(arg), 16);
	case 's':
	case 't': return "0x"+QString::number(static_cast<unsigned short>(arg), 16);
	case 'i':
	case 'j': return "0x"+QString::number(debuggeeIs32Bit() ? ILP32::toUInt(arg) : LP64::toUInt(arg), 16);
	case 'l':
	case 'm': return "0x"+QString::number(debuggeeIs32Bit() ? ILP32::toULong(arg) : LP64::toULong(arg), 16);
	case 'x': return "0x"+QString::number(static_cast<long long>(arg), 16);
	case 'y': return "0x"+QString::number(static_cast<long unsigned long>(arg), 16);
	case 'n':
	case 'o':
	default:
		return format_pointer(pointer_level, arg, type);
	}
}

//------------------------------------------------------------------------------
// Name: format_integer
// Desc:
//------------------------------------------------------------------------------
QString format_char(int pointer_level, edb::address_t arg, QChar type) {

	if(IProcess *process = edb::v1::debugger_core->process()) {
		if(pointer_level == 1) {
			if(arg == 0) {
				return "NULL";
			} else {
				QString string_param;
				int string_length;

				if(edb::v1::get_ascii_string_at_address(arg, string_param, edb::v1::config().min_string_length, 256, string_length)) {
					return QString("<%1> \"%2\"").arg(edb::v1::format_pointer(arg), string_param);
				} else {
					char character;
					process->read_bytes(arg, &character, sizeof(character));
					if(character == '\0') {
						return QString("<%1> \"\"").arg(edb::v1::format_pointer(arg));
					} else {
						return QString("<%1>").arg(edb::v1::format_pointer(arg));
					}
				}
			}
		} else {
			return format_integer(pointer_level, arg, type);
		}
	}

	return "?";
}

//------------------------------------------------------------------------------
// Name: format_argument
// Desc:
//------------------------------------------------------------------------------
QString format_argument(const QString &type, const Register& arg) {

	if(!arg) return QObject::tr("(failed to get value)");
	int pointer_level = 0;
	for(QChar ch: type) {

		if(ch == 'P') {
			++pointer_level;
		} else if(ch == 'r' || ch == 'V' || ch == 'K') {
			// skip things like const, volatile, restrict, they don't effect
			// display for us
			continue;
		} else {
			switch(ch.toLatin1()) {
			case 'v': return format_pointer(pointer_level, arg.valueAsAddress(), ch);
			case 'w': return format_integer(pointer_level, arg.valueAsInteger(), ch);
			case 'b': return format_integer(pointer_level, arg.valueAsSignedInteger(), ch);
			case 'c': return format_char(pointer_level, arg.valueAsAddress(), ch);
			case 'a': return format_integer(pointer_level, arg.valueAsSignedInteger(), ch);
			case 'h': return format_integer(pointer_level, arg.valueAsInteger(), ch);
			case 's': return format_integer(pointer_level, arg.valueAsSignedInteger(), ch);
			case 't': return format_integer(pointer_level, arg.valueAsInteger(), ch);
			case 'i': return format_integer(pointer_level, arg.valueAsSignedInteger(), ch);
			case 'j': return format_integer(pointer_level, arg.valueAsInteger(), ch);
			case 'l': return format_integer(pointer_level, arg.valueAsSignedInteger(), ch);
			case 'm': return format_integer(pointer_level, arg.valueAsInteger(), ch);
			case 'x': return format_integer(pointer_level, arg.valueAsSignedInteger(), ch);
			case 'y': return format_integer(pointer_level, arg.valueAsInteger(), ch);
			case 'n': return format_integer(pointer_level, arg.valueAsSignedInteger(), ch);
			case 'o': return format_integer(pointer_level, arg.valueAsSignedInteger(), ch);
			case 'f':
			case 'd':
			case 'e':
			case 'g':
			case 'z':
			default:
				break;
			}
		}
	}

	return format_pointer(pointer_level, arg.valueAsAddress(), 'x');
}

