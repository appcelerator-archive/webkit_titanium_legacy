/*
 * Copyright (C) 2008 Apple Inc.
 * Copyright (C) 2009 University of Szeged
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MacroAssemblerARM_h
#define MacroAssemblerARM_h

#include <wtf/Platform.h>

#if ENABLE(ASSEMBLER) && PLATFORM(ARM)

#include "ARMAssembler.h"
#include "AbstractMacroAssembler.h"

namespace JSC {

class MacroAssemblerARM : public AbstractMacroAssembler<ARMAssembler> {
public:
    enum Condition {
        Equal = ARMAssembler::EQ,
        NotEqual = ARMAssembler::NE,
        Above = ARMAssembler::HI,
        AboveOrEqual = ARMAssembler::CS,
        Below = ARMAssembler::CC,
        BelowOrEqual = ARMAssembler::LS,
        GreaterThan = ARMAssembler::GT,
        GreaterThanOrEqual = ARMAssembler::GE,
        LessThan = ARMAssembler::LT,
        LessThanOrEqual = ARMAssembler::LE,
        Overflow = ARMAssembler::VS,
        Signed = ARMAssembler::MI,
        Zero = ARMAssembler::EQ,
        NonZero = ARMAssembler::NE
    };

    enum DoubleCondition {
        DoubleEqual, //FIXME
        DoubleNotEqual, //FIXME
        DoubleGreaterThan, //FIXME
        DoubleGreaterThanOrEqual, //FIXME
        DoubleLessThan, //FIXME
        DoubleLessThanOrEqual, //FIXME
    };

    static const RegisterID stackPointerRegister = ARM::sp;

    static const Scale ScalePtr = TimesFour;

    void add32(RegisterID src, RegisterID dest)
    {
        m_assembler.adds_r(dest, dest, src);
    }

    void add32(Imm32 imm, Address address)
    {
        load32(address, ARM::S1);
        add32(imm, ARM::S1);
        store32(ARM::S1, address);
    }

    void add32(Imm32 imm, RegisterID dest)
    {
        m_assembler.adds_r(dest, dest, m_assembler.getImm(imm.m_value, ARM::S0));
    }

    void add32(Address src, RegisterID dest)
    {
        load32(src, ARM::S1);
        add32(ARM::S1, dest);
    }

    void and32(RegisterID src, RegisterID dest)
    {
        m_assembler.ands_r(dest, dest, src);
    }

    void and32(Imm32 imm, RegisterID dest)
    {
        ARMWord w = m_assembler.getImm(imm.m_value, ARM::S0, true);
        if (w & ARMAssembler::OP2_INV_IMM)
            m_assembler.bics_r(dest, dest, w & ~ARMAssembler::OP2_INV_IMM);
        else
            m_assembler.ands_r(dest, dest, w);
    }

    void lshift32(Imm32 imm, RegisterID dest)
    {
        m_assembler.movs_r(dest, m_assembler.lsl(dest, imm.m_value & 0x1f));
    }

    void lshift32(RegisterID shift_amount, RegisterID dest)
    {
        m_assembler.movs_r(dest, m_assembler.lsl_r(dest, shift_amount));
    }

    void mul32(RegisterID src, RegisterID dest)
    {
        if (src == dest) {
            move(src, ARM::S0);
            src = ARM::S0;
        }
        m_assembler.muls_r(dest, dest, src);
    }

    void mul32(Imm32 imm, RegisterID src, RegisterID dest)
    {
        move(imm, ARM::S0);
        m_assembler.muls_r(dest, src, ARM::S0);
    }

    void not32(RegisterID dest)
    {
        m_assembler.mvns_r(dest, dest);
    }

    void or32(RegisterID src, RegisterID dest)
    {
        m_assembler.orrs_r(dest, dest, src);
    }

    void or32(Imm32 imm, RegisterID dest)
    {
        m_assembler.orrs_r(dest, dest, m_assembler.getImm(imm.m_value, ARM::S0));
    }

    void rshift32(RegisterID shift_amount, RegisterID dest)
    {
        m_assembler.movs_r(dest, m_assembler.asr_r(dest, shift_amount));
    }

    void rshift32(Imm32 imm, RegisterID dest)
    {
        m_assembler.movs_r(dest, m_assembler.asr(dest, imm.m_value & 0x1f));
    }

    void sub32(RegisterID src, RegisterID dest)
    {
        m_assembler.subs_r(dest, dest, src);
    }

    void sub32(Imm32 imm, RegisterID dest)
    {
        m_assembler.subs_r(dest, dest, m_assembler.getImm(imm.m_value, ARM::S0));
    }

    void sub32(Imm32 imm, Address address)
    {
        load32(address, ARM::S1);
        sub32(imm, ARM::S1);
        store32(ARM::S1, address);
    }

    void sub32(Address src, RegisterID dest)
    {
        load32(src, ARM::S1);
        sub32(ARM::S1, dest);
    }

    void xor32(RegisterID src, RegisterID dest)
    {
        m_assembler.eors_r(dest, dest, src);
    }

    void xor32(Imm32 imm, RegisterID dest)
    {
        m_assembler.eors_r(dest, dest, m_assembler.getImm(imm.m_value, ARM::S0));
    }

    void load32(ImplicitAddress address, RegisterID dest)
    {
        m_assembler.dataTransfer32(true, dest, address.base, address.offset);
    }

    void load32(BaseIndex address, RegisterID dest)
    {
        m_assembler.baseIndexTransfer32(true, dest, address.base, address.index, static_cast<int>(address.scale), address.offset);
    }

    DataLabel32 load32WithAddressOffsetPatch(Address address, RegisterID dest)
    {
        DataLabel32 dataLabel(this);
        m_assembler.ldr_un_imm(ARM::S0, 0);
        m_assembler.dtr_ur(true, dest, address.base, ARM::S0);
        return dataLabel;
    }

    Label loadPtrWithPatchToLEA(Address address, RegisterID dest)
    {
        Label label(this);
        load32(address, dest);
        return label;
    }

    void load16(BaseIndex address, RegisterID dest)
    {
        m_assembler.add_r(ARM::S0, address.base, m_assembler.lsl(address.index, address.scale));
        if (address.offset>=0)
            m_assembler.ldrh_u(dest, ARM::S0, ARMAssembler::getOp2Byte(address.offset));
        else
            m_assembler.ldrh_d(dest, ARM::S0, ARMAssembler::getOp2Byte(-address.offset));
    }

    DataLabel32 store32WithAddressOffsetPatch(RegisterID src, Address address)
    {
        DataLabel32 dataLabel(this);
        m_assembler.ldr_un_imm(ARM::S0, 0);
        m_assembler.dtr_ur(false, src, address.base, ARM::S0);
        return dataLabel;
    }

    void store32(RegisterID src, ImplicitAddress address)
    {
        m_assembler.dataTransfer32(false, src, address.base, address.offset);
    }

    void store32(RegisterID src, BaseIndex address)
    {
        m_assembler.baseIndexTransfer32(false, src, address.base, address.index, static_cast<int>(address.scale), address.offset);
    }

    void store32(Imm32 imm, ImplicitAddress address)
    {
        move(imm, ARM::S1);
        store32(ARM::S1, address);
    }

    void store32(RegisterID src, void* address)
    {
        m_assembler.moveImm(reinterpret_cast<ARMWord>(address), ARM::S0);
        m_assembler.dtr_u(false, src, ARM::S0, 0);
    }

    void store32(Imm32 imm, void* address)
    {
        m_assembler.moveImm(reinterpret_cast<ARMWord>(address), ARM::S0);
        m_assembler.moveImm(imm.m_value, ARM::S1);
        m_assembler.dtr_u(false, ARM::S1, ARM::S0, 0);
    }

    void pop(RegisterID dest)
    {
        m_assembler.pop_r(dest);
    }

    void push(RegisterID src)
    {
        m_assembler.push_r(src);
    }

    void push(Address address)
    {
        load32(address, ARM::S1);
        push(ARM::S1);
    }

    void push(Imm32 imm)
    {
        move(imm, ARM::S0);
        push(ARM::S0);
    }

    void move(Imm32 imm, RegisterID dest)
    {
        m_assembler.moveImm(imm.m_value, dest);
    }

    void move(RegisterID src, RegisterID dest)
    {
        m_assembler.mov_r(dest, src);
    }

    void move(ImmPtr imm, RegisterID dest)
    {
        m_assembler.mov_r(dest, m_assembler.getImm(reinterpret_cast<ARMWord>(imm.m_value), ARM::S0));
    }

    void swap(RegisterID reg1, RegisterID reg2)
    {
        m_assembler.mov_r(ARM::S0, reg1);
        m_assembler.mov_r(reg1, reg2);
        m_assembler.mov_r(reg2, ARM::S0);
    }

    void signExtend32ToPtr(RegisterID src, RegisterID dest)
    {
        if (src != dest)
            move(src, dest);
    }

    void zeroExtend32ToPtr(RegisterID src, RegisterID dest)
    {
        if (src != dest)
            move(src, dest);
    }

    Jump branch32(Condition cond, RegisterID left, RegisterID right)
    {
        m_assembler.cmp_r(left, right);
        return Jump(m_assembler.jmp(ARMCondition(cond)));
    }

    Jump branch32(Condition cond, RegisterID left, Imm32 right)
    {
        m_assembler.cmp_r(left, m_assembler.getImm(right.m_value, ARM::S0));
        return Jump(m_assembler.jmp(ARMCondition(cond)));
    }

    Jump branch32(Condition cond, RegisterID left, Address right)
    {
        load32(right, ARM::S1);
        return branch32(cond, left, ARM::S1);
    }

    Jump branch32(Condition cond, Address left, RegisterID right)
    {
        load32(left, ARM::S1);
        return branch32(cond, ARM::S1, right);
    }

    Jump branch32(Condition cond, Address left, Imm32 right)
    {
        load32(left, ARM::S1);
        return branch32(cond, ARM::S1, right);
    }

    Jump branch32(Condition cond, BaseIndex left, Imm32 right)
    {
        load32(left, ARM::S1);
        return branch32(cond, ARM::S1, right);
    }

    Jump branch16(Condition cond, BaseIndex left, RegisterID right)
    {
        UNUSED_PARAM(cond);
        UNUSED_PARAM(left);
        UNUSED_PARAM(right);
        ASSERT_NOT_REACHED();
        return jump();
    }

    Jump branch16(Condition cond, BaseIndex left, Imm32 right)
    {
        load16(left, ARM::S0);
        move(right, ARM::S1);
        m_assembler.cmp_r(ARM::S0, ARM::S1);
        return m_assembler.jmp(ARMCondition(cond));
    }

    Jump branchTest32(Condition cond, RegisterID reg, RegisterID mask)
    {
        ASSERT((cond == Zero) || (cond == NonZero));
        m_assembler.tst_r(reg, mask);
        return Jump(m_assembler.jmp(ARMCondition(cond)));
    }

    Jump branchTest32(Condition cond, RegisterID reg, Imm32 mask = Imm32(-1))
    {
        ASSERT((cond == Zero) || (cond == NonZero));
        ARMWord w = m_assembler.getImm(mask.m_value, ARM::S0, true);
        if (w & ARMAssembler::OP2_INV_IMM)
            m_assembler.bics_r(ARM::S0, reg, w & ~ARMAssembler::OP2_INV_IMM);
        else
            m_assembler.tst_r(reg, w);
        return Jump(m_assembler.jmp(ARMCondition(cond)));
    }

    Jump branchTest32(Condition cond, Address address, Imm32 mask = Imm32(-1))
    {
        load32(address, ARM::S1);
        return branchTest32(cond, ARM::S1, mask);
    }

    Jump branchTest32(Condition cond, BaseIndex address, Imm32 mask = Imm32(-1))
    {
        load32(address, ARM::S1);
        return branchTest32(cond, ARM::S1, mask);
    }

    Jump jump()
    {
        return Jump(m_assembler.jmp());
    }

    void jump(RegisterID target)
    {
        move(target, ARM::pc);
    }

    void jump(Address address)
    {
        load32(address, ARM::pc);
    }

    Jump branchAdd32(Condition cond, RegisterID src, RegisterID dest)
    {
        ASSERT((cond == Overflow) || (cond == Signed) || (cond == Zero) || (cond == NonZero));
        add32(src, dest);
        return Jump(m_assembler.jmp(ARMCondition(cond)));
    }

    Jump branchAdd32(Condition cond, Imm32 imm, RegisterID dest)
    {
        ASSERT((cond == Overflow) || (cond == Signed) || (cond == Zero) || (cond == NonZero));
        add32(imm, dest);
        return Jump(m_assembler.jmp(ARMCondition(cond)));
    }

    void mull32(RegisterID src1, RegisterID src2, RegisterID dest)
    {
        if (src1 == dest) {
            move(src1, ARM::S0);
            src1 = ARM::S0;
        }
        m_assembler.mull_r(ARM::S1, dest, src2, src1);
        m_assembler.cmp_r(ARM::S1, m_assembler.asr(dest, 31));
    }

    Jump branchMul32(Condition cond, RegisterID src, RegisterID dest)
    {
        ASSERT((cond == Overflow) || (cond == Signed) || (cond == Zero) || (cond == NonZero));
        if (cond == Overflow) {
            mull32(src, dest, dest);
            cond = NonZero;
        }
        else
            mul32(src, dest);
        return Jump(m_assembler.jmp(ARMCondition(cond)));
    }

    Jump branchMul32(Condition cond, Imm32 imm, RegisterID src, RegisterID dest)
    {
        ASSERT((cond == Overflow) || (cond == Signed) || (cond == Zero) || (cond == NonZero));
        if (cond == Overflow) {
            move(imm, ARM::S0);
            mull32(ARM::S0, src, dest);
            cond = NonZero;
        }
        else
            mul32(imm, src, dest);
        return Jump(m_assembler.jmp(ARMCondition(cond)));
    }

    Jump branchSub32(Condition cond, RegisterID src, RegisterID dest)
    {
        ASSERT((cond == Overflow) || (cond == Signed) || (cond == Zero) || (cond == NonZero));
        sub32(src, dest);
        return Jump(m_assembler.jmp(ARMCondition(cond)));
    }

    Jump branchSub32(Condition cond, Imm32 imm, RegisterID dest)
    {
        ASSERT((cond == Overflow) || (cond == Signed) || (cond == Zero) || (cond == NonZero));
        sub32(imm, dest);
        return Jump(m_assembler.jmp(ARMCondition(cond)));
    }

    void breakpoint()
    {
        m_assembler.bkpt(0);
    }

    Call nearCall()
    {
        prepareCall();
        return Call(m_assembler.jmp(), Call::LinkableNear);
    }

    Call call(RegisterID target)
    {
        prepareCall();
        move(ARM::pc, target);
        JmpSrc jmpSrc;
        return Call(jmpSrc, Call::None);
    }

    void call(Address address)
    {
        call32(address.base, address.offset);
    }

    void ret()
    {
        pop(ARM::pc);
    }

    void set32(Condition cond, RegisterID left, RegisterID right, RegisterID dest)
    {
        m_assembler.cmp_r(left, right);
        m_assembler.mov_r(dest, ARMAssembler::getOp2(0));
        m_assembler.mov_r(dest, ARMAssembler::getOp2(1), ARMCondition(cond));
    }

    void set32(Condition cond, RegisterID left, Imm32 right, RegisterID dest)
    {
        m_assembler.cmp_r(left, m_assembler.getImm(right.m_value, ARM::S0));
        m_assembler.mov_r(dest, ARMAssembler::getOp2(0));
        m_assembler.mov_r(dest, ARMAssembler::getOp2(1), ARMCondition(cond));
    }

    void setTest32(Condition cond, Address address, Imm32 mask, RegisterID dest)
    {
        load32(address, ARM::S1);
        if (mask.m_value == -1)
            m_assembler.cmp_r(0, ARM::S1);
        else
            m_assembler.tst_r(ARM::S1, m_assembler.getImm(mask.m_value, ARM::S0));
        m_assembler.mov_r(dest, ARMAssembler::getOp2(0));
        m_assembler.mov_r(dest, ARMAssembler::getOp2(1), ARMCondition(cond));
    }

    void add32(Imm32 imm, RegisterID src, RegisterID dest)
    {
        m_assembler.add_r(dest, src, m_assembler.getImm(imm.m_value, ARM::S0));
    }

    void add32(Imm32 imm, AbsoluteAddress address)
    {
        m_assembler.moveImm(reinterpret_cast<ARMWord>(address.m_ptr), ARM::S1);
        m_assembler.dtr_u(true, ARM::S1, ARM::S1, 0);
        add32(imm, ARM::S1);
        m_assembler.moveImm(reinterpret_cast<ARMWord>(address.m_ptr), ARM::S0);
        m_assembler.dtr_u(false, ARM::S1, ARM::S0, 0);
    }

    void sub32(Imm32 imm, AbsoluteAddress address)
    {
        m_assembler.moveImm(reinterpret_cast<ARMWord>(address.m_ptr), ARM::S1);
        m_assembler.dtr_u(true, ARM::S1, ARM::S1, 0);
        sub32(imm, ARM::S1);
        m_assembler.moveImm(reinterpret_cast<ARMWord>(address.m_ptr), ARM::S0);
        m_assembler.dtr_u(false, ARM::S1, ARM::S0, 0);
    }

    void load32(void* address, RegisterID dest)
    {
        m_assembler.moveImm(reinterpret_cast<ARMWord>(address), ARM::S0);
        m_assembler.dtr_u(true, dest, ARM::S0, 0);
    }

    Jump branch32(Condition cond, AbsoluteAddress left, RegisterID right)
    {
        load32(left.m_ptr, ARM::S1);
        return branch32(cond, ARM::S1, right);
    }

    Jump branch32(Condition cond, AbsoluteAddress left, Imm32 right)
    {
        load32(left.m_ptr, ARM::S1);
        return branch32(cond, ARM::S1, right);
    }

    Call call()
    {
        prepareCall();
        return Call(m_assembler.jmp(), Call::Linkable);
    }

    Call tailRecursiveCall()
    {
        return Call::fromTailJump(jump());
    }

    Call makeTailRecursiveCall(Jump oldJump)
    {
        return Call::fromTailJump(oldJump);
    }

    DataLabelPtr moveWithPatch(ImmPtr initialValue, RegisterID dest)
    {
        DataLabelPtr dataLabel(this);
        m_assembler.ldr_un_imm(dest, reinterpret_cast<ARMWord>(initialValue.m_value));
        return dataLabel;
    }

    Jump branchPtrWithPatch(Condition cond, RegisterID left, DataLabelPtr& dataLabel, ImmPtr initialRightValue = ImmPtr(0))
    {
        dataLabel = moveWithPatch(initialRightValue, ARM::S1);
        Jump jump = branch32(cond, left, ARM::S1);
        jump.enableLatePatch();
        return jump;
    }

    Jump branchPtrWithPatch(Condition cond, Address left, DataLabelPtr& dataLabel, ImmPtr initialRightValue = ImmPtr(0))
    {
        load32(left, ARM::S1);
        dataLabel = moveWithPatch(initialRightValue, ARM::S0);
        Jump jump = branch32(cond, ARM::S0, ARM::S1);
        jump.enableLatePatch();
        return jump;
    }

    DataLabelPtr storePtrWithPatch(ImmPtr initialValue, ImplicitAddress address)
    {
        DataLabelPtr dataLabel = moveWithPatch(initialValue, ARM::S1);
        store32(ARM::S1, address);
        return dataLabel;
    }

    DataLabelPtr storePtrWithPatch(ImplicitAddress address)
    {
        return storePtrWithPatch(ImmPtr(0), address);
    }

    // Floating point operators
    bool supportsFloatingPoint() const
    {
        return false;
    }

    bool supportsFloatingPointTruncate() const
    {
        return false;
    }

    void loadDouble(ImplicitAddress address, FPRegisterID dest)
    {
        UNUSED_PARAM(address);
        UNUSED_PARAM(dest);
        ASSERT_NOT_REACHED();
    }

    void storeDouble(FPRegisterID src, ImplicitAddress address)
    {
        UNUSED_PARAM(src);
        UNUSED_PARAM(address);
        ASSERT_NOT_REACHED();
    }

    void addDouble(FPRegisterID src, FPRegisterID dest)
    {
        UNUSED_PARAM(src);
        UNUSED_PARAM(dest);
        ASSERT_NOT_REACHED();
    }

    void addDouble(Address src, FPRegisterID dest)
    {
        UNUSED_PARAM(src);
        UNUSED_PARAM(dest);
        ASSERT_NOT_REACHED();
    }

    void subDouble(FPRegisterID src, FPRegisterID dest)
    {
        UNUSED_PARAM(src);
        UNUSED_PARAM(dest);
        ASSERT_NOT_REACHED();
    }

    void subDouble(Address src, FPRegisterID dest)
    {
        UNUSED_PARAM(src);
        UNUSED_PARAM(dest);
        ASSERT_NOT_REACHED();
    }

    void mulDouble(FPRegisterID src, FPRegisterID dest)
    {
        UNUSED_PARAM(src);
        UNUSED_PARAM(dest);
        ASSERT_NOT_REACHED();
    }

    void mulDouble(Address src, FPRegisterID dest)
    {
        UNUSED_PARAM(src);
        UNUSED_PARAM(dest);
        ASSERT_NOT_REACHED();
    }

    void convertInt32ToDouble(RegisterID src, FPRegisterID dest)
    {
        UNUSED_PARAM(src);
        UNUSED_PARAM(dest);
        ASSERT_NOT_REACHED();
    }

    Jump branchDouble(DoubleCondition cond, FPRegisterID left, FPRegisterID right)
    {
        UNUSED_PARAM(cond);
        UNUSED_PARAM(left);
        UNUSED_PARAM(right);
        ASSERT_NOT_REACHED();
        return jump();
    }

    // Truncates 'src' to an integer, and places the resulting 'dest'.
    // If the result is not representable as a 32 bit value, branch.
    // May also branch for some values that are representable in 32 bits
    // (specifically, in this case, INT_MIN).
    Jump branchTruncateDoubleToInt32(FPRegisterID src, RegisterID dest)
    {
        UNUSED_PARAM(src);
        UNUSED_PARAM(dest);
        ASSERT_NOT_REACHED();
        return jump();
    }

protected:
    ARMAssembler::Condition ARMCondition(Condition cond)
    {
        return static_cast<ARMAssembler::Condition>(cond);
    }

    void prepareCall()
    {
        m_assembler.ensureSpace(3 * sizeof(ARMWord), sizeof(ARMWord));

        // S0 might be used for parameter passing
        m_assembler.add_r(ARM::S1, ARM::pc, ARMAssembler::OP2_IMM | 0x4);
        m_assembler.push_r(ARM::S1);
    }

    void call32(RegisterID base, int32_t offset)
    {
        if (base == ARM::sp)
            offset += 4;

        if (offset >= 0) {
            if (offset <= 0xfff) {
                prepareCall();
                m_assembler.dtr_u(true, ARM::pc, base, offset);
            } else if (offset <= 0xfffff) {
                m_assembler.add_r(ARM::S0, base, ARMAssembler::OP2_IMM | (offset >> 12) | (10 << 8));
                prepareCall();
                m_assembler.dtr_u(true, ARM::pc, ARM::S0, offset & 0xfff);
            } else {
                ARMWord reg = m_assembler.getImm(offset, ARM::S0);
                prepareCall();
                m_assembler.dtr_ur(true, ARM::pc, base, reg);
            }
        } else  {
            offset = -offset;
            if (offset <= 0xfff) {
                prepareCall();
                m_assembler.dtr_d(true, ARM::pc, base, offset);
            } else if (offset <= 0xfffff) {
                m_assembler.sub_r(ARM::S0, base, ARMAssembler::OP2_IMM | (offset >> 12) | (10 << 8));
                prepareCall();
                m_assembler.dtr_d(true, ARM::pc, ARM::S0, offset & 0xfff);
            } else {
                ARMWord reg = m_assembler.getImm(offset, ARM::S0);
                prepareCall();
                m_assembler.dtr_dr(true, ARM::pc, base, reg);
            }
        }
    }

private:
    friend class LinkBuffer;
    friend class RepatchBuffer;

    static void linkCall(void* code, Call call, FunctionPtr function)
    {
        ARMAssembler::linkCall(code, call.m_jmp, function.value());
    }

    static void repatchCall(CodeLocationCall call, CodeLocationLabel destination)
    {
        ARMAssembler::relinkCall(call.dataLocation(), destination.executableAddress());
    }

    static void repatchCall(CodeLocationCall call, FunctionPtr destination)
    {
        ARMAssembler::relinkCall(call.dataLocation(), destination.executableAddress());
    }

};

}

#endif

#endif // MacroAssemblerARM_h
