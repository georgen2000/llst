#include <inference.h>

using namespace type;

void TypeAnalyzer::processInstruction(const InstructionNode& instruction) {
    const TSmalltalkInstruction::TArgument argument = instruction.getInstruction().getArgument();

    switch (instruction.getInstruction().getOpcode()) {
        case opcode::pushArgument:
            m_context[instruction] = m_context.getArgument(argument);
            break;

        case opcode::pushConstant:  doPushConstant(instruction);  break;
        case opcode::pushLiteral:   doPushLiteral(instruction);   break;
        case opcode::markArguments: doMarkArguments(instruction); break;

        case opcode::sendBinary:    doSendBinary(instruction);    break;

        case opcode::sendMessage:
            // For now, treat method call as *
            m_context[instruction] = Type(Type::tkPolytype);
            break;

        default:
            break;
    }
}

void TypeAnalyzer::doPushConstant(const InstructionNode& instruction) {
    const TSmalltalkInstruction::TArgument argument = instruction.getInstruction().getArgument();
    Type& type = m_context[instruction];

    switch (argument) {
        case 0: case 1: case 2: case 3: case 4:
        case 5: case 6: case 7: case 8: case 9:
            type.set(TInteger(argument));
            break;

        case pushConstants::nil:         type.set(globals.nilObject);   break;
        case pushConstants::trueObject:  type.set(globals.trueObject);  break;
        case pushConstants::falseObject: type.set(globals.falseObject); break;

        default:
            std::fprintf(stderr, "VM: unknown push constant %d\n", argument);
            type.reset();
    }
}

void TypeAnalyzer::doPushLiteral(const InstructionNode& instruction) {
    TMethod* const method  = m_graph.getParsedMethod()->getOrigin();
    const TSmalltalkInstruction::TArgument argument = instruction.getInstruction().getArgument();
    TObject* const literal = method->literals->getField(argument);

    m_context[instruction] = Type(literal);
}

void TypeAnalyzer::doSendUnary(const InstructionNode& instruction) {
    const Type& argType = m_context[*instruction.getArgument()];
    const unaryBuiltIns::Opcode opcode = static_cast<unaryBuiltIns::Opcode>(instruction.getInstruction().getArgument());

    Type& result = m_context[instruction];
    switch (argType.getKind()) {
        case Type::tkLiteral:
        case Type::tkMonotype:
        {
            const bool isValueNil =
                   (argType.getValue() == globals.nilObject)
                || (argType.getValue() == globals.nilObject->getClass());

            if (opcode == unaryBuiltIns::isNil)
                result.set(isValueNil ? globals.trueObject : globals.falseObject);
            else
                result.set(isValueNil ? globals.falseObject : globals.trueObject);
            break;
        }

        case Type::tkComposite:
        case Type::tkArray:
        {
            // TODO Repeat the procedure over each subtype
            result.setKind(Type::tkPolytype);
            break;
        }

        default:
            // * isNil  = (Boolean)
            // * notNil = (Boolean)
            result.set(globals.trueObject->getClass()->getClass());
    }

}

void TypeAnalyzer::doSendBinary(const InstructionNode& instruction) {
    const Type& type1 = m_context[*instruction.getArgument(0)];
    const Type& type2 = m_context[*instruction.getArgument(1)];
    const binaryBuiltIns::Operator opcode = static_cast<binaryBuiltIns::Operator>(instruction.getInstruction().getArgument());

    Type& result = m_context[instruction];

    if (isSmallInteger(type1.getValue()) && isSmallInteger(type1.getValue())) {
        const int32_t rightOperand = TInteger(type2.getValue());
        const int32_t leftOperand  = TInteger(type1.getValue());

        switch (opcode) {
            case binaryBuiltIns::operatorLess:
                result.set((leftOperand < rightOperand) ? globals.trueObject : globals.falseObject);
                break;

            case binaryBuiltIns::operatorLessOrEq:
                result.set((leftOperand <= rightOperand) ? globals.trueObject : globals.falseObject);
                break;

            case binaryBuiltIns::operatorPlus:
                result.set(TInteger(leftOperand + rightOperand));
                break;

            default:
                std::fprintf(stderr, "VM: Invalid opcode %d passed to sendBinary\n", opcode);
        }

        return;
    }

    // Literal int or (SmallInt) monotype
    const bool isInt1 = isSmallInteger(type1.getValue()) || type1.getValue() == globals.smallIntClass;
    const bool isInt2 = isSmallInteger(type2.getValue()) || type2.getValue() == globals.smallIntClass;

    if (isInt1 && isInt2) {
        switch (opcode) {
            case binaryBuiltIns::operatorLess:
            case binaryBuiltIns::operatorLessOrEq:
                // (SmallInt) <= (SmallInt) = (Boolean)
                result.set(globals.trueObject->getClass()->getClass());
                break;

            case binaryBuiltIns::operatorPlus:
                // (SmallInt) + (SmallInt) = (SmallInt)
                result.set(globals.smallIntClass);
                break;

            default:
                std::fprintf(stderr, "VM: Invalid opcode %d passed to sendBinary\n", opcode);
                result.reset(); // ?
        }

        return;
    }

    // TODO In case of complex invocation encode resursive analysis of operator as a message

    result.setKind(Type::tkPolytype);
}

void TypeAnalyzer::doMarkArguments(const InstructionNode& instruction) {
    const Type& argsType = m_context[*instruction.getArgument()];
    Type& result = m_context[instruction];

    if (argsType.getKind() == Type::tkUndefined || argsType.getKind() == Type::tkPolytype) {
        result.set(globals.arrayClass);
        return;
    }

    for (std::size_t index = 0; index < argsType.getSubTypes().size(); index++)
        result.addSubType(argsType[index]);

    result.set(globals.arrayClass, Type::tkArray);
}

void TypeAnalyzer::processPhi(const PhiNode& phi) {
    Type& result = m_context[phi];

    const TNodeSet& incomings = phi.getRealValues();
    TNodeSet::iterator iNode = incomings.begin();
    for (; iNode != incomings.end(); ++iNode)
        result.addSubType(m_context[*(*iNode)->cast<InstructionNode>()]);

    result.setKind(Type::tkComposite);
}

void TypeAnalyzer::processTau(const TauNode& tau) {
    Type& result = m_context[tau];
    result.setKind(Type::tkPolytype);
}

void TypeAnalyzer::walkComplete() {

}
