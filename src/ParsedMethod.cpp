#include <stapi.h>

using namespace st;

void ParsedMethod::parseBlock(uint16_t startOffset, uint16_t stopOffset) {
    // Following instruction belong to the nested code block
    // ParsedBlock will decode all of it's instructions and nested blocks
    ParsedBlock* const parsedBlock = new ParsedBlock(this, startOffset, stopOffset);
    addParsedBlock(parsedBlock);
}

void ParsedMethod::addParsedBlock(ParsedBlock* parsedBlock) {
    m_parsedBlocks.push_back(parsedBlock);

    const uint16_t startOffset = parsedBlock->getStartOffset();
    const uint16_t stopOffset  = parsedBlock->getStopOffset();
    m_offsetToParsedBlock[startOffset]   = parsedBlock;
    m_endOffsetToParsedBlock[stopOffset] = parsedBlock;
}

ParsedMethod::~ParsedMethod() {
    for (TParsedBlockList::iterator iBlock = m_parsedBlocks.begin(),
        end = m_parsedBlocks.end(); iBlock != end; ++iBlock)
    {
        delete * iBlock;
    }
}
