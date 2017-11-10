/*
Copyright (C) 2006 - 2015 Evan Teran
                          evan.teran@gmail.com
Copyright (C) 2017 - 2017 Ruslan Kabatsayev
                          b7.10110111@gmail.com

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

#include "ArchProcessor.h"
#include "Configuration.h"
#include "RegisterViewModel.h"
#include "State.h"
#include "Util.h"
#include "edb.h"
#include "IDebugger.h"
#include "IProcess.h"

#include <cstdint>

#ifdef Q_OS_LINUX
#include <asm/unistd.h>
#include "errno-names-linux.h"
#endif


#include <QDomDocument>
#include <QXmlQuery>
#include <QFile>
#include <QMenu>

using std::uint32_t;

namespace {
static constexpr size_t GPR_COUNT=16;

template<typename T>
QString syscallErrName(T err) {
#ifdef Q_OS_LINUX
	std::size_t index=-err;
	if(index>=sizeof errnoNames/sizeof*errnoNames) return "";
	if(errnoNames[index]) return errnoNames[index];
    return "";
#else
	return "";
#endif
}

}

int capstoneRegToGPRIndex(int capstoneReg, bool& ok) {

	ok=false;
	int regIndex=-1;
	// NOTE: capstone registers are stupidly not in continuous order
	switch(capstoneReg)
	{
	case ARM_REG_R0:  regIndex=0; break;
	case ARM_REG_R1:  regIndex=1; break;
	case ARM_REG_R2:  regIndex=2; break;
	case ARM_REG_R3:  regIndex=3; break;
	case ARM_REG_R4:  regIndex=4; break;
	case ARM_REG_R5:  regIndex=5; break;
	case ARM_REG_R6:  regIndex=6; break;
	case ARM_REG_R7:  regIndex=7; break;
	case ARM_REG_R8:  regIndex=8; break;
	case ARM_REG_R9:  regIndex=9; break;
	case ARM_REG_R10: regIndex=10; break;
	case ARM_REG_R11: regIndex=11; break;
	case ARM_REG_R12: regIndex=12; break;
	case ARM_REG_R13: regIndex=13; break;
	case ARM_REG_R14: regIndex=14; break;
	case ARM_REG_R15: regIndex=15; break;
	default: return -1;
	}
	ok=true;
	return regIndex;
}

Result<edb::address_t> getOperandValueGPR(const edb::Instruction &insn, const edb::Operand &operand, const State &state) {

	using ResultT=Result<edb::address_t>;
	bool ok;
	const auto regIndex=capstoneRegToGPRIndex(operand->reg, ok);
	if(!ok) return ResultT(QObject::tr("bad operand register for instruction %1: %2.").arg(insn.mnemonic().c_str()).arg(operand->reg), 0);
	const auto reg=state.gp_register(regIndex);
	if(!reg)
		return ResultT(QObject::tr("failed to get register r%1.").arg(regIndex), 0);
	auto value=reg.valueAsAddress();
	return ResultT(value);
}

Result<edb::address_t> adjustR15Value(const edb::Instruction &insn, const int regIndex, edb::address_t value) {

	using ResultT=Result<edb::address_t>;
	if(regIndex==15)
	{
		// Even if current state's PC weren't on this instruction, the instruction still refers to
		// self, so use `insn` instead of `state` to get the value.
		const auto cpuMode=edb::v1::debugger_core->cpu_mode();
		switch(cpuMode)
		{
		case IDebugger::CPUMode::ARM32:
			value=insn.rva()+8;
			break;
		case IDebugger::CPUMode::Thumb:
			value=insn.rva()+4;
			break;
		default:
			return ResultT(QObject::tr("calculating effective address in modes other than ARM and Thumb is not supported."), 0);
		}
	}
	return ResultT(value);
}

uint32_t shift(uint32_t x, arm_shifter type, uint32_t shiftAmount, bool carryFlag)
{
    constexpr uint32_t highBit=1u<<31;
	const auto N=shiftAmount;
	switch(type)
	{
	case ARM_SFT_INVALID:
		return x;
	case ARM_SFT_ASR:
	case ARM_SFT_ASR_REG:
		assert(N>=1 && N<=32);
		// NOTE: unlike on x86, shift by 32 bits on ARM is not a NOP: it sets all bits to sign bit
		return N==32 ? -((x&highBit)>>31) : x>>N | ~(((x&highBit)>>N)-1);
	case ARM_SFT_LSL:
	case ARM_SFT_LSL_REG:
		assert(N>=0 && N<=31);
		return x<<N;
	case ARM_SFT_LSR:
	case ARM_SFT_LSR_REG:
		// NOTE: unlike on x86, shift by 32 bits on ARM is not a NOP: it clears the value
		return N==32 ? 0 : x>>N;
	case ARM_SFT_ROR:
	case ARM_SFT_ROR_REG:
	{
		assert(N>=1 && N<=31);
		constexpr unsigned mask=8*sizeof x-1;
		return x>>N | x<<((-N)&mask);
	}
	case ARM_SFT_RRX:
	case ARM_SFT_RRX_REG:
		return uint32_t(carryFlag)<<31 | x>>1;
	}
	assert(!"Must not reach here!");
	return x;
}

// NOTE: this function shouldn't be used for operands other than those used as addresses.
// E.g. for "STM Rn,{regs...}" this function shouldn't try to get the value of any of the {regs...}.
// Also note that undefined instructions like "STM PC, {regs...}" aren't checked here.
Result<edb::address_t> ArchProcessor::get_effective_address(const edb::Instruction &insn, const edb::Operand &operand, const State &state) const {

	using ResultT=Result<edb::address_t>;
	if(!operand || !insn) return ResultT(QObject::tr("operand is invalid"),0);

	const auto op=insn.operation();
	if(is_register(operand))
	{
		bool ok;
		const auto regIndex=capstoneRegToGPRIndex(operand->reg, ok);
		if(!ok) return ResultT(QObject::tr("bad operand register for instruction %1: %2.").arg(insn.mnemonic().c_str()).arg(operand->reg), 0);
		const auto reg=state.gp_register(regIndex);
		if(!reg) return ResultT(QObject::tr("failed to get register r%1.").arg(regIndex), 0);
		auto value=reg.valueAsAddress();
		return adjustR15Value(insn, regIndex, value);
	}
	else if(is_expression(operand))
	{
		bool ok;
		Register baseR, indexR, cpsrR;

		const auto baseIndex = capstoneRegToGPRIndex(operand->mem.base , ok);
		// base must be valid
		if(!ok || !(baseR = state.gp_register(baseIndex)))
			return ResultT(QObject::tr("failed to get register r%1.").arg(baseIndex), 0);

		const auto indexIndex = capstoneRegToGPRIndex(operand->mem.index, ok);
		if(ok) // index register may be irrelevant, only try to get it if its index is valid
		{
			if(!(indexR = state.gp_register(indexIndex)))
				return ResultT(QObject::tr("failed to get register r%1.").arg(indexIndex), 0);
		}

		cpsrR=state.flags_register();
		if(!cpsrR && (operand->shift.type==ARM_SFT_RRX || operand->shift.type==ARM_SFT_RRX_REG))
			return ResultT(QObject::tr("failed to get CPSR."), 0);
		const bool C = cpsrR ? cpsrR.valueAsInteger()&0x20000000 : false;

		edb::address_t addr=baseR.valueAsAddress();
		if(const auto adjustedRes=adjustR15Value(insn, baseIndex, addr))
		{
			addr = adjustedRes.value()+operand->mem.disp;
			if(indexR)
				addr += operand->mem.scale*shift(indexR.valueAsAddress(), operand->shift.type, operand->shift.value, C);
			return ResultT(addr);
		}
		else return adjustedRes;
	}

	return ResultT(QObject::tr("getting effective address for operand %1 of instruction %2 is not implemented").arg(operand.index()+1).arg(insn.mnemonic().c_str()), 0);
}

edb::address_t ArchProcessor::get_effective_address(const edb::Instruction &inst, const edb::Operand &op, const State &state, bool& ok) const {

	ok=false;
	const auto result = get_effective_address(inst, op, state);
	if(!result) return 0;
	return result.value();
}


RegisterViewModel& getModel() {
	return static_cast<RegisterViewModel&>(edb::v1::arch_processor().get_register_view_model());
}

ArchProcessor::ArchProcessor() {
	if(edb::v1::debugger_core) {
		connect(edb::v1::debugger_ui, SIGNAL(attachEvent()), this, SLOT(justAttached()));
	}
}

void analyze_syscall(const edb::Instruction &inst, QStringList &ret) {

	State state;
	edb::v1::debugger_core->get_state(&state);
	if(state.empty())
		return;
	const auto r7Reg=state.gp_register(7);
	if(!r7Reg) return;
	const std::uint32_t r7=r7Reg.valueAsInteger();

	QString syscall_entry;
	QDomDocument syscall_xml;
	QFile file(":/debugger/xml/syscalls.xml");
	if(file.open(QIODevice::ReadOnly | QIODevice::Text)) {

		QXmlQuery query;
		query.setFocus(&file);
		const QString arch = "arm";
		query.setQuery(QString("syscalls[@version='1.0']/linux[@arch='"+arch+"']/syscall[index=%1]").arg(r7));
		if (query.isValid()) {
			query.evaluateTo(&syscall_entry);
		}
		file.close();
	}

	if(!syscall_entry.isEmpty()) {
		syscall_xml.setContent("" + syscall_entry + "");
		QDomElement root = syscall_xml.documentElement();

		QStringList arguments;
		for (QDomElement argument = root.firstChildElement("argument"); !argument.isNull(); argument = argument.nextSiblingElement("argument")) {
			const QString argument_type     = argument.attribute("type");
			const QString argument_register = argument.attribute("register");
			const auto reg=state[argument_register];
			if(reg) {
				arguments << format_argument(argument_type, reg);
				continue;
			}
		}
		ret << ArchProcessor::tr("SYSCALL: %1(%2)").arg(root.attribute("name"), arguments.join(","));
	}

}

QStringList ArchProcessor::update_instruction_info(edb::address_t address) {

	QStringList ret;

	Q_ASSERT(edb::v1::debugger_core);

	if(IProcess *process = edb::v1::debugger_core->process()) {
		quint8 buffer[edb::Instruction::MAX_SIZE];

		if(process->read_bytes(address, buffer, sizeof buffer)==sizeof buffer) {
			const edb::Instruction inst(buffer, buffer + sizeof buffer, address);
			if(inst) {
				if(inst.operation()==ARM_INS_SVC) {
					analyze_syscall(inst, ret);
				}
			}
		}
	}
	return ret;
}

bool ArchProcessor::can_step_over(const edb::Instruction &inst) const {
	return inst && (is_call(inst) || is_interrupt(inst) || !modifies_pc(inst));
}

bool ArchProcessor::is_filling(const edb::Instruction &inst) const {
	Q_UNUSED(inst);
	return false;
}

void ArchProcessor::reset() {
}

void ArchProcessor::about_to_resume() {
	getModel().saveValues();
}

void ArchProcessor::setup_register_view() {

	if(edb::v1::debugger_core) {

		update_register_view(QString(), State());
	}
}

QString pcComment(Register const& reg, QString const& default_region_name) {
	const auto symname=edb::v1::find_function_symbol(reg.valueAsAddress(), default_region_name);
	return symname.isEmpty() ? symname : '<'+symname+'>';
}

QString gprComment(Register const& reg) {

	QString regString;
	int stringLength;
	QString comment;
	if(edb::v1::get_ascii_string_at_address(reg.valueAsAddress(), regString, edb::v1::config().min_string_length, 256, stringLength))
		comment=QString("ASCII \"%1\"").arg(regString);
	else if(edb::v1::get_utf16_string_at_address(reg.valueAsAddress(), regString, edb::v1::config().min_string_length, 256, stringLength))
		comment=QString("UTF16 \"%1\"").arg(regString);
	return comment;
}

void updateGPRs(RegisterViewModel& model, State const& state, QString const& default_region_name) {
	for(std::size_t i=0;i<GPR_COUNT;++i) {
		const auto reg=state.gp_register(i);
		Q_ASSERT(!!reg); Q_ASSERT(reg.bitSize()==32);
		QString comment;
		if(i==0) {
			const auto origR0Reg=state["orig_r0"];
			const auto origR0=origR0Reg.valueAsSignedInteger();
			if(origR0Reg && origR0!=-1) {

				comment="orig: "+edb::value32(origR0).toHexString();
				const auto errName=syscallErrName(reg.value<edb::value32>());
				if(!errName.isEmpty())
					comment="-"+errName+"; "+comment;
			}
		}
		if(comment.isEmpty()) {

			if(i!=15)
				comment=gprComment(reg);
			else
				comment=pcComment(reg,default_region_name);
		}
		model.updateGPR(i,reg.value<edb::value32>(),comment);
	}
}

bool is_jcc_taken(const edb::reg_t cpsr, edb::Instruction::ConditionCode cond) {
	const bool N = (cpsr & 0x80000000) != 0;
	const bool Z = (cpsr & 0x40000000) != 0;
	const bool C = (cpsr & 0x20000000) != 0;
	const bool V = (cpsr & 0x10000000) != 0;

	bool taken = false;
	switch(cond & 0xe) {
	case 0x0:
		taken = Z;
		break;
	case 0x2:
		taken = C;
		break;
	case 0x4:
		taken = N;
		break;
	case 0x6:
		taken = V;
		break;
	case 0x8:
		taken = C && !Z;
		break;
	case 0xa:
		taken = N == V;
		break;
	case 0xc:
		taken = !Z && (N==V);
		break;
	case 0xe:
		taken = true;
		break;
	}

	if(cond & 1)
		taken = !taken;

	return taken;
}

static const QLatin1String jumpConditionMnemonics[] = {
	QLatin1String("EQ"),  QLatin1String("NE"),
	QLatin1String("HS"),  QLatin1String("LO"),
	QLatin1String("MI"),  QLatin1String("PL"),
	QLatin1String("VS"),  QLatin1String("VC"),
	QLatin1String("HI"),  QLatin1String("LS"),
	QLatin1String("GE"),  QLatin1String("LT"),
	QLatin1String("GT"),  QLatin1String("LE"),
	QLatin1String("AL"),  QLatin1String("??"),
};

QString cpsrComment(edb::reg_t flags) {
	QString comment="(";
	for(int cond=0;cond<0x10-2;++cond) // we're not interested in AL or UNDEFINED conditions
		if(is_jcc_taken(flags,static_cast<edb::Instruction::ConditionCode>(cond)))
			comment+=jumpConditionMnemonics[cond]+',';
	comment[comment.size()-1]=')';
	return comment;
}

void updateCPSR(RegisterViewModel& model, State const& state) {
	const auto flags=state.flags_register();
	Q_ASSERT(!!flags);
	const auto comment=cpsrComment(flags.valueAsInteger());
	model.updateCPSR(flags.value<edb::value32>(),comment);
}

void ArchProcessor::update_register_view(const QString &default_region_name, const State &state) {

	auto& model = getModel();
	if(state.empty()) {
		model.setCPUMode(RegisterViewModel::CPUMode::UNKNOWN);
		return;
	}

	model.setCPUMode(RegisterViewModel::CPUMode::Defined);

	updateGPRs(model,state,default_region_name);
	updateCPSR(model,state);

	if(just_attached_) {
		model.saveValues();
		just_attached_=false;
	}
	model.dataUpdateFinished();
}

std::unique_ptr<QMenu> ArchProcessor::register_item_context_menu(const Register& reg) {
	Q_UNUSED(reg);
	return util::make_unique<QMenu>(nullptr);
}

RegisterViewModelBase::Model& ArchProcessor::get_register_view_model() const {
	static RegisterViewModel model(0);
	return model;
}

void ArchProcessor::justAttached() {
	just_attached_=true;
}

bool ArchProcessor::is_executed(const edb::Instruction &inst, const State &state) const
{
	return is_jcc_taken(state.flags(), inst.condition_code());
}
