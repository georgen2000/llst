#ifndef LLST_VM_H_INCLUDED
#define LLST_VM_H_INCLUDED

#include <list>

#include <types.h>
#include <memory.h>
#include <stdlib.h>

class SmalltalkVM {
public:
    enum TExecuteResult {
        returnError = 2,
        returnBadMethod,
        returnReturned,
        returnTimeExpired,
        returnBreak,
        
        returnNoReturn = 255
    }; 
private:
    enum Opcode {
        extended = 0,
        pushInstance,
        pushArgument,    
        pushTemporary,   
        pushLiteral,     
        pushConstant,    
        assignInstance,  
        assignTemporary, 
        markArguments,   
        sendMessage,     
        sendUnary,       
        sendBinary,      
        pushBlock,       
        doPrimitive,     
        doSpecial = 15
    };
    
    enum Special {
        selfReturn = 1,
        stackReturn,
        blockReturn,
        duplicate,
        popTop,
        branch,
        branchIfTrue,
        branchIfFalse,
        sendToSuper = 11,
        breakpoint = 12
    };
    
    enum {
        nilConst = 10,
        trueConst,
        falseConst
    };
    
    enum TClassID {
        Object,
        Class,
        Method,
        Context,
        Process,
        Array,
        Dictionary,
        Block,
    };
    
    enum SmallIntOpcode {
        smallIntAdd = 10,
        smallIntDiv,
        smallIntMod,
        smallIntLess,
        smallIntEqual,
        smallIntMul,
        smallIntSub,
        smallIntBitOr = 36,
        smallIntBitAnd = 37,
        smallIntBitShift = 39
    };
    
    enum {
        ioGetChar = 9,
        ioPutChar = 3
    };
    
    enum {
        stringAt        = 21,
        stringAtPut     = 22,
        arrayAt         = 24,
        arrayAtPut      = 5
    };
    
    enum IntegerOpcode {
        integerDiv = 25,
        integerMod,
        integerAdd,
        integerMul,
        integerSub,
        integerLess,
        integerEqual,
    };
    
    enum {
        returnIsEqual     = 1,
        returnClass       = 2,
        returnSize        = 4,
        inAtPut           = 5,
        allocateObject    = 7,
        blockInvoke       = 8,
        allocateByteArray = 20,
        cloneByteObject   = 23
    };
    
    struct TMethodCacheEntry
    {
        TObject* methodName;
        TClass*  receiverClass;
        TMethod* method;
    };
    
    static const unsigned int LOOKUP_CACHE_SIZE = 4096;
    TMethodCacheEntry m_lookupCache[LOOKUP_CACHE_SIZE];
    uint32_t m_cacheHits;
    uint32_t m_cacheMisses;

    // lexicographic comparison of two byte objects
//     int compareSymbols(const TByteObject* left, const TByteObject* right);
    
    // locate the method in the hierarchy of the class
    TMethod* lookupMethod(TSymbol* selector, TClass* klass);
    
    // fast method lookup in the method cache
    TMethod* lookupMethodInCache(TSymbol* selector, TClass* klass);
    
    // flush the method lookup cache
    void flushMethodCache();
    
    void doPushConstant(uint8_t constant, TObjectArray& stack, uint32_t& stackTop);
    
    void doSendMessage(
        TSymbol* selector, 
        TObjectArray& arguments, 
        TContext* context, 
        uint32_t& stackTop);
    
    TObject* doExecutePrimitive(
        uint8_t opcode,
        uint8_t loArgument,
        TContext*& currentContext,
        TMethod*& currentMethod,
        TObjectArray& stack, 
        uint32_t& stackTop,
        uint32_t& bytePointer,
        TProcess& process);
    
    TExecuteResult doDoSpecial(
        TInstruction instruction, 
        TContext*& context, 
        uint32_t& stackTop,
        TMethod*& method,
        uint32_t& bytePointer,
        TProcess*& process,
        TObject*& returnedValue);
    
    // The result may be nil if the opcode execution fails (division by zero etc)
    TObject* doSmallInt(
        SmallIntOpcode opcode,
        uint32_t leftOperand,
        uint32_t rightOperand);
        
    void failPrimitive(
        TObjectArray& stack,
        uint32_t& stackTop);
    
    // TODO Think about other memory organization
    std::vector<TObject*> m_rootStack;
    
    Image*          m_image;
    IMemoryManager* m_memoryManager;
    
    void onCollectionOccured();
    
    TObject* newBinaryObject(TClass* klass, size_t dataSize);
    TObject* newOrdinaryObject(TClass* klass, size_t slotSize);
    
    // Helper functions for backTraceContext()
    void printByteObject(TByteObject* value);
    void printValue(uint32_t index, TObject* value);
    void printContents(TObjectArray& array);
    void backTraceContext(TContext* context);
public:    
    TExecuteResult execute(TProcess* process, uint32_t ticks);
    SmalltalkVM(Image* image, IMemoryManager* memoryManager) 
        : m_cacheHits(0), m_cacheMisses(0), m_image(image), m_memoryManager(memoryManager) {}
    
    template<class T> T* newObject(size_t dataSize = 0);
};

template<class T> T* SmalltalkVM::newObject(size_t dataSize /*= 0*/)
{
    // TODO fast access to common classes
    TClass* klass = (TClass*) m_image->getGlobal(T::InstanceClassName());
    if (!klass)
        return (T*) globals.nilObject;
    
    if (T::InstancesAreBinary()) {   
        return (T*) newBinaryObject(klass, dataSize);
    } else {
        size_t slotSize = sizeof(T) + dataSize * sizeof(T*);
        return (T*) newOrdinaryObject(klass, slotSize);
    }
}

// Specializations of newObject for known types
template<> TObjectArray* SmalltalkVM::newObject<TObjectArray>(size_t dataSize /*= 0*/);
template<> TContext* SmalltalkVM::newObject<TContext>(size_t dataSize /*= 0*/);


#endif
