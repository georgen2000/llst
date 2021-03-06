#ifndef LLST_HELPER_CONTROL_GRAPH_INCLUDED
#define LLST_HELPER_CONTROL_GRAPH_INCLUDED

#include <gtest/gtest.h>
#include <instructions.h>
#include <analysis.h>

#include <algorithm>

#include "AssertFail.h"

class H_LastInstIsTerminator: public st::BasicBlockVisitor
{
public:
    H_LastInstIsTerminator(st::ParsedBytecode* parsedBytecode) : st::BasicBlockVisitor(parsedBytecode) {}
    virtual bool visitBlock(st::BasicBlock& BB) {
        std::size_t bbSize = BB.size();
        EXPECT_NE(0u, bbSize);

        st::TSmalltalkInstruction terminator(0);
        bool hasTerminator = BB.getTerminator(terminator);
        {
            SCOPED_TRACE("Each BB must have a terminator");
            EXPECT_TRUE(hasTerminator);
        }
        {
            SCOPED_TRACE("The instruction returned by BB::getTerminator must be a terminator");
            EXPECT_TRUE( terminator.isTerminator() );
        }
        {
            SCOPED_TRACE("The last instruction must be a terminator and it must be equal to BB::getTerminator");
            st::TSmalltalkInstruction lastInst = BB[bbSize-1];
            EXPECT_TRUE( lastInst.isTerminator() );
            EXPECT_EQ( lastInst.serialize(), terminator.serialize() );
        }
        {
            SCOPED_TRACE("There must be no terminators but the last one");
            st::BasicBlock::iterator iInstruction = BB.begin();
            st::BasicBlock::iterator iEnd         = BB.end()-1; // except the last inst

            while (iInstruction != iEnd) {
                EXPECT_FALSE((*iInstruction).isTerminator());
                ++iInstruction;
            }
        }

        return true;
    }
};

class H_DomainHasTerminator: public st::DomainVisitor
{
public:
    H_DomainHasTerminator(st::ControlGraph* graph) : st::DomainVisitor(graph) {}
    virtual bool visitDomain(st::ControlDomain& domain) {
        st::InstructionNode* terminator = domain.getTerminator();
        {
            SCOPED_TRACE("Each domain must have a terminator");
            EXPECT_TRUE( terminator != NULL );
            if (terminator)
                EXPECT_TRUE( terminator->getInstruction().isTerminator() );
        }
        return true;
    }
};

class H_AreBBsLinked: public st::NodeVisitor
{
public:
    H_AreBBsLinked(st::ControlGraph* graph) : st::NodeVisitor(graph) {}
    virtual bool visitDomain(st::ControlDomain& domain) {
        st::BasicBlock* currentBB = domain.getBasicBlock();
        if (currentBB->getOffset() != 0) {
            EXPECT_GT(currentBB->getReferers().size(), 0u) << "All BB but the 1st must have referrers. BB offset: " << currentBB->getOffset();
        }
        st::ControlDomain::iterator iNode = domain.begin();
        const st::ControlDomain::iterator iEnd = domain.end();

        if (iNode != iEnd) {
            while (iNode != iEnd) {
                if (! visitNode(** iNode))
                    return false;
                ++iNode;
            }
        }

        return true;
    }
    virtual bool visitNode(st::ControlNode& node) {
        st::BasicBlock* currentBB = node.getDomain()->getBasicBlock();
        if (st::InstructionNode* inst = node.cast<st::InstructionNode>()) {
            if (inst->getInstruction().isBranch()) {
                st::TSmalltalkInstruction branch = inst->getInstruction();
                const st::TNodeSet& outEdges = node.getOutEdges();
                if (branch.getArgument() == special::branchIfTrue || branch.getArgument() == special::branchIfFalse) {
                    EXPECT_EQ(2u, outEdges.size());
                    if (outEdges.size() == 2) {
                        st::TNodeSet::const_iterator edgeIter = outEdges.begin();
                        st::ControlNode* target1 = *(edgeIter++);
                        st::ControlNode* target2 = *edgeIter;
                        st::ControlDomain* target1Domain = target1->getDomain();
                        st::ControlDomain* target2Domain = target2->getDomain();
                        EXPECT_EQ(target1, target1Domain->getEntryPoint());
                        EXPECT_EQ(target2, target2Domain->getEntryPoint());
                        uint16_t target1Offset = target1Domain->getBasicBlock()->getOffset();
                        uint16_t target2Offset = target2Domain->getBasicBlock()->getOffset();

                        bool pointToFirst = target1Offset == branch.getExtra();
                        bool pointToSecond = target2Offset == branch.getExtra();
                        EXPECT_TRUE( pointToFirst || pointToSecond ) << "branchIf* must point to one of the targets";
                        EXPECT_FALSE( pointToFirst && pointToSecond ) << "branchIf* must point to only one of the targets";

                        {
                            SCOPED_TRACE("The referrers of outgoing edges must contain current BB");
                            st::BasicBlock::TBasicBlockSet& referrers1 = target1->getDomain()->getBasicBlock()->getReferers();
                            EXPECT_NE(referrers1.end(), referrers1.find(currentBB));
                            st::BasicBlock::TBasicBlockSet& referrers2 = target2->getDomain()->getBasicBlock()->getReferers();
                            EXPECT_NE(referrers2.end(), referrers2.find(currentBB));
                        }
                    }
                }
                if (branch.getArgument() == special::branch) {
                    EXPECT_EQ(1u, outEdges.size());
                    if (outEdges.size() == 1) {
                        st::ControlNode* target = *(outEdges.begin());
                        st::ControlDomain* targetDomain = target->getDomain();
                        EXPECT_EQ(target, targetDomain->getEntryPoint());
                        uint16_t targetOffset = targetDomain->getBasicBlock()->getOffset();
                        EXPECT_EQ(targetOffset, branch.getExtra()) << "Unconditional branch must point exactly to its the only one out edge";
                        st::BasicBlock::TBasicBlockSet& referrers = target->getDomain()->getBasicBlock()->getReferers();
                        EXPECT_NE(referrers.end(), referrers.find(currentBB));
                    }
                }
            }
        }
        return true;
    }
};

class H_CorrectNumOfEdges: public st::PlainNodeVisitor
{
public:
    H_CorrectNumOfEdges(st::ControlGraph* graph) : st::PlainNodeVisitor(graph) {}
    virtual bool visitNode(st::ControlNode& node) {
        if (st::InstructionNode* inst = node.cast<st::InstructionNode>())
        {
            SCOPED_TRACE(inst->getInstruction().toString());
            switch (inst->getInstruction().getOpcode()) {
                case opcode::pushInstance:
                case opcode::pushArgument:
                case opcode::pushTemporary:
                case opcode::pushLiteral:
                case opcode::pushConstant:
                case opcode::pushBlock:
                    EXPECT_EQ(0u, inst->getArgumentsCount());
                    break;
                case opcode::sendUnary:
                case opcode::assignInstance:
                case opcode::assignTemporary:
                    EXPECT_EQ(1u, inst->getArgumentsCount());
                    break;
                case opcode::sendBinary:
                    EXPECT_EQ(2u, inst->getArgumentsCount());
                    break;
                case opcode::doSpecial: {
                    switch (inst->getInstruction().getArgument()) {
                        case special::stackReturn:
                        case special::blockReturn:
                        case special::popTop:
                        case special::branchIfTrue:
                        case special::branchIfFalse:
                        case special::duplicate:
                        case special::sendToSuper:
                            EXPECT_EQ(1u, inst->getArgumentsCount());
                            break;
                        case special::branch:
                            EXPECT_EQ(0u, inst->getArgumentsCount());
                            break;
                    }
                } break;
                case opcode::doPrimitive:
                    EXPECT_EQ(inst->getInstruction().getArgument(), inst->getArgumentsCount());
                    break;
                default:
                    EXPECT_GE(inst->getArgumentsCount(), 1u);
                    break;
            }
            if (inst->getInstruction().isValueProvider() && inst->getInstruction().getOpcode() != opcode::pushBlock) {
                const st::TNodeSet& consumers = inst->getConsumers();
                EXPECT_GT(consumers.size(), 0u);
            }
        }
        if (st::PhiNode* phi = node.cast<st::PhiNode>()) {
            const st::PhiNode::TIncomingList& incomingList = phi->getIncomingList();
            const st::TNodeSet& outEdges = phi->getOutEdges();
            EXPECT_GT(incomingList.size(), 0u) << "The phi must have at least 1 incoming edge";
            EXPECT_GE(outEdges.size(), 1u) << "There must be a node using the given phi";
        }
        EXPECT_NE(st::ControlNode::ntTau, node.getNodeType()); // TODO: Tau logic is not done yet
        return true;
    }
};

class H_NoOrphans {
    st::ControlGraph* m_cfg;
public:
    H_NoOrphans(st::ControlGraph* graph) : m_cfg(graph) {}
    void check() {
        st::TNodeSet linkedNodes = getLinkedNodes();
        st::TNodeSet allNodes = getAllNodes();
        typedef std::vector<st::ControlNode*> TOrphans;
        TOrphans orphans;
        std::set_difference(
            linkedNodes.begin(), linkedNodes.end(),
            allNodes.begin(), allNodes.end(),
            std::back_inserter(orphans)
        );
        EXPECT_EQ(0u, orphans.size());
    }
private:
    st::TNodeSet getLinkedNodes() const {
        class LinkedNodesGetter : public st::NodeVisitor {
        public:
            st::TNodeSet& m_visitedNodes;
            LinkedNodesGetter(st::ControlGraph* graph, st::TNodeSet& outResult)
                : st::NodeVisitor(graph), m_visitedNodes(outResult) {}
            virtual bool visitNode(st::ControlNode& node) {
                m_visitedNodes.insert(&node);
                st::TNodeSet outEdges = node.getOutEdges();
                for(st::TNodeSet::const_iterator it = outEdges.begin(); it != outEdges.end(); ++it) {
                    m_visitedNodes.insert(*it);
                }
                return true;
            }
        };
        st::TNodeSet result;
        LinkedNodesGetter getter(m_cfg, result);
        getter.run();
        return result;
    }
    st::TNodeSet getAllNodes() const {
        class AllNodesGetter : public st::PlainNodeVisitor {
        public:
            st::TNodeSet& m_visitedNodes;
            AllNodesGetter(st::ControlGraph* graph, st::TNodeSet& outResult)
                : st::PlainNodeVisitor(graph), m_visitedNodes(outResult) {}
            virtual bool visitNode(st::ControlNode& node) {
                m_visitedNodes.insert(&node);
                return true;
            }
        };
        st::TNodeSet result;
        AllNodesGetter getter(m_cfg, result);
        getter.run();
        return result;
    }
};

class H_ConsumeProvider: public st::PlainNodeVisitor
{
public:
    H_ConsumeProvider(st::ControlGraph* graph) : st::PlainNodeVisitor(graph) {}
    virtual bool visitNode(st::ControlNode& node) {
        if (st::InstructionNode* instNode = node.cast<st::InstructionNode>()) {
            st::TSmalltalkInstruction inst = instNode->getInstruction();
            if (inst.isValueConsumer()) {
                SCOPED_TRACE(inst.toString());
                std::size_t argSize = instNode->getArgumentsCount();
                for(std::size_t i = 0; i < argSize; ++i) {
                    st::ControlNode* argNode = instNode->getArgument(i);
                    if (st::InstructionNode* arg = argNode->cast<st::InstructionNode>()) {
                        std::string provider = arg->getInstruction().toString();
                        std::string consumer = inst.toString();
                        EXPECT_TRUE(arg->getInstruction().isValueProvider()) << "'" << provider << "' should provide value for '" << consumer << "'";
                    }
                }
            }
        }
        if (st::PhiNode* phiNode = node.cast<st::PhiNode>()) {
            const st::TNodeSet& inEdges = phiNode->getInEdges();
            for(st::TNodeSet::const_iterator it = inEdges.begin(); it != inEdges.end(); ++it) {
                EXPECT_NE(st::ControlNode::ntInstruction, (*it)->getNodeType());
            }
        }
        return true;
    }
};

class H_BranchJumpsOnCorrectNode: public st::NodeVisitor
{
public:
    H_BranchJumpsOnCorrectNode(st::ControlGraph* graph) : st::NodeVisitor(graph) {}
    virtual bool visitNode(st::ControlNode& node) {
        if (st::InstructionNode* instNode = node.cast<st::InstructionNode>()) {
            st::TSmalltalkInstruction inst = instNode->getInstruction();
            if (inst.isBranch()) {
                const st::TNodeSet& outEdges = instNode->getOutEdges();
                for(st::TNodeSet::const_iterator it = outEdges.begin(); it != outEdges.end(); ++it) {
                    st::ControlNode* outNode = *it;
                    if (outNode->cast<st::InstructionNode>()) {
                        st::TSmalltalkInstruction outInst = outNode->cast<st::InstructionNode>()->getInstruction();

                        if (inst.getArgument() == special::branch) {
                            SCOPED_TRACE("Unconditional branches shouldn't jump on terminators");
                            EXPECT_FALSE(outInst.isTerminator())
                                << "'" << inst.toString() << "' shouldn't jump on '" << outInst.toString() << "'";
                        } else {
                            SCOPED_TRACE("Conditional branches shouldn't jump on branches");
                            EXPECT_FALSE(outInst.isBranch())
                                << "'" << inst.toString() << "' shouldn't jump on '" << outInst.toString() << "'";
                        }
                    }
                }
            }
        }
        return true;
    }
};

class H_NonUniqueIncomingsOfPhi: public st::PlainNodeVisitor
{
public:
    H_NonUniqueIncomingsOfPhi(st::ControlGraph* graph) : st::PlainNodeVisitor(graph) {}
    virtual bool visitNode(st::ControlNode& node) {
        struct CompareIncoming {
            static bool cmp(const st::PhiNode::TIncoming& left, const st::PhiNode::TIncoming& right) {
                return left.node == right.node;
            }
        };

        if (st::PhiNode* phi = node.cast<st::PhiNode>()) {
            st::PhiNode::TIncomingList incomingList = phi->getIncomingList();
            st::PhiNode::TIncomingList::iterator uniqueEnd = std::unique(incomingList.begin(), incomingList.end(), CompareIncoming::cmp);
            std::size_t uniqueSize = std::distance(incomingList.begin(), uniqueEnd);
            EXPECT_EQ(uniqueSize, phi->getIncomingList().size()) << "The incomings of phi must differ between each other";
        }
        return true;
    }
};

void H_CheckCFGCorrect(st::ControlGraph* graph)
{
    {
        H_LastInstIsTerminator visitor(graph->getParsedMethod());
        visitor.run();
    }
    {
        H_DomainHasTerminator visitor(graph);
        visitor.run();
    }
    {
        H_AreBBsLinked visitor(graph);
        visitor.run();
    }
    {
        H_CorrectNumOfEdges visitor(graph);
        visitor.run();
    }
    {
        H_NoOrphans checker(graph);
        checker.check();
    }
    {
        H_ConsumeProvider visitor(graph);
        visitor.run();
    }
    {
    //    H_BranchJumpsOnCorrectNode visitor(graph);
    //    visitor.run();
    }
    {
        H_NonUniqueIncomingsOfPhi visitor(graph);
        visitor.run();
    }
}

#endif
