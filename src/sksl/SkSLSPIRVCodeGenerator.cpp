/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
 
#include "SkSLSPIRVCodeGenerator.h"

#include "string.h"

#include "GLSL.std.450.h"

#include "ir/SkSLExpressionStatement.h"
#include "ir/SkSLExtension.h"
#include "ir/SkSLIndexExpression.h"
#include "ir/SkSLVariableReference.h"

namespace SkSL {

#define SPIRV_DEBUG 0

static const int32_t SKSL_MAGIC  = 0x0; // FIXME: we should probably register a magic number

void SPIRVCodeGenerator::setupIntrinsics() {
#define ALL_GLSL(x) std::make_tuple(kGLSL_STD_450_IntrinsicKind, GLSLstd450 ## x, GLSLstd450 ## x, \
                                    GLSLstd450 ## x, GLSLstd450 ## x)
#define BY_TYPE_GLSL(ifFloat, ifInt, ifUInt) std::make_tuple(kGLSL_STD_450_IntrinsicKind, \
                                                             GLSLstd450 ## ifFloat, \
                                                             GLSLstd450 ## ifInt, \
                                                             GLSLstd450 ## ifUInt, \
                                                             SpvOpUndef)
#define SPECIAL(x) std::make_tuple(kSpecial_IntrinsicKind, k ## x ## _SpecialIntrinsic, \
                                   k ## x ## _SpecialIntrinsic, k ## x ## _SpecialIntrinsic, \
                                   k ## x ## _SpecialIntrinsic)
    fIntrinsicMap["round"]         = ALL_GLSL(Round);
    fIntrinsicMap["roundEven"]     = ALL_GLSL(RoundEven);
    fIntrinsicMap["trunc"]         = ALL_GLSL(Trunc);
    fIntrinsicMap["abs"]           = BY_TYPE_GLSL(FAbs, SAbs, SAbs);
    fIntrinsicMap["sign"]          = BY_TYPE_GLSL(FSign, SSign, SSign);
    fIntrinsicMap["floor"]         = ALL_GLSL(Floor);
    fIntrinsicMap["ceil"]          = ALL_GLSL(Ceil);
    fIntrinsicMap["fract"]         = ALL_GLSL(Fract);
    fIntrinsicMap["radians"]       = ALL_GLSL(Radians);
    fIntrinsicMap["degrees"]       = ALL_GLSL(Degrees);
    fIntrinsicMap["sin"]           = ALL_GLSL(Sin);
    fIntrinsicMap["cos"]           = ALL_GLSL(Cos);
    fIntrinsicMap["tan"]           = ALL_GLSL(Tan);
    fIntrinsicMap["asin"]          = ALL_GLSL(Asin);
    fIntrinsicMap["acos"]          = ALL_GLSL(Acos);
    fIntrinsicMap["atan"]          = SPECIAL(Atan);
    fIntrinsicMap["sinh"]          = ALL_GLSL(Sinh);
    fIntrinsicMap["cosh"]          = ALL_GLSL(Cosh);
    fIntrinsicMap["tanh"]          = ALL_GLSL(Tanh);
    fIntrinsicMap["asinh"]         = ALL_GLSL(Asinh);
    fIntrinsicMap["acosh"]         = ALL_GLSL(Acosh);
    fIntrinsicMap["atanh"]         = ALL_GLSL(Atanh);
    fIntrinsicMap["pow"]           = ALL_GLSL(Pow);
    fIntrinsicMap["exp"]           = ALL_GLSL(Exp);
    fIntrinsicMap["log"]           = ALL_GLSL(Log);
    fIntrinsicMap["exp2"]          = ALL_GLSL(Exp2);
    fIntrinsicMap["log2"]          = ALL_GLSL(Log2);
    fIntrinsicMap["sqrt"]          = ALL_GLSL(Sqrt);
    fIntrinsicMap["inversesqrt"]   = ALL_GLSL(InverseSqrt);
    fIntrinsicMap["determinant"]   = ALL_GLSL(Determinant);
    fIntrinsicMap["matrixInverse"] = ALL_GLSL(MatrixInverse);
    fIntrinsicMap["mod"]           = std::make_tuple(kSPIRV_IntrinsicKind, SpvOpFMod, SpvOpSMod, 
                                                     SpvOpUMod, SpvOpUndef);
    fIntrinsicMap["min"]           = BY_TYPE_GLSL(FMin, SMin, UMin);
    fIntrinsicMap["max"]           = BY_TYPE_GLSL(FMax, SMax, UMax);
    fIntrinsicMap["clamp"]         = BY_TYPE_GLSL(FClamp, SClamp, UClamp);
    fIntrinsicMap["dot"]           = std::make_tuple(kSPIRV_IntrinsicKind, SpvOpDot, SpvOpUndef,
                                                     SpvOpUndef, SpvOpUndef);
    fIntrinsicMap["mix"]           = ALL_GLSL(FMix);
    fIntrinsicMap["step"]          = ALL_GLSL(Step);
    fIntrinsicMap["smoothstep"]    = ALL_GLSL(SmoothStep);
    fIntrinsicMap["fma"]           = ALL_GLSL(Fma);
    fIntrinsicMap["frexp"]         = ALL_GLSL(Frexp);
    fIntrinsicMap["ldexp"]         = ALL_GLSL(Ldexp);

#define PACK(type) fIntrinsicMap["pack" #type] = ALL_GLSL(Pack ## type); \
                   fIntrinsicMap["unpack" #type] = ALL_GLSL(Unpack ## type)
    PACK(Snorm4x8);
    PACK(Unorm4x8);
    PACK(Snorm2x16);
    PACK(Unorm2x16);
    PACK(Half2x16);
    PACK(Double2x32);
    fIntrinsicMap["length"]      = ALL_GLSL(Length);
    fIntrinsicMap["distance"]    = ALL_GLSL(Distance);
    fIntrinsicMap["cross"]       = ALL_GLSL(Cross);
    fIntrinsicMap["normalize"]   = ALL_GLSL(Normalize);
    fIntrinsicMap["faceForward"] = ALL_GLSL(FaceForward);
    fIntrinsicMap["reflect"]     = ALL_GLSL(Reflect);
    fIntrinsicMap["refract"]     = ALL_GLSL(Refract);
    fIntrinsicMap["findLSB"]     = ALL_GLSL(FindILsb);
    fIntrinsicMap["findMSB"]     = BY_TYPE_GLSL(FindSMsb, FindSMsb, FindUMsb);
    fIntrinsicMap["dFdx"]        = std::make_tuple(kSPIRV_IntrinsicKind, SpvOpDPdx, SpvOpUndef,
                                                   SpvOpUndef, SpvOpUndef);
    fIntrinsicMap["dFdy"]        = std::make_tuple(kSPIRV_IntrinsicKind, SpvOpDPdy, SpvOpUndef,
                                                   SpvOpUndef, SpvOpUndef);
    fIntrinsicMap["dFdy"]        = std::make_tuple(kSPIRV_IntrinsicKind, SpvOpDPdy, SpvOpUndef,
                                                   SpvOpUndef, SpvOpUndef);
    fIntrinsicMap["texture"]     = SPECIAL(Texture);
    fIntrinsicMap["texture2D"]   = SPECIAL(Texture2D);
    fIntrinsicMap["textureProj"] = SPECIAL(TextureProj);

    fIntrinsicMap["any"]              = std::make_tuple(kSPIRV_IntrinsicKind, SpvOpUndef, 
                                                        SpvOpUndef, SpvOpUndef, SpvOpAny);
    fIntrinsicMap["all"]              = std::make_tuple(kSPIRV_IntrinsicKind, SpvOpUndef, 
                                                        SpvOpUndef, SpvOpUndef, SpvOpAll);
    fIntrinsicMap["equal"]            = std::make_tuple(kSPIRV_IntrinsicKind, SpvOpFOrdEqual, 
                                                        SpvOpIEqual, SpvOpIEqual, 
                                                        SpvOpLogicalEqual);
    fIntrinsicMap["notEqual"]         = std::make_tuple(kSPIRV_IntrinsicKind, SpvOpFOrdNotEqual, 
                                                        SpvOpINotEqual, SpvOpINotEqual, 
                                                        SpvOpLogicalNotEqual);
    fIntrinsicMap["lessThan"]         = std::make_tuple(kSPIRV_IntrinsicKind, SpvOpSLessThan, 
                                                        SpvOpULessThan, SpvOpFOrdLessThan, 
                                                        SpvOpUndef);
    fIntrinsicMap["lessThanEqual"]    = std::make_tuple(kSPIRV_IntrinsicKind, SpvOpSLessThanEqual, 
                                                        SpvOpULessThanEqual, SpvOpFOrdLessThanEqual,
                                                        SpvOpUndef);
    fIntrinsicMap["greaterThan"]      = std::make_tuple(kSPIRV_IntrinsicKind, SpvOpSGreaterThan, 
                                                        SpvOpUGreaterThan, SpvOpFOrdGreaterThan, 
                                                        SpvOpUndef);
    fIntrinsicMap["greaterThanEqual"] = std::make_tuple(kSPIRV_IntrinsicKind, 
                                                        SpvOpSGreaterThanEqual, 
                                                        SpvOpUGreaterThanEqual, 
                                                        SpvOpFOrdGreaterThanEqual,
                                                        SpvOpUndef);

// interpolateAt* not yet supported...
}

void SPIRVCodeGenerator::writeWord(int32_t word, std::ostream& out) {
#if SPIRV_DEBUG
    out << "(" << word << ") ";
#else
    out.write((const char*) &word, sizeof(word));
#endif
}

static bool is_float(const Type& type) {
    if (type.kind() == Type::kVector_Kind) {
        return is_float(*type.componentType());
    }
    return type == *kFloat_Type || type == *kDouble_Type;
}

static bool is_signed(const Type& type) {
    if (type.kind() == Type::kVector_Kind) {
        return is_signed(*type.componentType());
    }
    return type == *kInt_Type;
}

static bool is_unsigned(const Type& type) {
    if (type.kind() == Type::kVector_Kind) {
        return is_unsigned(*type.componentType());
    }
    return type == *kUInt_Type;
}

static bool is_bool(const Type& type) {
    if (type.kind() == Type::kVector_Kind) {
        return is_bool(*type.componentType());
    }
    return type == *kBool_Type;
}

static bool is_out(std::shared_ptr<Variable> var) {
    return (var->fModifiers.fFlags & Modifiers::kOut_Flag) != 0;
}

#if SPIRV_DEBUG
static std::string opcode_text(SpvOp_ opCode) {
    switch (opCode) {
        case SpvOpNop:
            return "Nop";
        case SpvOpUndef:
            return "Undef";
        case SpvOpSourceContinued:
            return "SourceContinued";
        case SpvOpSource:
            return "Source";
        case SpvOpSourceExtension:
            return "SourceExtension";
        case SpvOpName:
            return "Name";
        case SpvOpMemberName:
            return "MemberName";
        case SpvOpString:
            return "String";
        case SpvOpLine:
            return "Line";
        case SpvOpExtension:
            return "Extension";
        case SpvOpExtInstImport:
            return "ExtInstImport";
        case SpvOpExtInst:
            return "ExtInst";
        case SpvOpMemoryModel:
            return "MemoryModel";
        case SpvOpEntryPoint:
            return "EntryPoint";
        case SpvOpExecutionMode:
            return "ExecutionMode";
        case SpvOpCapability:
            return "Capability";
        case SpvOpTypeVoid:
            return "TypeVoid";
        case SpvOpTypeBool:
            return "TypeBool";
        case SpvOpTypeInt:
            return "TypeInt";
        case SpvOpTypeFloat:
            return "TypeFloat";
        case SpvOpTypeVector:
            return "TypeVector";
        case SpvOpTypeMatrix:
            return "TypeMatrix";
        case SpvOpTypeImage:
            return "TypeImage";
        case SpvOpTypeSampler:
            return "TypeSampler";
        case SpvOpTypeSampledImage:
            return "TypeSampledImage";
        case SpvOpTypeArray:
            return "TypeArray";
        case SpvOpTypeRuntimeArray:
            return "TypeRuntimeArray";
        case SpvOpTypeStruct:
            return "TypeStruct";
        case SpvOpTypeOpaque:
            return "TypeOpaque";
        case SpvOpTypePointer:
            return "TypePointer";
        case SpvOpTypeFunction:
            return "TypeFunction";
        case SpvOpTypeEvent:
            return "TypeEvent";
        case SpvOpTypeDeviceEvent:
            return "TypeDeviceEvent";
        case SpvOpTypeReserveId:
            return "TypeReserveId";
        case SpvOpTypeQueue:
            return "TypeQueue";
        case SpvOpTypePipe:
            return "TypePipe";
        case SpvOpTypeForwardPointer:
            return "TypeForwardPointer";
        case SpvOpConstantTrue:
            return "ConstantTrue";
        case SpvOpConstantFalse:
            return "ConstantFalse";
        case SpvOpConstant:
            return "Constant";
        case SpvOpConstantComposite:
            return "ConstantComposite";
        case SpvOpConstantSampler:
            return "ConstantSampler";
        case SpvOpConstantNull:
            return "ConstantNull";
        case SpvOpSpecConstantTrue:
            return "SpecConstantTrue";
        case SpvOpSpecConstantFalse:
            return "SpecConstantFalse";
        case SpvOpSpecConstant:
            return "SpecConstant";
        case SpvOpSpecConstantComposite:
            return "SpecConstantComposite";
        case SpvOpSpecConstantOp:
            return "SpecConstantOp";
        case SpvOpFunction:
            return "Function";
        case SpvOpFunctionParameter:
            return "FunctionParameter";
        case SpvOpFunctionEnd:
            return "FunctionEnd";
        case SpvOpFunctionCall:
            return "FunctionCall";
        case SpvOpVariable:
            return "Variable";
        case SpvOpImageTexelPointer:
            return "ImageTexelPointer";
        case SpvOpLoad:
            return "Load";
        case SpvOpStore:
            return "Store";
        case SpvOpCopyMemory:
            return "CopyMemory";
        case SpvOpCopyMemorySized:
            return "CopyMemorySized";
        case SpvOpAccessChain:
            return "AccessChain";
        case SpvOpInBoundsAccessChain:
            return "InBoundsAccessChain";
        case SpvOpPtrAccessChain:
            return "PtrAccessChain";
        case SpvOpArrayLength:
            return "ArrayLength";
        case SpvOpGenericPtrMemSemantics:
            return "GenericPtrMemSemantics";
        case SpvOpInBoundsPtrAccessChain:
            return "InBoundsPtrAccessChain";
        case SpvOpDecorate:
            return "Decorate";
        case SpvOpMemberDecorate:
            return "MemberDecorate";
        case SpvOpDecorationGroup:
            return "DecorationGroup";
        case SpvOpGroupDecorate:
            return "GroupDecorate";
        case SpvOpGroupMemberDecorate:
            return "GroupMemberDecorate";
        case SpvOpVectorExtractDynamic:
            return "VectorExtractDynamic";
        case SpvOpVectorInsertDynamic:
            return "VectorInsertDynamic";
        case SpvOpVectorShuffle:
            return "VectorShuffle";
        case SpvOpCompositeConstruct:
            return "CompositeConstruct";
        case SpvOpCompositeExtract:
            return "CompositeExtract";
        case SpvOpCompositeInsert:
            return "CompositeInsert";
        case SpvOpCopyObject:
            return "CopyObject";
        case SpvOpTranspose:
            return "Transpose";
        case SpvOpSampledImage:
            return "SampledImage";
        case SpvOpImageSampleImplicitLod:
            return "ImageSampleImplicitLod";
        case SpvOpImageSampleExplicitLod:
            return "ImageSampleExplicitLod";
        case SpvOpImageSampleDrefImplicitLod:
            return "ImageSampleDrefImplicitLod";
        case SpvOpImageSampleDrefExplicitLod:
            return "ImageSampleDrefExplicitLod";
        case SpvOpImageSampleProjImplicitLod:
            return "ImageSampleProjImplicitLod";
        case SpvOpImageSampleProjExplicitLod:
            return "ImageSampleProjExplicitLod";
        case SpvOpImageSampleProjDrefImplicitLod:
            return "ImageSampleProjDrefImplicitLod";
        case SpvOpImageSampleProjDrefExplicitLod:
            return "ImageSampleProjDrefExplicitLod";
        case SpvOpImageFetch:
            return "ImageFetch";
        case SpvOpImageGather:
            return "ImageGather";
        case SpvOpImageDrefGather:
            return "ImageDrefGather";
        case SpvOpImageRead:
            return "ImageRead";
        case SpvOpImageWrite:
            return "ImageWrite";
        case SpvOpImage:
            return "Image";
        case SpvOpImageQueryFormat:
            return "ImageQueryFormat";
        case SpvOpImageQueryOrder:
            return "ImageQueryOrder";
        case SpvOpImageQuerySizeLod:
            return "ImageQuerySizeLod";
        case SpvOpImageQuerySize:
            return "ImageQuerySize";
        case SpvOpImageQueryLod:
            return "ImageQueryLod";
        case SpvOpImageQueryLevels:
            return "ImageQueryLevels";
        case SpvOpImageQuerySamples:
            return "ImageQuerySamples";
        case SpvOpConvertFToU:
            return "ConvertFToU";
        case SpvOpConvertFToS:
            return "ConvertFToS";
        case SpvOpConvertSToF:
            return "ConvertSToF";
        case SpvOpConvertUToF:
            return "ConvertUToF";
        case SpvOpUConvert:
            return "UConvert";
        case SpvOpSConvert:
            return "SConvert";
        case SpvOpFConvert:
            return "FConvert";
        case SpvOpQuantizeToF16:
            return "QuantizeToF16";
        case SpvOpConvertPtrToU:
            return "ConvertPtrToU";
        case SpvOpSatConvertSToU:
            return "SatConvertSToU";
        case SpvOpSatConvertUToS:
            return "SatConvertUToS";
        case SpvOpConvertUToPtr:
            return "ConvertUToPtr";
        case SpvOpPtrCastToGeneric:
            return "PtrCastToGeneric";
        case SpvOpGenericCastToPtr:
            return "GenericCastToPtr";
        case SpvOpGenericCastToPtrExplicit:
            return "GenericCastToPtrExplicit";
        case SpvOpBitcast:
            return "Bitcast";
        case SpvOpSNegate:
            return "SNegate";
        case SpvOpFNegate:
            return "FNegate";
        case SpvOpIAdd:
            return "IAdd";
        case SpvOpFAdd:
            return "FAdd";
        case SpvOpISub:
            return "ISub";
        case SpvOpFSub:
            return "FSub";
        case SpvOpIMul:
            return "IMul";
        case SpvOpFMul:
            return "FMul";
        case SpvOpUDiv:
            return "UDiv";
        case SpvOpSDiv:
            return "SDiv";
        case SpvOpFDiv:
            return "FDiv";
        case SpvOpUMod:
            return "UMod";
        case SpvOpSRem:
            return "SRem";
        case SpvOpSMod:
            return "SMod";
        case SpvOpFRem:
            return "FRem";
        case SpvOpFMod:
            return "FMod";
        case SpvOpVectorTimesScalar:
            return "VectorTimesScalar";
        case SpvOpMatrixTimesScalar:
            return "MatrixTimesScalar";
        case SpvOpVectorTimesMatrix:
            return "VectorTimesMatrix";
        case SpvOpMatrixTimesVector:
            return "MatrixTimesVector";
        case SpvOpMatrixTimesMatrix:
            return "MatrixTimesMatrix";
        case SpvOpOuterProduct:
            return "OuterProduct";
        case SpvOpDot:
            return "Dot";
        case SpvOpIAddCarry:
            return "IAddCarry";
        case SpvOpISubBorrow:
            return "ISubBorrow";
        case SpvOpUMulExtended:
            return "UMulExtended";
        case SpvOpSMulExtended:
            return "SMulExtended";
        case SpvOpAny:
            return "Any";
        case SpvOpAll:
            return "All";
        case SpvOpIsNan:
            return "IsNan";
        case SpvOpIsInf:
            return "IsInf";
        case SpvOpIsFinite:
            return "IsFinite";
        case SpvOpIsNormal:
            return "IsNormal";
        case SpvOpSignBitSet:
            return "SignBitSet";
        case SpvOpLessOrGreater:
            return "LessOrGreater";
        case SpvOpOrdered:
            return "Ordered";
        case SpvOpUnordered:
            return "Unordered";
        case SpvOpLogicalEqual:
            return "LogicalEqual";
        case SpvOpLogicalNotEqual:
            return "LogicalNotEqual";
        case SpvOpLogicalOr:
            return "LogicalOr";
        case SpvOpLogicalAnd:
            return "LogicalAnd";
        case SpvOpLogicalNot:
            return "LogicalNot";
        case SpvOpSelect:
            return "Select";
        case SpvOpIEqual:
            return "IEqual";
        case SpvOpINotEqual:
            return "INotEqual";
        case SpvOpUGreaterThan:
            return "UGreaterThan";
        case SpvOpSGreaterThan:
            return "SGreaterThan";
        case SpvOpUGreaterThanEqual:
            return "UGreaterThanEqual";
        case SpvOpSGreaterThanEqual:
            return "SGreaterThanEqual";
        case SpvOpULessThan:
            return "ULessThan";
        case SpvOpSLessThan:
            return "SLessThan";
        case SpvOpULessThanEqual:
            return "ULessThanEqual";
        case SpvOpSLessThanEqual:
            return "SLessThanEqual";
        case SpvOpFOrdEqual:
            return "FOrdEqual";
        case SpvOpFUnordEqual:
            return "FUnordEqual";
        case SpvOpFOrdNotEqual:
            return "FOrdNotEqual";
        case SpvOpFUnordNotEqual:
            return "FUnordNotEqual";
        case SpvOpFOrdLessThan:
            return "FOrdLessThan";
        case SpvOpFUnordLessThan:
            return "FUnordLessThan";
        case SpvOpFOrdGreaterThan:
            return "FOrdGreaterThan";
        case SpvOpFUnordGreaterThan:
            return "FUnordGreaterThan";
        case SpvOpFOrdLessThanEqual:
            return "FOrdLessThanEqual";
        case SpvOpFUnordLessThanEqual:
            return "FUnordLessThanEqual";
        case SpvOpFOrdGreaterThanEqual:
            return "FOrdGreaterThanEqual";
        case SpvOpFUnordGreaterThanEqual:
            return "FUnordGreaterThanEqual";
        case SpvOpShiftRightLogical:
            return "ShiftRightLogical";
        case SpvOpShiftRightArithmetic:
            return "ShiftRightArithmetic";
        case SpvOpShiftLeftLogical:
            return "ShiftLeftLogical";
        case SpvOpBitwiseOr:
            return "BitwiseOr";
        case SpvOpBitwiseXor:
            return "BitwiseXor";
        case SpvOpBitwiseAnd:
            return "BitwiseAnd";
        case SpvOpNot:
            return "Not";
        case SpvOpBitFieldInsert:
            return "BitFieldInsert";
        case SpvOpBitFieldSExtract:
            return "BitFieldSExtract";
        case SpvOpBitFieldUExtract:
            return "BitFieldUExtract";
        case SpvOpBitReverse:
            return "BitReverse";
        case SpvOpBitCount:
            return "BitCount";
        case SpvOpDPdx:
            return "DPdx";
        case SpvOpDPdy:
            return "DPdy";
        case SpvOpFwidth:
            return "Fwidth";
        case SpvOpDPdxFine:
            return "DPdxFine";
        case SpvOpDPdyFine:
            return "DPdyFine";
        case SpvOpFwidthFine:
            return "FwidthFine";
        case SpvOpDPdxCoarse:
            return "DPdxCoarse";
        case SpvOpDPdyCoarse:
            return "DPdyCoarse";
        case SpvOpFwidthCoarse:
            return "FwidthCoarse";
        case SpvOpEmitVertex:
            return "EmitVertex";
        case SpvOpEndPrimitive:
            return "EndPrimitive";
        case SpvOpEmitStreamVertex:
            return "EmitStreamVertex";
        case SpvOpEndStreamPrimitive:
            return "EndStreamPrimitive";
        case SpvOpControlBarrier:
            return "ControlBarrier";
        case SpvOpMemoryBarrier:
            return "MemoryBarrier";
        case SpvOpAtomicLoad:
            return "AtomicLoad";
        case SpvOpAtomicStore:
            return "AtomicStore";
        case SpvOpAtomicExchange:
            return "AtomicExchange";
        case SpvOpAtomicCompareExchange:
            return "AtomicCompareExchange";
        case SpvOpAtomicCompareExchangeWeak:
            return "AtomicCompareExchangeWeak";
        case SpvOpAtomicIIncrement:
            return "AtomicIIncrement";
        case SpvOpAtomicIDecrement:
            return "AtomicIDecrement";
        case SpvOpAtomicIAdd:
            return "AtomicIAdd";
        case SpvOpAtomicISub:
            return "AtomicISub";
        case SpvOpAtomicSMin:
            return "AtomicSMin";
        case SpvOpAtomicUMin:
            return "AtomicUMin";
        case SpvOpAtomicSMax:
            return "AtomicSMax";
        case SpvOpAtomicUMax:
            return "AtomicUMax";
        case SpvOpAtomicAnd:
            return "AtomicAnd";
        case SpvOpAtomicOr:
            return "AtomicOr";
        case SpvOpAtomicXor:
            return "AtomicXor";
        case SpvOpPhi:
            return "Phi";
        case SpvOpLoopMerge:
            return "LoopMerge";
        case SpvOpSelectionMerge:
            return "SelectionMerge";
        case SpvOpLabel:
            return "Label";
        case SpvOpBranch:
            return "Branch";
        case SpvOpBranchConditional:
            return "BranchConditional";
        case SpvOpSwitch:
            return "Switch";
        case SpvOpKill:
            return "Kill";
        case SpvOpReturn:
            return "Return";
        case SpvOpReturnValue:
            return "ReturnValue";
        case SpvOpUnreachable:
            return "Unreachable";
        case SpvOpLifetimeStart:
            return "LifetimeStart";
        case SpvOpLifetimeStop:
            return "LifetimeStop";
        case SpvOpGroupAsyncCopy:
            return "GroupAsyncCopy";
        case SpvOpGroupWaitEvents:
            return "GroupWaitEvents";
        case SpvOpGroupAll:
            return "GroupAll";
        case SpvOpGroupAny:
            return "GroupAny";
        case SpvOpGroupBroadcast:
            return "GroupBroadcast";
        case SpvOpGroupIAdd:
            return "GroupIAdd";
        case SpvOpGroupFAdd:
            return "GroupFAdd";
        case SpvOpGroupFMin:
            return "GroupFMin";
        case SpvOpGroupUMin:
            return "GroupUMin";
        case SpvOpGroupSMin:
            return "GroupSMin";
        case SpvOpGroupFMax:
            return "GroupFMax";
        case SpvOpGroupUMax:
            return "GroupUMax";
        case SpvOpGroupSMax:
            return "GroupSMax";
        case SpvOpReadPipe:
            return "ReadPipe";
        case SpvOpWritePipe:
            return "WritePipe";
        case SpvOpReservedReadPipe:
            return "ReservedReadPipe";
        case SpvOpReservedWritePipe:
            return "ReservedWritePipe";
        case SpvOpReserveReadPipePackets:
            return "ReserveReadPipePackets";
        case SpvOpReserveWritePipePackets:
            return "ReserveWritePipePackets";
        case SpvOpCommitReadPipe:
            return "CommitReadPipe";
        case SpvOpCommitWritePipe:
            return "CommitWritePipe";
        case SpvOpIsValidReserveId:
            return "IsValidReserveId";
        case SpvOpGetNumPipePackets:
            return "GetNumPipePackets";
        case SpvOpGetMaxPipePackets:
            return "GetMaxPipePackets";
        case SpvOpGroupReserveReadPipePackets:
            return "GroupReserveReadPipePackets";
        case SpvOpGroupReserveWritePipePackets:
            return "GroupReserveWritePipePackets";
        case SpvOpGroupCommitReadPipe:
            return "GroupCommitReadPipe";
        case SpvOpGroupCommitWritePipe:
            return "GroupCommitWritePipe";
        case SpvOpEnqueueMarker:
            return "EnqueueMarker";
        case SpvOpEnqueueKernel:
            return "EnqueueKernel";
        case SpvOpGetKernelNDrangeSubGroupCount:
            return "GetKernelNDrangeSubGroupCount";
        case SpvOpGetKernelNDrangeMaxSubGroupSize:
            return "GetKernelNDrangeMaxSubGroupSize";
        case SpvOpGetKernelWorkGroupSize:
            return "GetKernelWorkGroupSize";
        case SpvOpGetKernelPreferredWorkGroupSizeMultiple:
            return "GetKernelPreferredWorkGroupSizeMultiple";
        case SpvOpRetainEvent:
            return "RetainEvent";
        case SpvOpReleaseEvent:
            return "ReleaseEvent";
        case SpvOpCreateUserEvent:
            return "CreateUserEvent";
        case SpvOpIsValidEvent:
            return "IsValidEvent";
        case SpvOpSetUserEventStatus:
            return "SetUserEventStatus";
        case SpvOpCaptureEventProfilingInfo:
            return "CaptureEventProfilingInfo";
        case SpvOpGetDefaultQueue:
            return "GetDefaultQueue";
        case SpvOpBuildNDRange:
            return "BuildNDRange";
        case SpvOpImageSparseSampleImplicitLod:
            return "ImageSparseSampleImplicitLod";
        case SpvOpImageSparseSampleExplicitLod:
            return "ImageSparseSampleExplicitLod";
        case SpvOpImageSparseSampleDrefImplicitLod:
            return "ImageSparseSampleDrefImplicitLod";
        case SpvOpImageSparseSampleDrefExplicitLod:
            return "ImageSparseSampleDrefExplicitLod";
        case SpvOpImageSparseSampleProjImplicitLod:
            return "ImageSparseSampleProjImplicitLod";
        case SpvOpImageSparseSampleProjExplicitLod:
            return "ImageSparseSampleProjExplicitLod";
        case SpvOpImageSparseSampleProjDrefImplicitLod:
            return "ImageSparseSampleProjDrefImplicitLod";
        case SpvOpImageSparseSampleProjDrefExplicitLod:
            return "ImageSparseSampleProjDrefExplicitLod";
        case SpvOpImageSparseFetch:
            return "ImageSparseFetch";
        case SpvOpImageSparseGather:
            return "ImageSparseGather";
        case SpvOpImageSparseDrefGather:
            return "ImageSparseDrefGather";
        case SpvOpImageSparseTexelsResident:
            return "ImageSparseTexelsResident";
        case SpvOpNoLine:
            return "NoLine";
        case SpvOpAtomicFlagTestAndSet:
            return "AtomicFlagTestAndSet";
        case SpvOpAtomicFlagClear:
            return "AtomicFlagClear";
        case SpvOpImageSparseRead:
            return "ImageSparseRead";
        default:
            ABORT("unsupported SPIR-V op");    
    }
}
#endif

void SPIRVCodeGenerator::writeOpCode(SpvOp_ opCode, int length, std::ostream& out) {
    ASSERT(opCode != SpvOpUndef);
    switch (opCode) {
        case SpvOpReturn:      // fall through
        case SpvOpReturnValue: // fall through
        case SpvOpBranch:      // fall through
        case SpvOpBranchConditional:
            ASSERT(fCurrentBlock);
            fCurrentBlock = 0;
            break;
        case SpvOpConstant:          // fall through
        case SpvOpConstantTrue:      // fall through
        case SpvOpConstantFalse:     // fall through
        case SpvOpConstantComposite: // fall through
        case SpvOpTypeVoid:          // fall through
        case SpvOpTypeInt:           // fall through
        case SpvOpTypeFloat:         // fall through
        case SpvOpTypeBool:          // fall through
        case SpvOpTypeVector:        // fall through
        case SpvOpTypeMatrix:        // fall through
        case SpvOpTypeArray:         // fall through
        case SpvOpTypePointer:       // fall through
        case SpvOpTypeFunction:      // fall through
        case SpvOpTypeRuntimeArray:  // fall through
        case SpvOpTypeStruct:        // fall through
        case SpvOpTypeImage:         // fall through
        case SpvOpTypeSampledImage:  // fall through
        case SpvOpVariable:          // fall through
        case SpvOpFunction:          // fall through
        case SpvOpFunctionParameter: // fall through
        case SpvOpFunctionEnd:       // fall through
        case SpvOpExecutionMode:     // fall through
        case SpvOpMemoryModel:       // fall through
        case SpvOpCapability:        // fall through
        case SpvOpExtInstImport:     // fall through
        case SpvOpEntryPoint:        // fall through
        case SpvOpSource:            // fall through
        case SpvOpSourceExtension:   // fall through
        case SpvOpName:              // fall through
        case SpvOpMemberName:        // fall through
        case SpvOpDecorate:          // fall through
        case SpvOpMemberDecorate:
            break;
        default:
            ASSERT(fCurrentBlock);
    }
#if SPIRV_DEBUG
    out << std::endl << opcode_text(opCode) << " ";
#else
    this->writeWord((length << 16) | opCode, out);
#endif
}

void SPIRVCodeGenerator::writeLabel(SpvId label, std::ostream& out) {
    fCurrentBlock = label;
    this->writeInstruction(SpvOpLabel, label, out);
}

void SPIRVCodeGenerator::writeInstruction(SpvOp_ opCode, std::ostream& out) {
    this->writeOpCode(opCode, 1, out);
}

void SPIRVCodeGenerator::writeInstruction(SpvOp_ opCode, int32_t word1, std::ostream& out) {
    this->writeOpCode(opCode, 2, out);
    this->writeWord(word1, out);
}

void SPIRVCodeGenerator::writeString(const char* string, std::ostream& out) {
    size_t length = strlen(string);
    out << string;
    switch (length % 4) {
        case 1:
            out << (char) 0;
            // fall through
        case 2:
            out << (char) 0;
            // fall through
        case 3:
            out << (char) 0;
            break;
        default:
            this->writeWord(0, out);
    }
}

void SPIRVCodeGenerator::writeInstruction(SpvOp_ opCode, const char* string, std::ostream& out) {
    int32_t length = (int32_t) strlen(string);
    this->writeOpCode(opCode, 1 + (length + 4) / 4, out);
    this->writeString(string, out);
}


void SPIRVCodeGenerator::writeInstruction(SpvOp_ opCode, int32_t word1, const char* string, 
                                          std::ostream& out) {
    int32_t length = (int32_t) strlen(string);
    this->writeOpCode(opCode, 2 + (length + 4) / 4, out);
    this->writeWord(word1, out);
    this->writeString(string, out);
}

void SPIRVCodeGenerator::writeInstruction(SpvOp_ opCode, int32_t word1, int32_t word2, 
                                          const char* string, std::ostream& out) {
    int32_t length = (int32_t) strlen(string);
    this->writeOpCode(opCode, 3 + (length + 4) / 4, out);
    this->writeWord(word1, out);
    this->writeWord(word2, out);
    this->writeString(string, out);
}

void SPIRVCodeGenerator::writeInstruction(SpvOp_ opCode, int32_t word1, int32_t word2, 
                                          std::ostream& out) {
    this->writeOpCode(opCode, 3, out);
    this->writeWord(word1, out);
    this->writeWord(word2, out);
}

void SPIRVCodeGenerator::writeInstruction(SpvOp_ opCode, int32_t word1, int32_t word2, 
                                          int32_t word3, std::ostream& out) {
    this->writeOpCode(opCode, 4, out);
    this->writeWord(word1, out);
    this->writeWord(word2, out);
    this->writeWord(word3, out);
}

void SPIRVCodeGenerator::writeInstruction(SpvOp_ opCode, int32_t word1, int32_t word2, 
                                          int32_t word3, int32_t word4, std::ostream& out) {
    this->writeOpCode(opCode, 5, out);
    this->writeWord(word1, out);
    this->writeWord(word2, out);
    this->writeWord(word3, out);
    this->writeWord(word4, out);
}

void SPIRVCodeGenerator::writeInstruction(SpvOp_ opCode, int32_t word1, int32_t word2, 
                                          int32_t word3, int32_t word4, int32_t word5, 
                                          std::ostream& out) {
    this->writeOpCode(opCode, 6, out);
    this->writeWord(word1, out);
    this->writeWord(word2, out);
    this->writeWord(word3, out);
    this->writeWord(word4, out);
    this->writeWord(word5, out);
}

void SPIRVCodeGenerator::writeInstruction(SpvOp_ opCode, int32_t word1, int32_t word2, 
                                          int32_t word3, int32_t word4, int32_t word5,
                                          int32_t word6, std::ostream& out) {
    this->writeOpCode(opCode, 7, out);
    this->writeWord(word1, out);
    this->writeWord(word2, out);
    this->writeWord(word3, out);
    this->writeWord(word4, out);
    this->writeWord(word5, out);
    this->writeWord(word6, out);
}

void SPIRVCodeGenerator::writeInstruction(SpvOp_ opCode, int32_t word1, int32_t word2, 
                                          int32_t word3, int32_t word4, int32_t word5,
                                          int32_t word6, int32_t word7, std::ostream& out) {
    this->writeOpCode(opCode, 8, out);
    this->writeWord(word1, out);
    this->writeWord(word2, out);
    this->writeWord(word3, out);
    this->writeWord(word4, out);
    this->writeWord(word5, out);
    this->writeWord(word6, out);
    this->writeWord(word7, out);
}

void SPIRVCodeGenerator::writeInstruction(SpvOp_ opCode, int32_t word1, int32_t word2, 
                                          int32_t word3, int32_t word4, int32_t word5,
                                          int32_t word6, int32_t word7, int32_t word8,
                                          std::ostream& out) {
    this->writeOpCode(opCode, 9, out);
    this->writeWord(word1, out);
    this->writeWord(word2, out);
    this->writeWord(word3, out);
    this->writeWord(word4, out);
    this->writeWord(word5, out);
    this->writeWord(word6, out);
    this->writeWord(word7, out);
    this->writeWord(word8, out);
}

void SPIRVCodeGenerator::writeCapabilities(std::ostream& out) {
    for (uint64_t i = 0, bit = 1; i <= kLast_Capability; i++, bit <<= 1) {
        if (fCapabilities & bit) {
            this->writeInstruction(SpvOpCapability, (SpvId) i, out);
        }
    }
}

SpvId SPIRVCodeGenerator::nextId() {
    return fIdCount++;
}

void SPIRVCodeGenerator::writeStruct(const Type& type, SpvId resultId) {
    this->writeInstruction(SpvOpName, resultId, type.name().c_str(), fNameBuffer);
    // go ahead and write all of the field types, so we don't inadvertently write them while we're
    // in the middle of writing the struct instruction
    std::vector<SpvId> types;
    for (const auto& f : type.fields()) {
        types.push_back(this->getType(*f.fType));
    }
    this->writeOpCode(SpvOpTypeStruct, 2 + (int32_t) types.size(), fConstantBuffer);
    this->writeWord(resultId, fConstantBuffer);
    for (SpvId id : types) {
        this->writeWord(id, fConstantBuffer);
    }
    size_t offset = 0;
    for (int32_t i = 0; i < (int32_t) type.fields().size(); i++) {
        size_t size = type.fields()[i].fType->size();
        size_t alignment = type.fields()[i].fType->alignment();
        size_t mod = offset % alignment;
        if (mod != 0) {
            offset += alignment - mod;
        }
        this->writeInstruction(SpvOpMemberName, resultId, i, type.fields()[i].fName.c_str(),
                               fNameBuffer);
        this->writeLayout(type.fields()[i].fModifiers.fLayout, resultId, i);
        if (type.fields()[i].fModifiers.fLayout.fBuiltin < 0) {
            this->writeInstruction(SpvOpMemberDecorate, resultId, (SpvId) i, SpvDecorationOffset, 
                                   (SpvId) offset, fDecorationBuffer);
        }
        if (type.fields()[i].fType->kind() == Type::kMatrix_Kind) {
            this->writeInstruction(SpvOpMemberDecorate, resultId, i, SpvDecorationColMajor, 
                                   fDecorationBuffer);
            this->writeInstruction(SpvOpMemberDecorate, resultId, i, SpvDecorationMatrixStride, 
                                   (SpvId) type.fields()[i].fType->stride(), fDecorationBuffer);
        }
        offset += size;
        Type::Kind kind = type.fields()[i].fType->kind();
        if ((kind == Type::kArray_Kind || kind == Type::kStruct_Kind) && offset % alignment != 0) {
            offset += alignment - offset % alignment;
        }
        ASSERT(offset % alignment == 0);
    }
}

SpvId SPIRVCodeGenerator::getType(const Type& type) {
    auto entry = fTypeMap.find(type.name());
    if (entry == fTypeMap.end()) {
        SpvId result = this->nextId();
        switch (type.kind()) {
            case Type::kScalar_Kind:
                if (type == *kBool_Type) {
                    this->writeInstruction(SpvOpTypeBool, result, fConstantBuffer);
                } else if (type == *kInt_Type) {
                    this->writeInstruction(SpvOpTypeInt, result, 32, 1, fConstantBuffer);
                } else if (type == *kUInt_Type) {
                    this->writeInstruction(SpvOpTypeInt, result, 32, 0, fConstantBuffer);
                } else if (type == *kFloat_Type) {
                    this->writeInstruction(SpvOpTypeFloat, result, 32, fConstantBuffer);
                } else if (type == *kDouble_Type) {
                    this->writeInstruction(SpvOpTypeFloat, result, 64, fConstantBuffer);
                } else {
                    ASSERT(false);
                }
                break;
            case Type::kVector_Kind:
                this->writeInstruction(SpvOpTypeVector, result, 
                                       this->getType(*type.componentType()),
                                       type.columns(), fConstantBuffer);
                break;
            case Type::kMatrix_Kind:
                this->writeInstruction(SpvOpTypeMatrix, result, this->getType(*index_type(type)), 
                                       type.columns(), fConstantBuffer);
                break;
            case Type::kStruct_Kind:
                this->writeStruct(type, result);
                break;
            case Type::kArray_Kind: {
                if (type.columns() > 0) {
                    IntLiteral count(Position(), type.columns());
                    this->writeInstruction(SpvOpTypeArray, result, 
                                           this->getType(*type.componentType()), 
                                           this->writeIntLiteral(count), fConstantBuffer);
                    this->writeInstruction(SpvOpDecorate, result, SpvDecorationArrayStride, 
                                           (int32_t) type.stride(), fDecorationBuffer);
                } else {
                    ABORT("runtime-sized arrays are not yet supported");
                    this->writeInstruction(SpvOpTypeRuntimeArray, result, 
                                           this->getType(*type.componentType()), fConstantBuffer);
                }
                break;
            }
            case Type::kSampler_Kind: {
                SpvId image = this->nextId();
                this->writeInstruction(SpvOpTypeImage, image, this->getType(*kFloat_Type), 
                                       type.dimensions(), type.isDepth(), type.isArrayed(),
                                       type.isMultisampled(), type.isSampled(), 
                                       SpvImageFormatUnknown, fConstantBuffer);
                this->writeInstruction(SpvOpTypeSampledImage, result, image, fConstantBuffer);
                break;
            }
            default:
                if (type == *kVoid_Type) {
                    this->writeInstruction(SpvOpTypeVoid, result, fConstantBuffer);
                } else {
                    ABORT("invalid type: %s", type.description().c_str());
                }
        }
        fTypeMap[type.name()] = result;
        return result;
    }
    return entry->second;
}

SpvId SPIRVCodeGenerator::getFunctionType(std::shared_ptr<FunctionDeclaration> function) {
    std::string key = function->fReturnType->description() + "(";
    std::string separator = "";
    for (size_t i = 0; i < function->fParameters.size(); i++) {
        key += separator;
        separator = ", ";
        key += function->fParameters[i]->fType->description();
    }
    key += ")";
    auto entry = fTypeMap.find(key);
    if (entry == fTypeMap.end()) {
        SpvId result = this->nextId();
        int32_t length = 3 + (int32_t) function->fParameters.size();
        SpvId returnType = this->getType(*function->fReturnType);
        std::vector<SpvId> parameterTypes;
        for (size_t i = 0; i < function->fParameters.size(); i++) {
            // glslang seems to treat all function arguments as pointers whether they need to be or 
            // not. I  was initially puzzled by this until I ran bizarre failures with certain 
            // patterns of function calls and control constructs, as exemplified by this minimal 
            // failure case:
            //
            // void sphere(float x) {
            // }
            // 
            // void map() {
            //     sphere(1.0);
            // }
            // 
            // void main() {
            //     for (int i = 0; i < 1; i++) {
            //         map();
            //     }
            // }
            //
            // As of this writing, compiling this in the "obvious" way (with sphere taking a float) 
            // crashes. Making it take a float* and storing the argument in a temporary variable, 
            // as glslang does, fixes it. It's entirely possible I simply missed whichever part of
            // the spec makes this make sense.
//            if (is_out(function->fParameters[i])) {
                parameterTypes.push_back(this->getPointerType(function->fParameters[i]->fType,
                                                              SpvStorageClassFunction));
//            } else {
//                parameterTypes.push_back(this->getType(*function->fParameters[i]->fType));
//            }
        }
        this->writeOpCode(SpvOpTypeFunction, length, fConstantBuffer);
        this->writeWord(result, fConstantBuffer);
        this->writeWord(returnType, fConstantBuffer);
        for (SpvId id : parameterTypes) {
            this->writeWord(id, fConstantBuffer);
        }
        fTypeMap[key] = result;
        return result;
    }
    return entry->second;
}

SpvId SPIRVCodeGenerator::getPointerType(std::shared_ptr<Type> type, 
                                         SpvStorageClass_ storageClass) {
    std::string key = type->description() + "*" + to_string(storageClass);
    auto entry = fTypeMap.find(key);
    if (entry == fTypeMap.end()) {
        SpvId result = this->nextId();
        this->writeInstruction(SpvOpTypePointer, result, storageClass, 
                               this->getType(*type), fConstantBuffer);
        fTypeMap[key] = result;
        return result;
    }
    return entry->second;
}

SpvId SPIRVCodeGenerator::writeExpression(Expression& expr, std::ostream& out) {
    switch (expr.fKind) {
        case Expression::kBinary_Kind:
            return this->writeBinaryExpression((BinaryExpression&) expr, out);
        case Expression::kBoolLiteral_Kind:
            return this->writeBoolLiteral((BoolLiteral&) expr);
        case Expression::kConstructor_Kind:
            return this->writeConstructor((Constructor&) expr, out);
        case Expression::kIntLiteral_Kind:
            return this->writeIntLiteral((IntLiteral&) expr);
        case Expression::kFieldAccess_Kind:
            return this->writeFieldAccess(((FieldAccess&) expr), out);
        case Expression::kFloatLiteral_Kind:
            return this->writeFloatLiteral(((FloatLiteral&) expr));
        case Expression::kFunctionCall_Kind:
            return this->writeFunctionCall((FunctionCall&) expr, out);
        case Expression::kPrefix_Kind:
            return this->writePrefixExpression((PrefixExpression&) expr, out);
        case Expression::kPostfix_Kind:
            return this->writePostfixExpression((PostfixExpression&) expr, out);
        case Expression::kSwizzle_Kind:
            return this->writeSwizzle((Swizzle&) expr, out);
        case Expression::kVariableReference_Kind:
            return this->writeVariableReference((VariableReference&) expr, out);
        case Expression::kTernary_Kind:
            return this->writeTernaryExpression((TernaryExpression&) expr, out);
        case Expression::kIndex_Kind:
            return this->writeIndexExpression((IndexExpression&) expr, out);
        default:
            ABORT("unsupported expression: %s", expr.description().c_str());
    }
    return -1;
}

SpvId SPIRVCodeGenerator::writeIntrinsicCall(FunctionCall& c, std::ostream& out) {
    auto intrinsic = fIntrinsicMap.find(c.fFunction->fName);
    ASSERT(intrinsic != fIntrinsicMap.end());
    std::shared_ptr<Type> type = c.fArguments[0]->fType;
    int32_t intrinsicId;
    if (std::get<0>(intrinsic->second) == kSpecial_IntrinsicKind || is_float(*type)) {
        intrinsicId = std::get<1>(intrinsic->second);
    } else if (is_signed(*type)) {
        intrinsicId = std::get<2>(intrinsic->second);
    } else if (is_unsigned(*type)) {
        intrinsicId = std::get<3>(intrinsic->second);
    } else if (is_bool(*type)) {
        intrinsicId = std::get<4>(intrinsic->second);
    } else {
        ABORT("invalid call %s, cannot operate on '%s'", c.description().c_str(),
              type->description().c_str());
    }
    switch (std::get<0>(intrinsic->second)) {
        case kGLSL_STD_450_IntrinsicKind: {
            SpvId result = this->nextId();
            std::vector<SpvId> arguments;
            for (size_t i = 0; i < c.fArguments.size(); i++) {
                arguments.push_back(this->writeExpression(*c.fArguments[i], out));
            }
            this->writeOpCode(SpvOpExtInst, 5 + (int32_t) arguments.size(), out);
            this->writeWord(this->getType(*c.fType), out);
            this->writeWord(result, out);
            this->writeWord(fGLSLExtendedInstructions, out);
            this->writeWord(intrinsicId, out);
            for (SpvId id : arguments) {
                this->writeWord(id, out);
            }
            return result;
        }
        case kSPIRV_IntrinsicKind: {
            SpvId result = this->nextId();
            std::vector<SpvId> arguments;
            for (size_t i = 0; i < c.fArguments.size(); i++) {
                arguments.push_back(this->writeExpression(*c.fArguments[i], out));
            }
            this->writeOpCode((SpvOp_) intrinsicId, 3 + (int32_t) arguments.size(), out);
            this->writeWord(this->getType(*c.fType), out);
            this->writeWord(result, out);
            for (SpvId id : arguments) {
                this->writeWord(id, out);
            }
            return result;
        }
        case kSpecial_IntrinsicKind:
            return this->writeSpecialIntrinsic(c, (SpecialIntrinsic) intrinsicId, out);
        default:
            ABORT("unsupported intrinsic kind");
    }
}

SpvId SPIRVCodeGenerator::writeSpecialIntrinsic(FunctionCall& c, SpecialIntrinsic kind, 
                                                std::ostream& out) {
    SpvId result = this->nextId();
    switch (kind) {
        case kAtan_SpecialIntrinsic: {
            std::vector<SpvId> arguments;
            for (size_t i = 0; i < c.fArguments.size(); i++) {
                arguments.push_back(this->writeExpression(*c.fArguments[i], out));
            }
            this->writeOpCode(SpvOpExtInst, 5 + (int32_t) arguments.size(), out);
            this->writeWord(this->getType(*c.fType), out);
            this->writeWord(result, out);
            this->writeWord(fGLSLExtendedInstructions, out);
            this->writeWord(arguments.size() == 2 ? GLSLstd450Atan2 : GLSLstd450Atan, out);
            for (SpvId id : arguments) {
                this->writeWord(id, out);
            }
            return result;            
        }
        case kTexture_SpecialIntrinsic: {
            SpvId type = this->getType(*c.fType);
            SpvId sampler = this->writeExpression(*c.fArguments[0], out);
            SpvId uv = this->writeExpression(*c.fArguments[1], out);
            if (c.fArguments.size() == 3) {
                this->writeInstruction(SpvOpImageSampleImplicitLod, type, result, sampler, uv,
                                       SpvImageOperandsBiasMask,
                                       this->writeExpression(*c.fArguments[2], out),
                                       out);
            } else {
                ASSERT(c.fArguments.size() == 2);
                this->writeInstruction(SpvOpImageSampleImplicitLod, type, result, sampler, uv, out);
            }
            break;
        }
        case kTextureProj_SpecialIntrinsic: {
            SpvId type = this->getType(*c.fType);
            SpvId sampler = this->writeExpression(*c.fArguments[0], out);
            SpvId uv = this->writeExpression(*c.fArguments[1], out);
            if (c.fArguments.size() == 3) {
                this->writeInstruction(SpvOpImageSampleProjImplicitLod, type, result, sampler, uv,
                                       SpvImageOperandsBiasMask,
                                       this->writeExpression(*c.fArguments[2], out),
                                       out);
            } else {
                ASSERT(c.fArguments.size() == 2);
                this->writeInstruction(SpvOpImageSampleProjImplicitLod, type, result, sampler, uv, 
                                       out);
            }
            break;
        }
        case kTexture2D_SpecialIntrinsic: {
            SpvId img = this->writeExpression(*c.fArguments[0], out);
            SpvId coords = this->writeExpression(*c.fArguments[1], out);
            this->writeInstruction(SpvOpImageSampleImplicitLod,
                                   this->getType(*c.fType),
                                   result, 
                                   img,
                                   coords,
                                   out);
            break;
        }
    }
    return result;
}

SpvId SPIRVCodeGenerator::writeFunctionCall(FunctionCall& c, std::ostream& out) {
    const auto& entry = fFunctionMap.find(c.fFunction);
    if (entry == fFunctionMap.end()) {
        return this->writeIntrinsicCall(c, out);
    }
    // stores (variable, type, lvalue) pairs to extract and save after the function call is complete
    std::vector<std::tuple<SpvId, SpvId, std::unique_ptr<LValue>>> lvalues;
    std::vector<SpvId> arguments;
    for (size_t i = 0; i < c.fArguments.size(); i++) {
        // id of temporary variable that we will use to hold this argument, or 0 if it is being 
        // passed directly
        SpvId tmpVar;
        // if we need a temporary var to store this argument, this is the value to store in the var
        SpvId tmpValueId;
        if (is_out(c.fFunction->fParameters[i])) {
            std::unique_ptr<LValue> lv = this->getLValue(*c.fArguments[i], out);
            SpvId ptr = lv->getPointer();
            if (ptr) {
                arguments.push_back(ptr);
                continue;
            } else {
                // lvalue cannot simply be read and written via a pointer (e.g. a swizzle). Need to
                // copy it into a temp, call the function, read the value out of the temp, and then
                // update the lvalue.
                tmpValueId = lv->load(out);
                tmpVar = this->nextId();
                lvalues.push_back(std::make_tuple(tmpVar, this->getType(*c.fArguments[i]->fType),
                                  std::move(lv)));
            }
        } else {
            // see getFunctionType for an explanation of why we're always using pointer parameters
            tmpValueId = this->writeExpression(*c.fArguments[i], out);
            tmpVar = this->nextId();
        }
        this->writeInstruction(SpvOpVariable, 
                               this->getPointerType(c.fArguments[i]->fType, 
                                                    SpvStorageClassFunction),
                               tmpVar, 
                               SpvStorageClassFunction,
                               out);
        this->writeInstruction(SpvOpStore, tmpVar, tmpValueId, out);
        arguments.push_back(tmpVar);
    }
    SpvId result = this->nextId();
    this->writeOpCode(SpvOpFunctionCall, 4 + (int32_t) c.fArguments.size(), out);
    this->writeWord(this->getType(*c.fType), out);
    this->writeWord(result, out);
    this->writeWord(entry->second, out);
    for (SpvId id : arguments) {
        this->writeWord(id, out);
    }
    // now that the call is complete, we may need to update some lvalues with the new values of out
    // arguments
    for (const auto& tuple : lvalues) {
        SpvId load = this->nextId();
        this->writeInstruction(SpvOpLoad, std::get<1>(tuple), load, std::get<0>(tuple), out);
        std::get<2>(tuple)->store(load, out);
    }
    return result;
}

SpvId SPIRVCodeGenerator::writeConstantVector(Constructor& c) {
    ASSERT(c.fType->kind() == Type::kVector_Kind && c.isConstant());
    SpvId result = this->nextId();
    std::vector<SpvId> arguments;
    for (size_t i = 0; i < c.fArguments.size(); i++) {
        arguments.push_back(this->writeExpression(*c.fArguments[i], fConstantBuffer));
    }
    SpvId type = this->getType(*c.fType);
    if (c.fArguments.size() == 1) {
        // with a single argument, a vector will have all of its entries equal to the argument
        this->writeOpCode(SpvOpConstantComposite, 3 + c.fType->columns(), fConstantBuffer);
        this->writeWord(type, fConstantBuffer);
        this->writeWord(result, fConstantBuffer);
        for (int i = 0; i < c.fType->columns(); i++) {
            this->writeWord(arguments[0], fConstantBuffer);
        }
    } else {
        this->writeOpCode(SpvOpConstantComposite, 3 + (int32_t) c.fArguments.size(), 
                          fConstantBuffer);
        this->writeWord(type, fConstantBuffer);
        this->writeWord(result, fConstantBuffer);
        for (SpvId id : arguments) {
            this->writeWord(id, fConstantBuffer);
        }
    }
    return result;
}

SpvId SPIRVCodeGenerator::writeFloatConstructor(Constructor& c, std::ostream& out) {
    ASSERT(c.fType == kFloat_Type);
    ASSERT(c.fArguments.size() == 1);
    ASSERT(c.fArguments[0]->fType->isNumber());
    SpvId result = this->nextId();
    SpvId parameter = this->writeExpression(*c.fArguments[0], out);
    if (c.fArguments[0]->fType == kInt_Type) {
        this->writeInstruction(SpvOpConvertSToF, this->getType(*c.fType), result, parameter, 
                               out);
    } else if (c.fArguments[0]->fType == kUInt_Type) {
        this->writeInstruction(SpvOpConvertUToF, this->getType(*c.fType), result, parameter, 
                               out);
    } else if (c.fArguments[0]->fType == kFloat_Type) {
        return parameter;
    }
    return result;
}

SpvId SPIRVCodeGenerator::writeIntConstructor(Constructor& c, std::ostream& out) {
    ASSERT(c.fType == kInt_Type);
    ASSERT(c.fArguments.size() == 1);
    ASSERT(c.fArguments[0]->fType->isNumber());
    SpvId result = this->nextId();
    SpvId parameter = this->writeExpression(*c.fArguments[0], out);
    if (c.fArguments[0]->fType == kFloat_Type) {
        this->writeInstruction(SpvOpConvertFToS, this->getType(*c.fType), result, parameter, 
                               out);
    } else if (c.fArguments[0]->fType == kUInt_Type) {
        this->writeInstruction(SpvOpSatConvertUToS, this->getType(*c.fType), result, parameter, 
                               out);
    } else if (c.fArguments[0]->fType == kInt_Type) {
        return parameter;
    }
    return result;
}

SpvId SPIRVCodeGenerator::writeMatrixConstructor(Constructor& c, std::ostream& out) {
    ASSERT(c.fType->kind() == Type::kMatrix_Kind);
    // go ahead and write the arguments so we don't try to write new instructions in the middle of
    // an instruction
    std::vector<SpvId> arguments;
    for (size_t i = 0; i < c.fArguments.size(); i++) {
        arguments.push_back(this->writeExpression(*c.fArguments[i], out));
    }
    SpvId result = this->nextId();
    int rows = c.fType->rows();
    int columns = c.fType->columns();
    // FIXME this won't work to create a matrix from another matrix
    if (arguments.size() == 1) {
        // with a single argument, a matrix will have all of its diagonal entries equal to the 
        // argument and its other values equal to zero
        // FIXME this won't work for int matrices
        FloatLiteral zero(Position(), 0);
        SpvId zeroId = this->writeFloatLiteral(zero);
        std::vector<SpvId> columnIds;
        for (int column = 0; column < columns; column++) {
            this->writeOpCode(SpvOpCompositeConstruct, 3 + c.fType->rows(), 
                              out);
            this->writeWord(this->getType(*c.fType->componentType()->toCompound(rows, 1)), out);
            SpvId columnId = this->nextId();
            this->writeWord(columnId, out);
            columnIds.push_back(columnId);
            for (int row = 0; row < c.fType->columns(); row++) {
                this->writeWord(row == column ? arguments[0] : zeroId, out);
            }
        }
        this->writeOpCode(SpvOpCompositeConstruct, 3 + columns, 
                          out);
        this->writeWord(this->getType(*c.fType), out);
        this->writeWord(result, out);
        for (SpvId id : columnIds) {
            this->writeWord(id, out);
        }
    } else {
        std::vector<SpvId> columnIds;
        int currentCount = 0;
        for (size_t i = 0; i < arguments.size(); i++) {
            if (c.fArguments[i]->fType->kind() == Type::kVector_Kind) {
                ASSERT(currentCount == 0);
                columnIds.push_back(arguments[i]);
                currentCount = 0;
            } else {
                ASSERT(c.fArguments[i]->fType->kind() == Type::kScalar_Kind);
                if (currentCount == 0) {
                    this->writeOpCode(SpvOpCompositeConstruct, 3 + c.fType->rows(), out);
                    this->writeWord(this->getType(*c.fType->componentType()->toCompound(rows, 1)), 
                                    out);
                    SpvId id = this->nextId();
                    this->writeWord(id, out);
                    columnIds.push_back(id);
                }
                this->writeWord(arguments[i], out);
                currentCount = (currentCount + 1) % rows;
            }
        }
        ASSERT(columnIds.size() == (size_t) columns);
        this->writeOpCode(SpvOpCompositeConstruct, 3 + columns, out);
        this->writeWord(this->getType(*c.fType), out);
        this->writeWord(result, out);
        for (SpvId id : columnIds) {
            this->writeWord(id, out);
        }
    }
    return result;
}

SpvId SPIRVCodeGenerator::writeVectorConstructor(Constructor& c, std::ostream& out) {
    ASSERT(c.fType->kind() == Type::kVector_Kind);
    if (c.isConstant()) {
        return this->writeConstantVector(c);
    }
    // go ahead and write the arguments so we don't try to write new instructions in the middle of
    // an instruction
    std::vector<SpvId> arguments;
    for (size_t i = 0; i < c.fArguments.size(); i++) {
        arguments.push_back(this->writeExpression(*c.fArguments[i], out));
    }
    SpvId result = this->nextId();
    if (arguments.size() == 1 && c.fArguments[0]->fType->kind() == Type::kScalar_Kind) {
        this->writeOpCode(SpvOpCompositeConstruct, 3 + c.fType->columns(), out);
        this->writeWord(this->getType(*c.fType), out);
        this->writeWord(result, out);
        for (int i = 0; i < c.fType->columns(); i++) {
            this->writeWord(arguments[0], out);
        }
    } else {
        this->writeOpCode(SpvOpCompositeConstruct, 3 + (int32_t) c.fArguments.size(), out);
        this->writeWord(this->getType(*c.fType), out);
        this->writeWord(result, out);
        for (SpvId id : arguments) {
            this->writeWord(id, out);
        }
    }
    return result;
}

SpvId SPIRVCodeGenerator::writeConstructor(Constructor& c, std::ostream& out) {
    if (c.fType == kFloat_Type) {
        return this->writeFloatConstructor(c, out);
    } else if (c.fType == kInt_Type) {
        return this->writeIntConstructor(c, out);
    }
    switch (c.fType->kind()) {
        case Type::kVector_Kind:
            return this->writeVectorConstructor(c, out);
        case Type::kMatrix_Kind:
            return this->writeMatrixConstructor(c, out);
        default:
            ABORT("unsupported constructor: %s", c.description().c_str());
    }
}

SpvStorageClass_ get_storage_class(const Modifiers& modifiers) {
    if (modifiers.fFlags & Modifiers::kIn_Flag) {
        return SpvStorageClassInput;
    } else if (modifiers.fFlags & Modifiers::kOut_Flag) {
        return SpvStorageClassOutput;
    } else if (modifiers.fFlags & Modifiers::kUniform_Flag) {
        return SpvStorageClassUniform;
    } else {
        return SpvStorageClassFunction;
    }
}

SpvStorageClass_ get_storage_class(Expression& expr) {
    switch (expr.fKind) {
        case Expression::kVariableReference_Kind:
            return get_storage_class(((VariableReference&) expr).fVariable->fModifiers);
        case Expression::kFieldAccess_Kind:
            return get_storage_class(*((FieldAccess&) expr).fBase);
        case Expression::kIndex_Kind:
            return get_storage_class(*((IndexExpression&) expr).fBase);
        default:
            return SpvStorageClassFunction;
    }
}

std::vector<SpvId> SPIRVCodeGenerator::getAccessChain(Expression& expr, std::ostream& out) {
    std::vector<SpvId> chain;
    switch (expr.fKind) {
        case Expression::kIndex_Kind: {
            IndexExpression& indexExpr = (IndexExpression&) expr;
            chain = this->getAccessChain(*indexExpr.fBase, out);
            chain.push_back(this->writeExpression(*indexExpr.fIndex, out));
            break;
        }
        case Expression::kFieldAccess_Kind: {
            FieldAccess& fieldExpr = (FieldAccess&) expr;
            chain = this->getAccessChain(*fieldExpr.fBase, out);
            IntLiteral index(Position(), fieldExpr.fFieldIndex);
            chain.push_back(this->writeIntLiteral(index));
            break;
        }
        default:
            chain.push_back(this->getLValue(expr, out)->getPointer());
    }
    return chain;
}

class PointerLValue : public SPIRVCodeGenerator::LValue {
public:
    PointerLValue(SPIRVCodeGenerator& gen, SpvId pointer, SpvId type) 
    : fGen(gen)
    , fPointer(pointer)
    , fType(type) {}

    virtual SpvId getPointer() override {
        return fPointer;
    }

    virtual SpvId load(std::ostream& out) override {
        SpvId result = fGen.nextId();
        fGen.writeInstruction(SpvOpLoad, fType, result, fPointer, out);
        return result;
    }

    virtual void store(SpvId value, std::ostream& out) override {
        fGen.writeInstruction(SpvOpStore, fPointer, value, out);
    }

private:
    SPIRVCodeGenerator& fGen;
    const SpvId fPointer;
    const SpvId fType;
};

class SwizzleLValue : public SPIRVCodeGenerator::LValue {
public:
    SwizzleLValue(SPIRVCodeGenerator& gen, SpvId vecPointer, const std::vector<int>& components, 
                  const Type& baseType, const Type& swizzleType)
    : fGen(gen)
    , fVecPointer(vecPointer)
    , fComponents(components)
    , fBaseType(baseType)
    , fSwizzleType(swizzleType) {}

    virtual SpvId getPointer() override {
        return 0;
    }

    virtual SpvId load(std::ostream& out) override {
        SpvId base = fGen.nextId();
        fGen.writeInstruction(SpvOpLoad, fGen.getType(fBaseType), base, fVecPointer, out);
        SpvId result = fGen.nextId();
        fGen.writeOpCode(SpvOpVectorShuffle, 5 + (int32_t) fComponents.size(), out);
        fGen.writeWord(fGen.getType(fSwizzleType), out);
        fGen.writeWord(result, out);
        fGen.writeWord(base, out);
        fGen.writeWord(base, out);
        for (int component : fComponents) {
            fGen.writeWord(component, out);
        }
        return result;
    }

    virtual void store(SpvId value, std::ostream& out) override {
        // use OpVectorShuffle to mix and match the vector components. We effectively create
        // a virtual vector out of the concatenation of the left and right vectors, and then
        // select components from this virtual vector to make the result vector. For 
        // instance, given:
        // vec3 L = ...;
        // vec3 R = ...;
        // L.xz = R.xy;
        // we end up with the virtual vector (L.x, L.y, L.z, R.x, R.y, R.z). Then we want 
        // our result vector to look like (R.x, L.y, R.y), so we need to select indices
        // (3, 1, 4).
        SpvId base = fGen.nextId();
        fGen.writeInstruction(SpvOpLoad, fGen.getType(fBaseType), base, fVecPointer, out);
        SpvId shuffle = fGen.nextId();
        fGen.writeOpCode(SpvOpVectorShuffle, 5 + fBaseType.columns(), out);
        fGen.writeWord(fGen.getType(fBaseType), out);
        fGen.writeWord(shuffle, out);
        fGen.writeWord(base, out);
        fGen.writeWord(value, out);
        for (int i = 0; i < fBaseType.columns(); i++) {
            // current offset into the virtual vector, defaults to pulling the unmodified
            // value from the left side
            int offset = i;
            // check to see if we are writing this component
            for (size_t j = 0; j < fComponents.size(); j++) {
                if (fComponents[j] == i) {
                    // we're writing to this component, so adjust the offset to pull from 
                    // the correct component of the right side instead of preserving the
                    // value from the left
                    offset = (int) (j + fBaseType.columns());
                    break;
                }
            }
            fGen.writeWord(offset, out);
        }
        fGen.writeInstruction(SpvOpStore, fVecPointer, shuffle, out);
    }

private:
    SPIRVCodeGenerator& fGen;
    const SpvId fVecPointer;
    const std::vector<int>& fComponents;
    const Type& fBaseType;
    const Type& fSwizzleType;
};

std::unique_ptr<SPIRVCodeGenerator::LValue> SPIRVCodeGenerator::getLValue(Expression& expr, 
                                                                          std::ostream& out) {
    switch (expr.fKind) {
        case Expression::kVariableReference_Kind: {
            std::shared_ptr<Variable> var = ((VariableReference&) expr).fVariable;
            auto entry = fVariableMap.find(var);
            ASSERT(entry != fVariableMap.end());
            return std::unique_ptr<SPIRVCodeGenerator::LValue>(new PointerLValue(
                                                                       *this,
                                                                       entry->second, 
                                                                       this->getType(*expr.fType)));
        }
        case Expression::kIndex_Kind: // fall through
        case Expression::kFieldAccess_Kind: {
            std::vector<SpvId> chain = this->getAccessChain(expr, out);
            SpvId member = this->nextId();
            this->writeOpCode(SpvOpAccessChain, (SpvId) (3 + chain.size()), out);
            this->writeWord(this->getPointerType(expr.fType, get_storage_class(expr)), out); 
            this->writeWord(member, out);
            for (SpvId idx : chain) {
                this->writeWord(idx, out);
            }
            return std::unique_ptr<SPIRVCodeGenerator::LValue>(new PointerLValue(
                                                                       *this,
                                                                       member, 
                                                                       this->getType(*expr.fType)));
        }

        case Expression::kSwizzle_Kind: {
            Swizzle& swizzle = (Swizzle&) expr;
            size_t count = swizzle.fComponents.size();
            SpvId base = this->getLValue(*swizzle.fBase, out)->getPointer();
            ASSERT(base);
            if (count == 1) {
                IntLiteral index(Position(), swizzle.fComponents[0]);
                SpvId member = this->nextId();
                this->writeInstruction(SpvOpAccessChain,
                                       this->getPointerType(swizzle.fType, 
                                                            get_storage_class(*swizzle.fBase)), 
                                       member, 
                                       base, 
                                       this->writeIntLiteral(index), 
                                       out);
                return std::unique_ptr<SPIRVCodeGenerator::LValue>(new PointerLValue(
                                                                       *this,
                                                                       member, 
                                                                       this->getType(*expr.fType)));
            } else {
                return std::unique_ptr<SPIRVCodeGenerator::LValue>(new SwizzleLValue(
                                                                              *this, 
                                                                              base, 
                                                                              swizzle.fComponents, 
                                                                              *swizzle.fBase->fType,
                                                                              *expr.fType));
            }
        }

        default:
            // expr isn't actually an lvalue, create a dummy variable for it. This case happens due
            // to the need to store values in temporary variables during function calls (see 
            // comments in getFunctionType); erroneous uses of rvalues as lvalues should have been
            // caught by IRGenerator
            SpvId result = this->nextId();
            SpvId type = this->getPointerType(expr.fType, SpvStorageClassFunction);
            this->writeInstruction(SpvOpVariable, type, result, SpvStorageClassFunction, out);
            this->writeInstruction(SpvOpStore, result, this->writeExpression(expr, out), out);
            return std::unique_ptr<SPIRVCodeGenerator::LValue>(new PointerLValue(
                                                                       *this,
                                                                       result, 
                                                                       this->getType(*expr.fType)));
    }
}

SpvId SPIRVCodeGenerator::writeVariableReference(VariableReference& ref, std::ostream& out) {
    auto entry = fVariableMap.find(ref.fVariable);
    ASSERT(entry != fVariableMap.end());
    SpvId var = entry->second;
    SpvId result = this->nextId();
    this->writeInstruction(SpvOpLoad, this->getType(*ref.fVariable->fType), result, var, out);
    return result;
}

SpvId SPIRVCodeGenerator::writeIndexExpression(IndexExpression& expr, std::ostream& out) {
    return getLValue(expr, out)->load(out);
}

SpvId SPIRVCodeGenerator::writeFieldAccess(FieldAccess& f, std::ostream& out) {
    return getLValue(f, out)->load(out);
}

SpvId SPIRVCodeGenerator::writeSwizzle(Swizzle& swizzle, std::ostream& out) {
    SpvId base = this->writeExpression(*swizzle.fBase, out);
    SpvId result = this->nextId();
    size_t count = swizzle.fComponents.size();
    if (count == 1) {
        this->writeInstruction(SpvOpCompositeExtract, this->getType(*swizzle.fType), result, base, 
                               swizzle.fComponents[0], out); 
    } else {
        this->writeOpCode(SpvOpVectorShuffle, 5 + (int32_t) count, out);
        this->writeWord(this->getType(*swizzle.fType), out);
        this->writeWord(result, out);
        this->writeWord(base, out);
        this->writeWord(base, out);
        for (int component : swizzle.fComponents) {
            this->writeWord(component, out);
        }
    }
    return result;
}

SpvId SPIRVCodeGenerator::writeBinaryOperation(const Type& resultType, 
                                               const Type& operandType, SpvId lhs, 
                                               SpvId rhs, SpvOp_ ifFloat, SpvOp_ ifInt, 
                                               SpvOp_ ifUInt, SpvOp_ ifBool, std::ostream& out) {
    SpvId result = this->nextId();
    if (is_float(operandType)) {
        this->writeInstruction(ifFloat, this->getType(resultType), result, lhs, rhs, out);
    } else if (is_signed(operandType)) {
        this->writeInstruction(ifInt, this->getType(resultType), result, lhs, rhs, out);
    } else if (is_unsigned(operandType)) {
        this->writeInstruction(ifUInt, this->getType(resultType), result, lhs, rhs, out);
    } else if (operandType == *kBool_Type) {
        this->writeInstruction(ifBool, this->getType(resultType), result, lhs, rhs, out);
    } else {
        ABORT("invalid operandType: %s", operandType.description().c_str());
    }
    return result;
}

bool is_assignment(Token::Kind op) {
    switch (op) {
        case Token::EQ:           // fall through
        case Token::PLUSEQ:       // fall through
        case Token::MINUSEQ:      // fall through
        case Token::STAREQ:       // fall through
        case Token::SLASHEQ:      // fall through
        case Token::PERCENTEQ:    // fall through
        case Token::SHLEQ:        // fall through
        case Token::SHREQ:        // fall through
        case Token::BITWISEOREQ:  // fall through
        case Token::BITWISEXOREQ: // fall through
        case Token::BITWISEANDEQ: // fall through
        case Token::LOGICALOREQ:  // fall through
        case Token::LOGICALXOREQ: // fall through
        case Token::LOGICALANDEQ:
            return true;
        default:
            return false;
    }
}

SpvId SPIRVCodeGenerator::writeBinaryExpression(BinaryExpression& b, std::ostream& out) {
    // handle cases where we don't necessarily evaluate both LHS and RHS
    switch (b.fOperator) {
        case Token::EQ: {
            SpvId rhs = this->writeExpression(*b.fRight, out);
            this->getLValue(*b.fLeft, out)->store(rhs, out);
            return rhs;
        }
        case Token::LOGICALAND:
            return this->writeLogicalAnd(b, out);
        case Token::LOGICALOR:
            return this->writeLogicalOr(b, out);
        default:
            break;
    }

    // "normal" operators
    const Type& resultType = *b.fType;
    std::unique_ptr<LValue> lvalue;
    SpvId lhs;
    if (is_assignment(b.fOperator)) {
        lvalue = this->getLValue(*b.fLeft, out);
        lhs = lvalue->load(out);
    } else {
        lvalue = nullptr;
        lhs = this->writeExpression(*b.fLeft, out);
    }
    SpvId rhs = this->writeExpression(*b.fRight, out);
    // component type we are operating on: float, int, uint
    const Type* operandType;
    // IR allows mismatched types in expressions (e.g. vec2 * float), but they need special handling
    // in SPIR-V
    if (b.fLeft->fType != b.fRight->fType) {
        if (b.fLeft->fType->kind() == Type::kVector_Kind && 
            b.fRight->fType->isNumber()) {
            // promote number to vector
            SpvId vec = this->nextId();
            this->writeOpCode(SpvOpCompositeConstruct, 3 + b.fType->columns(), out);
            this->writeWord(this->getType(resultType), out);
            this->writeWord(vec, out);
            for (int i = 0; i < resultType.columns(); i++) {
                this->writeWord(rhs, out);
            }
            rhs = vec;
            operandType = b.fRight->fType.get();
        } else if (b.fRight->fType->kind() == Type::kVector_Kind && 
                   b.fLeft->fType->isNumber()) {
            // promote number to vector
            SpvId vec = this->nextId();
            this->writeOpCode(SpvOpCompositeConstruct, 3 + b.fType->columns(), out);
            this->writeWord(this->getType(resultType), out);
            this->writeWord(vec, out);
            for (int i = 0; i < resultType.columns(); i++) {
                this->writeWord(lhs, out);
            }
            lhs = vec;
            ASSERT(!lvalue);
            operandType = b.fLeft->fType.get();
        } else if (b.fLeft->fType->kind() == Type::kMatrix_Kind) {
            SpvOp_ op;
            if (b.fRight->fType->kind() == Type::kMatrix_Kind) {
                op = SpvOpMatrixTimesMatrix;
            } else if (b.fRight->fType->kind() == Type::kVector_Kind) {
                op = SpvOpMatrixTimesVector;
            } else {
                ASSERT(b.fRight->fType->kind() == Type::kScalar_Kind);
                op = SpvOpMatrixTimesScalar;
            }
            SpvId result = this->nextId();
            this->writeInstruction(op, this->getType(*b.fType), result, lhs, rhs, out);
            if (b.fOperator == Token::STAREQ) {
                lvalue->store(result, out);
            } else {
                ASSERT(b.fOperator == Token::STAR);
            }
            return result;
        } else if (b.fRight->fType->kind() == Type::kMatrix_Kind) {
            SpvId result = this->nextId();
            if (b.fLeft->fType->kind() == Type::kVector_Kind) {
                this->writeInstruction(SpvOpVectorTimesMatrix, this->getType(*b.fType), result, 
                                       lhs, rhs, out);
            } else {
                ASSERT(b.fLeft->fType->kind() == Type::kScalar_Kind);
                this->writeInstruction(SpvOpMatrixTimesScalar, this->getType(*b.fType), result, rhs, 
                                       lhs, out);
            }
            if (b.fOperator == Token::STAREQ) {
                lvalue->store(result, out);
            } else {
                ASSERT(b.fOperator == Token::STAR);
            }
            return result;
        } else {
            ABORT("unsupported binary expression: %s", b.description().c_str());
        }
    } else {
        operandType = b.fLeft->fType.get();
        ASSERT(*operandType == *b.fRight->fType);
    }
    switch (b.fOperator) {
        case Token::EQEQ:
            ASSERT(resultType == *kBool_Type);
            return this->writeBinaryOperation(resultType, *operandType, lhs, rhs, SpvOpFOrdEqual, 
                                              SpvOpIEqual, SpvOpIEqual, SpvOpLogicalEqual, out);
        case Token::NEQ:
            ASSERT(resultType == *kBool_Type);
            return this->writeBinaryOperation(resultType, *operandType, lhs, rhs, SpvOpFOrdNotEqual, 
                                              SpvOpINotEqual, SpvOpINotEqual, SpvOpLogicalNotEqual, 
                                              out);
        case Token::GT:
            ASSERT(resultType == *kBool_Type);
            return this->writeBinaryOperation(resultType, *operandType, lhs, rhs, 
                                              SpvOpFOrdGreaterThan, SpvOpSGreaterThan, 
                                              SpvOpUGreaterThan, SpvOpUndef, out);
        case Token::LT:
            ASSERT(resultType == *kBool_Type);
            return this->writeBinaryOperation(resultType, *operandType, lhs, rhs, SpvOpFOrdLessThan, 
                                              SpvOpSLessThan, SpvOpULessThan, SpvOpUndef, out);
        case Token::GTEQ:
            ASSERT(resultType == *kBool_Type);
            return this->writeBinaryOperation(resultType, *operandType, lhs, rhs, 
                                              SpvOpFOrdGreaterThanEqual, SpvOpSGreaterThanEqual, 
                                              SpvOpUGreaterThanEqual, SpvOpUndef, out);
        case Token::LTEQ:
            ASSERT(resultType == *kBool_Type);
            return this->writeBinaryOperation(resultType, *operandType, lhs, rhs, 
                                              SpvOpFOrdLessThanEqual, SpvOpSLessThanEqual, 
                                              SpvOpULessThanEqual, SpvOpUndef, out);
        case Token::PLUS:
            return this->writeBinaryOperation(resultType, *operandType, lhs, rhs, SpvOpFAdd, 
                                              SpvOpIAdd, SpvOpIAdd, SpvOpUndef, out);
        case Token::MINUS:
            return this->writeBinaryOperation(resultType, *operandType, lhs, rhs, SpvOpFSub, 
                                              SpvOpISub, SpvOpISub, SpvOpUndef, out);
        case Token::STAR:
            if (b.fLeft->fType->kind() == Type::kMatrix_Kind && 
                b.fRight->fType->kind() == Type::kMatrix_Kind) {
                // matrix multiply
                SpvId result = this->nextId();
                this->writeInstruction(SpvOpMatrixTimesMatrix, this->getType(resultType), result,
                                       lhs, rhs, out);
                return result;
            }
            return this->writeBinaryOperation(resultType, *operandType, lhs, rhs, SpvOpFMul, 
                                              SpvOpIMul, SpvOpIMul, SpvOpUndef, out);
        case Token::SLASH:
            return this->writeBinaryOperation(resultType, *operandType, lhs, rhs, SpvOpFDiv, 
                                              SpvOpSDiv, SpvOpUDiv, SpvOpUndef, out);
        case Token::PLUSEQ: {
            SpvId result = this->writeBinaryOperation(resultType, *operandType, lhs, rhs, SpvOpFAdd, 
                                                      SpvOpIAdd, SpvOpIAdd, SpvOpUndef, out);
            ASSERT(lvalue);
            lvalue->store(result, out);
            return result;
        }
        case Token::MINUSEQ: {
            SpvId result = this->writeBinaryOperation(resultType, *operandType, lhs, rhs, SpvOpFSub, 
                                                      SpvOpISub, SpvOpISub, SpvOpUndef, out);
            ASSERT(lvalue);
            lvalue->store(result, out);
            return result;
        }
        case Token::STAREQ: {
            if (b.fLeft->fType->kind() == Type::kMatrix_Kind && 
                b.fRight->fType->kind() == Type::kMatrix_Kind) {
                // matrix multiply
                SpvId result = this->nextId();
                this->writeInstruction(SpvOpMatrixTimesMatrix, this->getType(resultType), result,
                                       lhs, rhs, out);
                ASSERT(lvalue);
                lvalue->store(result, out);
                return result;
            }
            SpvId result = this->writeBinaryOperation(resultType, *operandType, lhs, rhs, SpvOpFMul, 
                                                      SpvOpIMul, SpvOpIMul, SpvOpUndef, out);
            ASSERT(lvalue);
            lvalue->store(result, out);
            return result;
        }
        case Token::SLASHEQ: {
            SpvId result = this->writeBinaryOperation(resultType, *operandType, lhs, rhs, SpvOpFDiv, 
                                                      SpvOpSDiv, SpvOpUDiv, SpvOpUndef, out);
            ASSERT(lvalue);
            lvalue->store(result, out);
            return result;
        }
        default:
            // FIXME: missing support for some operators (bitwise, &&=, ||=, shift...)
            ABORT("unsupported binary expression: %s", b.description().c_str());
    }
}

SpvId SPIRVCodeGenerator::writeLogicalAnd(BinaryExpression& a, std::ostream& out) {
    ASSERT(a.fOperator == Token::LOGICALAND);
    BoolLiteral falseLiteral(Position(), false);
    SpvId falseConstant = this->writeBoolLiteral(falseLiteral);
    SpvId lhs = this->writeExpression(*a.fLeft, out);
    SpvId rhsLabel = this->nextId();
    SpvId end = this->nextId();
    SpvId lhsBlock = fCurrentBlock;
    this->writeInstruction(SpvOpSelectionMerge, end, SpvSelectionControlMaskNone, out);
    this->writeInstruction(SpvOpBranchConditional, lhs, rhsLabel, end, out);
    this->writeLabel(rhsLabel, out);
    SpvId rhs = this->writeExpression(*a.fRight, out);
    SpvId rhsBlock = fCurrentBlock;
    this->writeInstruction(SpvOpBranch, end, out);
    this->writeLabel(end, out);
    SpvId result = this->nextId();
    this->writeInstruction(SpvOpPhi, this->getType(*kBool_Type), result, falseConstant, lhsBlock,
                           rhs, rhsBlock, out);
    return result;
}

SpvId SPIRVCodeGenerator::writeLogicalOr(BinaryExpression& o, std::ostream& out) {
    ASSERT(o.fOperator == Token::LOGICALOR);
    BoolLiteral trueLiteral(Position(), true);
    SpvId trueConstant = this->writeBoolLiteral(trueLiteral);
    SpvId lhs = this->writeExpression(*o.fLeft, out);
    SpvId rhsLabel = this->nextId();
    SpvId end = this->nextId();
    SpvId lhsBlock = fCurrentBlock;
    this->writeInstruction(SpvOpSelectionMerge, end, SpvSelectionControlMaskNone, out);
    this->writeInstruction(SpvOpBranchConditional, lhs, end, rhsLabel, out);
    this->writeLabel(rhsLabel, out);
    SpvId rhs = this->writeExpression(*o.fRight, out);
    SpvId rhsBlock = fCurrentBlock;
    this->writeInstruction(SpvOpBranch, end, out);
    this->writeLabel(end, out);
    SpvId result = this->nextId();
    this->writeInstruction(SpvOpPhi, this->getType(*kBool_Type), result, trueConstant, lhsBlock,
                           rhs, rhsBlock, out);
    return result;
}

SpvId SPIRVCodeGenerator::writeTernaryExpression(TernaryExpression& t, std::ostream& out) {
    SpvId test = this->writeExpression(*t.fTest, out);
    if (t.fIfTrue->isConstant() && t.fIfFalse->isConstant()) {
        // both true and false are constants, can just use OpSelect
        SpvId result = this->nextId();
        SpvId trueId = this->writeExpression(*t.fIfTrue, out);
        SpvId falseId = this->writeExpression(*t.fIfFalse, out);
        this->writeInstruction(SpvOpSelect, this->getType(*t.fType), result, test, trueId, falseId, 
                               out);
        return result;
    }
    // was originally using OpPhi to choose the result, but for some reason that is crashing on 
    // Adreno. Switched to storing the result in a temp variable as glslang does.
    SpvId var = this->nextId();
    this->writeInstruction(SpvOpVariable, this->getPointerType(t.fType, SpvStorageClassFunction), 
                           var, SpvStorageClassFunction, out);
    SpvId trueLabel = this->nextId();
    SpvId falseLabel = this->nextId();
    SpvId end = this->nextId();
    this->writeInstruction(SpvOpSelectionMerge, end, SpvSelectionControlMaskNone, out);
    this->writeInstruction(SpvOpBranchConditional, test, trueLabel, falseLabel, out);
    this->writeLabel(trueLabel, out);
    this->writeInstruction(SpvOpStore, var, this->writeExpression(*t.fIfTrue, out), out);
    this->writeInstruction(SpvOpBranch, end, out);
    this->writeLabel(falseLabel, out);
    this->writeInstruction(SpvOpStore, var, this->writeExpression(*t.fIfFalse, out), out);
    this->writeInstruction(SpvOpBranch, end, out);
    this->writeLabel(end, out);
    SpvId result = this->nextId();
    this->writeInstruction(SpvOpLoad, this->getType(*t.fType), result, var, out);
    return result;
}

Expression* literal_1(const Type& type) {
    static IntLiteral int1(Position(), 1);
    static FloatLiteral float1(Position(), 1.0);
    if (type == *kInt_Type) {
        return &int1;
    }
    else if (type == *kFloat_Type) {
        return &float1;
    } else {
        ABORT("math is unsupported on type '%s'")
    }
}

SpvId SPIRVCodeGenerator::writePrefixExpression(PrefixExpression& p, std::ostream& out) {
    if (p.fOperator == Token::MINUS) {
        SpvId result = this->nextId();
        SpvId typeId = this->getType(*p.fType);
        SpvId expr = this->writeExpression(*p.fOperand, out);
        if (is_float(*p.fType)) {
            this->writeInstruction(SpvOpFNegate, typeId, result, expr, out);
        } else if (is_signed(*p.fType)) {
            this->writeInstruction(SpvOpSNegate, typeId, result, expr, out);
        } else {
            ABORT("unsupported prefix expression %s", p.description().c_str());
        };
        return result;
    }
    switch (p.fOperator) {
        case Token::PLUS:
            return this->writeExpression(*p.fOperand, out);
        case Token::PLUSPLUS: {
            std::unique_ptr<LValue> lv = this->getLValue(*p.fOperand, out);
            SpvId one = this->writeExpression(*literal_1(*p.fType), out);
            SpvId result = this->writeBinaryOperation(*p.fType, *p.fType, lv->load(out), one, 
                                                      SpvOpFAdd, SpvOpIAdd, SpvOpIAdd, SpvOpUndef, 
                                                      out);
            lv->store(result, out);
            return result;
        }
        case Token::MINUSMINUS: {
            std::unique_ptr<LValue> lv = this->getLValue(*p.fOperand, out);
            SpvId one = this->writeExpression(*literal_1(*p.fType), out);
            SpvId result = this->writeBinaryOperation(*p.fType, *p.fType, lv->load(out), one, 
                                                      SpvOpFSub, SpvOpISub, SpvOpISub, SpvOpUndef, 
                                                      out);
            lv->store(result, out);
            return result;
        }
        case Token::NOT: {
            ASSERT(p.fOperand->fType == kBool_Type);
            SpvId result = this->nextId();
            this->writeInstruction(SpvOpLogicalNot, this->getType(*p.fOperand->fType), result,
                                   this->writeExpression(*p.fOperand, out), out);
            return result;
        }
        default:
            ABORT("unsupported prefix expression: %s", p.description().c_str());
    }
}

SpvId SPIRVCodeGenerator::writePostfixExpression(PostfixExpression& p, std::ostream& out) {
    std::unique_ptr<LValue> lv = this->getLValue(*p.fOperand, out);
    SpvId result = lv->load(out);
    SpvId one = this->writeExpression(*literal_1(*p.fType), out);
    switch (p.fOperator) {
        case Token::PLUSPLUS: {
            SpvId temp = this->writeBinaryOperation(*p.fType, *p.fType, result, one, SpvOpFAdd, 
                                                    SpvOpIAdd, SpvOpIAdd, SpvOpUndef, out);
            lv->store(temp, out);
            return result;
        }
        case Token::MINUSMINUS: {
            SpvId temp = this->writeBinaryOperation(*p.fType, *p.fType, result, one, SpvOpFSub, 
                                                    SpvOpISub, SpvOpISub, SpvOpUndef, out);
            lv->store(temp, out);
            return result;
        }
        default:
            ABORT("unsupported postfix expression %s", p.description().c_str());
    }
}

SpvId SPIRVCodeGenerator::writeBoolLiteral(BoolLiteral& b) {
    if (b.fValue) {
        if (fBoolTrue == 0) {
            fBoolTrue = this->nextId();
            this->writeInstruction(SpvOpConstantTrue, this->getType(*b.fType), fBoolTrue, 
                                   fConstantBuffer);
        }
        return fBoolTrue;
    } else {
        if (fBoolFalse == 0) {
            fBoolFalse = this->nextId();
            this->writeInstruction(SpvOpConstantFalse, this->getType(*b.fType), fBoolFalse, 
                                   fConstantBuffer);
        }
        return fBoolFalse;
    }
}

SpvId SPIRVCodeGenerator::writeIntLiteral(IntLiteral& i) {
    if (i.fType == kInt_Type) {
        auto entry = fIntConstants.find(i.fValue);
        if (entry == fIntConstants.end()) {
            SpvId result = this->nextId();
            this->writeInstruction(SpvOpConstant, this->getType(*i.fType), result, (SpvId) i.fValue, 
                                   fConstantBuffer);
            fIntConstants[i.fValue] = result;
            return result;
        }
        return entry->second;
    } else {
        ASSERT(i.fType == kUInt_Type);
        auto entry = fUIntConstants.find(i.fValue);
        if (entry == fUIntConstants.end()) {
            SpvId result = this->nextId();
            this->writeInstruction(SpvOpConstant, this->getType(*i.fType), result, (SpvId) i.fValue, 
                                   fConstantBuffer);
            fUIntConstants[i.fValue] = result;
            return result;
        }
        return entry->second;
    }
}

SpvId SPIRVCodeGenerator::writeFloatLiteral(FloatLiteral& f) {
    if (f.fType == kFloat_Type) {
        float value = (float) f.fValue;
        auto entry = fFloatConstants.find(value);
        if (entry == fFloatConstants.end()) {
            SpvId result = this->nextId();
            uint32_t bits;
            ASSERT(sizeof(bits) == sizeof(value));
            memcpy(&bits, &value, sizeof(bits));
            this->writeInstruction(SpvOpConstant, this->getType(*f.fType), result, bits, 
                                   fConstantBuffer);
            fFloatConstants[value] = result;
            return result;
        }
        return entry->second;
    } else {
        ASSERT(f.fType == kDouble_Type);
        auto entry = fDoubleConstants.find(f.fValue);
        if (entry == fDoubleConstants.end()) {
            SpvId result = this->nextId();
            uint64_t bits;
            ASSERT(sizeof(bits) == sizeof(f.fValue));
            memcpy(&bits, &f.fValue, sizeof(bits));
            this->writeInstruction(SpvOpConstant, this->getType(*f.fType), result, 
                                   bits & 0xffffffff, bits >> 32, fConstantBuffer);
            fDoubleConstants[f.fValue] = result;
            return result;
        }
        return entry->second;
    }
}

SpvId SPIRVCodeGenerator::writeFunctionStart(std::shared_ptr<FunctionDeclaration> f, 
                                             std::ostream& out) {
    SpvId result = fFunctionMap[f];
    this->writeInstruction(SpvOpFunction, this->getType(*f->fReturnType), result, 
                           SpvFunctionControlMaskNone, this->getFunctionType(f), out);
    this->writeInstruction(SpvOpName, result, f->fName.c_str(), fNameBuffer);
    for (size_t i = 0; i < f->fParameters.size(); i++) {
        SpvId id = this->nextId();
        fVariableMap[f->fParameters[i]] = id;
        SpvId type;
        type = this->getPointerType(f->fParameters[i]->fType, SpvStorageClassFunction);
        this->writeInstruction(SpvOpFunctionParameter, type, id, out);
    }
    return result;
}

SpvId SPIRVCodeGenerator::writeFunction(FunctionDefinition& f, std::ostream& out) {
    SpvId result = this->writeFunctionStart(f.fDeclaration, out);
    this->writeLabel(this->nextId(), out);
    if (f.fDeclaration->fName == "main") {
        out << fGlobalInitializersBuffer.str();
    }
    std::stringstream bodyBuffer;
    this->writeBlock(*f.fBody, bodyBuffer);
    out << fVariableBuffer.str();
    fVariableBuffer.str("");
    out << bodyBuffer.str();
    if (fCurrentBlock) {
        this->writeInstruction(SpvOpReturn, out);
    }
    this->writeInstruction(SpvOpFunctionEnd, out);
    return result;
}

void SPIRVCodeGenerator::writeLayout(const Layout& layout, SpvId target) {
    if (layout.fLocation >= 0) {
        this->writeInstruction(SpvOpDecorate, target, SpvDecorationLocation, layout.fLocation, 
                               fDecorationBuffer);
    }
    if (layout.fBinding >= 0) {
        this->writeInstruction(SpvOpDecorate, target, SpvDecorationBinding, layout.fBinding, 
                               fDecorationBuffer);
    }
    if (layout.fIndex >= 0) {
        this->writeInstruction(SpvOpDecorate, target, SpvDecorationIndex, layout.fIndex, 
                               fDecorationBuffer);
    }
    if (layout.fSet >= 0) {
        this->writeInstruction(SpvOpDecorate, target, SpvDecorationDescriptorSet, layout.fSet, 
                               fDecorationBuffer);
    }
    if (layout.fBuiltin >= 0) {
        this->writeInstruction(SpvOpDecorate, target, SpvDecorationBuiltIn, layout.fBuiltin, 
                               fDecorationBuffer);
    }
}

void SPIRVCodeGenerator::writeLayout(const Layout& layout, SpvId target, int member) {
    if (layout.fLocation >= 0) {
        this->writeInstruction(SpvOpMemberDecorate, target, member, SpvDecorationLocation, 
                               layout.fLocation, fDecorationBuffer);
    }
    if (layout.fBinding >= 0) {
        this->writeInstruction(SpvOpMemberDecorate, target, member, SpvDecorationBinding, 
                               layout.fBinding, fDecorationBuffer);
    }
    if (layout.fIndex >= 0) {
        this->writeInstruction(SpvOpMemberDecorate, target, member, SpvDecorationIndex, 
                               layout.fIndex, fDecorationBuffer);
    }
    if (layout.fSet >= 0) {
        this->writeInstruction(SpvOpMemberDecorate, target, member, SpvDecorationDescriptorSet, 
                               layout.fSet, fDecorationBuffer);
    }
    if (layout.fBuiltin >= 0) {
        this->writeInstruction(SpvOpMemberDecorate, target, member, SpvDecorationBuiltIn, 
                               layout.fBuiltin, fDecorationBuffer);
    }
}

SpvId SPIRVCodeGenerator::writeInterfaceBlock(InterfaceBlock& intf) {
    SpvId type = this->getType(*intf.fVariable->fType);
    SpvId result = this->nextId();
    this->writeInstruction(SpvOpDecorate, type, SpvDecorationBlock, fDecorationBuffer);
    SpvStorageClass_ storageClass = get_storage_class(intf.fVariable->fModifiers);
    SpvId ptrType = this->nextId();
    this->writeInstruction(SpvOpTypePointer, ptrType, storageClass, type, fConstantBuffer);
    this->writeInstruction(SpvOpVariable, ptrType, result, storageClass, fConstantBuffer);
    this->writeLayout(intf.fVariable->fModifiers.fLayout, result);
    fVariableMap[intf.fVariable] = result;
    return result;
}

void SPIRVCodeGenerator::writeGlobalVars(VarDeclaration& decl, std::ostream& out) {
    for (size_t i = 0; i < decl.fVars.size(); i++) {
        if (!decl.fVars[i]->fIsReadFrom && !decl.fVars[i]->fIsWrittenTo) {
            continue;
        }
        SpvStorageClass_ storageClass;
        if (decl.fVars[i]->fModifiers.fFlags & Modifiers::kIn_Flag) {
            storageClass = SpvStorageClassInput;
        } else if (decl.fVars[i]->fModifiers.fFlags & Modifiers::kOut_Flag) {
            storageClass = SpvStorageClassOutput;
        } else if (decl.fVars[i]->fModifiers.fFlags & Modifiers::kUniform_Flag) {
            if (decl.fVars[i]->fType->kind() == Type::kSampler_Kind) {
                storageClass = SpvStorageClassUniformConstant;
            } else {
                storageClass = SpvStorageClassUniform;
            }
        } else {
            storageClass = SpvStorageClassPrivate;
        }
        SpvId id = this->nextId();
        fVariableMap[decl.fVars[i]] = id;
        SpvId type = this->getPointerType(decl.fVars[i]->fType, storageClass);
        this->writeInstruction(SpvOpVariable, type, id, storageClass, fConstantBuffer);
        this->writeInstruction(SpvOpName, id, decl.fVars[i]->fName.c_str(), fNameBuffer);
        if (decl.fVars[i]->fType->kind() == Type::kMatrix_Kind) {
            this->writeInstruction(SpvOpMemberDecorate, id, (SpvId) i, SpvDecorationColMajor, 
                                   fDecorationBuffer);
            this->writeInstruction(SpvOpMemberDecorate, id, (SpvId) i, SpvDecorationMatrixStride, 
                                   (SpvId) decl.fVars[i]->fType->stride(), fDecorationBuffer);
        }
        if (decl.fValues[i]) {
			ASSERT(!fCurrentBlock);
			fCurrentBlock = -1;
            SpvId value = this->writeExpression(*decl.fValues[i], fGlobalInitializersBuffer);
            this->writeInstruction(SpvOpStore, id, value, fGlobalInitializersBuffer);
			fCurrentBlock = 0;
        }
        this->writeLayout(decl.fVars[i]->fModifiers.fLayout, id);
    }
}

void SPIRVCodeGenerator::writeVarDeclaration(VarDeclaration& decl, std::ostream& out) {
    for (size_t i = 0; i < decl.fVars.size(); i++) {
        SpvId id = this->nextId();
        fVariableMap[decl.fVars[i]] = id;
        SpvId type = this->getPointerType(decl.fVars[i]->fType, SpvStorageClassFunction);
        this->writeInstruction(SpvOpVariable, type, id, SpvStorageClassFunction, fVariableBuffer);
        this->writeInstruction(SpvOpName, id, decl.fVars[i]->fName.c_str(), fNameBuffer);
        if (decl.fValues[i]) {
            SpvId value = this->writeExpression(*decl.fValues[i], out);
            this->writeInstruction(SpvOpStore, id, value, out);
        }
    }
}

void SPIRVCodeGenerator::writeStatement(Statement& s, std::ostream& out) {
    switch (s.fKind) {
        case Statement::kBlock_Kind:
            this->writeBlock((Block&) s, out);
            break;
        case Statement::kExpression_Kind:
            this->writeExpression(*((ExpressionStatement&) s).fExpression, out);
            break;
        case Statement::kReturn_Kind: 
            this->writeReturnStatement((ReturnStatement&) s, out);
            break;
        case Statement::kVarDeclaration_Kind:
            this->writeVarDeclaration(*((VarDeclarationStatement&) s).fDeclaration, out);
            break;
        case Statement::kIf_Kind:
            this->writeIfStatement((IfStatement&) s, out);
            break;
        case Statement::kFor_Kind:
            this->writeForStatement((ForStatement&) s, out);
            break;
        case Statement::kBreak_Kind:
            this->writeInstruction(SpvOpBranch, fBreakTarget.top(), out);
            break;
        case Statement::kContinue_Kind:
            this->writeInstruction(SpvOpBranch, fContinueTarget.top(), out);
            break;
        case Statement::kDiscard_Kind:
            this->writeInstruction(SpvOpKill, out);
            break;
        default:
            ABORT("unsupported statement: %s", s.description().c_str());
    }
}

void SPIRVCodeGenerator::writeBlock(Block& b, std::ostream& out) {
    for (size_t i = 0; i < b.fStatements.size(); i++) {
        this->writeStatement(*b.fStatements[i], out);
    }
}

void SPIRVCodeGenerator::writeIfStatement(IfStatement& stmt, std::ostream& out) {
    SpvId test = this->writeExpression(*stmt.fTest, out);
    SpvId ifTrue = this->nextId();
    SpvId ifFalse = this->nextId();
    if (stmt.fIfFalse) {
        SpvId end = this->nextId();
        this->writeInstruction(SpvOpSelectionMerge, end, SpvSelectionControlMaskNone, out);
        this->writeInstruction(SpvOpBranchConditional, test, ifTrue, ifFalse, out);
        this->writeLabel(ifTrue, out);
        this->writeStatement(*stmt.fIfTrue, out);
        if (fCurrentBlock) {
            this->writeInstruction(SpvOpBranch, end, out);
        }
        this->writeLabel(ifFalse, out);
        this->writeStatement(*stmt.fIfFalse, out);
        if (fCurrentBlock) {
            this->writeInstruction(SpvOpBranch, end, out);
        }
        this->writeLabel(end, out);
    } else {
        this->writeInstruction(SpvOpSelectionMerge, ifFalse, SpvSelectionControlMaskNone, out);
        this->writeInstruction(SpvOpBranchConditional, test, ifTrue, ifFalse, out);
        this->writeLabel(ifTrue, out);
        this->writeStatement(*stmt.fIfTrue, out);
        if (fCurrentBlock) {
            this->writeInstruction(SpvOpBranch, ifFalse, out);
        }
        this->writeLabel(ifFalse, out);
    }
}

void SPIRVCodeGenerator::writeForStatement(ForStatement& f, std::ostream& out) {
    if (f.fInitializer) {
        this->writeStatement(*f.fInitializer, out);
    }
    SpvId header = this->nextId();
    SpvId start = this->nextId();
    SpvId body = this->nextId();
    SpvId next = this->nextId();
    fContinueTarget.push(next);
    SpvId end = this->nextId();
    fBreakTarget.push(end);
    this->writeInstruction(SpvOpBranch, header, out);
    this->writeLabel(header, out);
    this->writeInstruction(SpvOpLoopMerge, end, next, SpvLoopControlMaskNone, out);
	this->writeInstruction(SpvOpBranch, start, out);
    this->writeLabel(start, out);
    SpvId test = this->writeExpression(*f.fTest, out);
    this->writeInstruction(SpvOpBranchConditional, test, body, end, out);
    this->writeLabel(body, out);
    this->writeStatement(*f.fStatement, out);
    if (fCurrentBlock) {
        this->writeInstruction(SpvOpBranch, next, out);
    }
    this->writeLabel(next, out);
    if (f.fNext) {
        this->writeExpression(*f.fNext, out);
    }
    this->writeInstruction(SpvOpBranch, header, out);
    this->writeLabel(end, out);
    fBreakTarget.pop();
    fContinueTarget.pop();
}

void SPIRVCodeGenerator::writeReturnStatement(ReturnStatement& r, std::ostream& out) {
    if (r.fExpression) {
        this->writeInstruction(SpvOpReturnValue, this->writeExpression(*r.fExpression, out), 
                               out);
    } else {
        this->writeInstruction(SpvOpReturn, out);
    }
}

void SPIRVCodeGenerator::writeInstructions(Program& program, std::ostream& out) {
    fGLSLExtendedInstructions = this->nextId();
    std::stringstream body;
    std::vector<SpvId> interfaceVars;
    // assign IDs to functions
    for (size_t i = 0; i < program.fElements.size(); i++) {
        if (program.fElements[i]->fKind == ProgramElement::kFunction_Kind) {
            FunctionDefinition& f = (FunctionDefinition&) *program.fElements[i];
            fFunctionMap[f.fDeclaration] = this->nextId();
        }
    }
    for (size_t i = 0; i < program.fElements.size(); i++) {
        if (program.fElements[i]->fKind == ProgramElement::kInterfaceBlock_Kind) {
            InterfaceBlock& intf = (InterfaceBlock&) *program.fElements[i];
            SpvId id = this->writeInterfaceBlock(intf);
            if ((intf.fVariable->fModifiers.fFlags & Modifiers::kIn_Flag) ||
                (intf.fVariable->fModifiers.fFlags & Modifiers::kOut_Flag)) {
                interfaceVars.push_back(id);
            }
        }
    }
    for (size_t i = 0; i < program.fElements.size(); i++) {
        if (program.fElements[i]->fKind == ProgramElement::kVar_Kind) {
            this->writeGlobalVars(((VarDeclaration&) *program.fElements[i]), body);
        }
    }
    for (size_t i = 0; i < program.fElements.size(); i++) {
        if (program.fElements[i]->fKind == ProgramElement::kFunction_Kind) {
            this->writeFunction(((FunctionDefinition&) *program.fElements[i]), body);
        }
    }
    std::shared_ptr<FunctionDeclaration> main = nullptr;
    for (auto entry : fFunctionMap) {
		if (entry.first->fName == "main") {
            main = entry.first;
        }
    }
    ASSERT(main);
    for (auto entry : fVariableMap) {
        std::shared_ptr<Variable> var = entry.first;
        if (var->fStorage == Variable::kGlobal_Storage && 
                ((var->fModifiers.fFlags & Modifiers::kIn_Flag) ||
                 (var->fModifiers.fFlags & Modifiers::kOut_Flag))) {
            interfaceVars.push_back(entry.second);
        }
    }
    this->writeCapabilities(out);
    this->writeInstruction(SpvOpExtInstImport, fGLSLExtendedInstructions, "GLSL.std.450", out);
    this->writeInstruction(SpvOpMemoryModel, SpvAddressingModelLogical, SpvMemoryModelGLSL450, out);
    this->writeOpCode(SpvOpEntryPoint, (SpvId) (3 + (strlen(main->fName.c_str()) + 4) / 4) + 
                      (int32_t) interfaceVars.size(), out);
    switch (program.fKind) {
        case Program::kVertex_Kind:
            this->writeWord(SpvExecutionModelVertex, out);
            break;
        case Program::kFragment_Kind:
            this->writeWord(SpvExecutionModelFragment, out);
            break;
    }
    this->writeWord(fFunctionMap[main], out);
    this->writeString(main->fName.c_str(), out);
    for (int var : interfaceVars) {
        this->writeWord(var, out);
    }
    if (program.fKind == Program::kFragment_Kind) {
        this->writeInstruction(SpvOpExecutionMode, 
                               fFunctionMap[main], 
                               SpvExecutionModeOriginUpperLeft,
                               out);
    }
    for (size_t i = 0; i < program.fElements.size(); i++) {
        if (program.fElements[i]->fKind == ProgramElement::kExtension_Kind) {
            this->writeInstruction(SpvOpSourceExtension, 
                                   ((Extension&) *program.fElements[i]).fName.c_str(), 
                                   out);
        }
    }
    
    out << fNameBuffer.str();
    out << fDecorationBuffer.str();
    out << fConstantBuffer.str();
    out << fExternalFunctionsBuffer.str();
    out << body.str();
}

void SPIRVCodeGenerator::generateCode(Program& program, std::ostream& out) {
    this->writeWord(SpvMagicNumber, out);
    this->writeWord(SpvVersion, out);
    this->writeWord(SKSL_MAGIC, out);
    std::stringstream buffer;
    this->writeInstructions(program, buffer);
    this->writeWord(fIdCount, out);
    this->writeWord(0, out); // reserved, always zero
    out << buffer.str();
}

}
