#ifndef ARCH_PROCESSOR_COMMON_H_20170930
#define ARCH_PROCESSOR_COMMON_H_20170930

#include "Types.h"
#include <QChar>

QString format_pointer(int pointer_level, edb::reg_t arg, QChar type);
QString format_integer(int pointer_level, edb::reg_t arg, QChar type);
QString format_char(int pointer_level, edb::address_t arg, QChar type);
QString format_argument(const QString &type, const Register& arg);

#endif
