// pyvm.h - virtual machine for bytecodes emitted by MicroPython 1.12

#define SHOW_INSTR_PTR 0 // show instr ptr each time through loop (in pyvm.h)
//CG: off op:print # set to "on" to enable per-opcode debug output

#ifndef INNER_HOOK
#define INNER_HOOK
#endif

namespace Monty {

class PyVM : public Interp {
    enum Op : uint8_t {
        //CG< opcodes ../../git/micropython/py/bc0.h
        LoadConstString        = 0x10,
        LoadName               = 0x11,
        LoadGlobal             = 0x12,
        LoadAttr               = 0x13,
        LoadMethod             = 0x14,
        LoadSuperMethod        = 0x15,
        StoreName              = 0x16,
        StoreGlobal            = 0x17,
        StoreAttr              = 0x18,
        DeleteName             = 0x19,
        DeleteGlobal           = 0x1A,
        ImportName             = 0x1B,
        ImportFrom             = 0x1C,
        MakeClosure            = 0x20,
        MakeClosureDefargs     = 0x21,
        LoadConstSmallInt      = 0x22,
        LoadConstObj           = 0x23,
        LoadFastN              = 0x24,
        LoadDeref              = 0x25,
        StoreFastN             = 0x26,
        StoreDeref             = 0x27,
        DeleteFast             = 0x28,
        DeleteDeref            = 0x29,
        BuildTuple             = 0x2A,
        BuildList              = 0x2B,
        BuildMap               = 0x2C,
        BuildSet               = 0x2D,
        BuildSlice             = 0x2E,
        StoreComp              = 0x2F,
        UnpackSequence         = 0x30,
        UnpackEx               = 0x31,
        MakeFunction           = 0x32,
        MakeFunctionDefargs    = 0x33,
        CallFunction           = 0x34,
        CallFunctionVarKw      = 0x35,
        CallMethod             = 0x36,
        CallMethodVarKw        = 0x37,
        UnwindJump             = 0x40,
        Jump                   = 0x42,
        PopJumpIfTrue          = 0x43,
        PopJumpIfFalse         = 0x44,
        JumpIfTrueOrPop        = 0x45,
        JumpIfFalseOrPop       = 0x46,
        SetupWith              = 0x47,
        SetupExcept            = 0x48,
        SetupFinally           = 0x49,
        PopExceptJump          = 0x4A,
        ForIter                = 0x4B,
        LoadConstFalse         = 0x50,
        LoadConstNone          = 0x51,
        LoadConstTrue          = 0x52,
        LoadNull               = 0x53,
        LoadBuildClass         = 0x54,
        LoadSubscr             = 0x55,
        StoreSubscr            = 0x56,
        DupTop                 = 0x57,
        DupTopTwo              = 0x58,
        PopTop                 = 0x59,
        RotTwo                 = 0x5A,
        RotThree               = 0x5B,
        WithCleanup            = 0x5C,
        EndFinally             = 0x5D,
        GetIter                = 0x5E,
        GetIterStack           = 0x5F,
        StoreMap               = 0x62,
        ReturnValue            = 0x63,
        RaiseLast              = 0x64,
        RaiseObj               = 0x65,
        RaiseFrom              = 0x66,
        YieldValue             = 0x67,
        YieldFrom              = 0x68,
        ImportStar             = 0x69,
        //CG>
        LoadConstSmallIntMulti = 0x70,
        LoadFastMulti          = 0xB0,
        StoreFastMulti         = 0xC0,
        UnaryOpMulti           = 0xD0,
        BinaryOpMulti          = 0xD7,
    };

    Value* sp {nullptr};
    uint8_t const* ip {nullptr};

    uint32_t fetchV (uint32_t v =0) {
        uint8_t b = 0x80;
        while (b & 0x80) {
            b = *ip++;
            v = (v << 7) | (b & 0x7F);
        }
        return v;
    }

    int fetchO () {
        int n = *ip++;
        return n | (*ip++ << 8);
    }

    char const* fetchQ () {
        return context->callee->qStrAt(fetchO() + 1);
    }

    // special wrapper to deal with context changes vs cached sp/ip values
    template< typename T >
    auto contextAdjuster (T fun) -> Value {
        context->spOff = sp - context->begin();
        context->ipOff = ip - context->ipBase();
        Value v = fun();
        if (context != nullptr) {
            sp = context->begin() + context->spOff;
            ip = context->ipBase() + context->ipOff;
        } else
            interrupt(0); // nothing left to do, exit inner loop
        return v;
    }

    void instructionTrace () {
#if SHOW_INSTR_PTR
        printf("\tip %04d sp %2d e %d ", (int) (ip - context->ipBase()),
                                         (int) (sp - context->spBase()),
                                         (int) frame().ep);
        printf("op 0x%02x : ", *ip);
        if (sp >= context->spBase())
            sp->dump();
        printf("\n");
#endif
        assert(ip >= context->ipBase() && sp >= context->spBase() - 1);
        INNER_HOOK
    }

    // check and trigger gc on backwards jumps, i.e. inside all loops
    static void loopCheck (int arg) {
        if (arg < 0 && gcCheck())
            interrupt(0);
    }

    //CG: op-init

    //CG1 op
    void opLoadNull () {
        *++sp = {};
    }
    //CG1 op
    void opLoadConstNone () {
        *++sp = Null;
    }
    //CG1 op
    void opLoadConstFalse () {
        *++sp = False;
    }
    //CG1 op
    void opLoadConstTrue () {
        *++sp = True;
    }
    //CG1 op q
    void opLoadConstString (char const* arg) {
        *++sp = arg;
    }
    //CG1 op
    void opLoadConstSmallInt () {
        *++sp = fetchV((*ip & 0x40) ? ~0 : 0);
    }
    //CG1 op v
    void opLoadConstObj (int arg) {
        *++sp = context->callee->code.constAt(arg);
    }
    //CG1 op v
    void opLoadFastN (int arg) {
        *++sp = context->fastSlot(arg);
    }
    //CG1 op v
    void opStoreFastN (int arg) {
        context->fastSlot(arg) = *sp--;
    }
    //CG1 op v
    void opDeleteFast (int arg) {
        context->fastSlot(arg) = {};
    }

    //CG1 op
    void opDupTop () {
        ++sp; sp[0] = sp[-1];
    }
    //CG1 op
    void opDupTopTwo () {
        sp += 2; sp[0] = sp[-2]; sp[-1] = sp[-3];
    }
    //CG1 op
    void opPopTop () {
        --sp;
    }
    //CG1 op
    void opRotTwo () {
        auto v = sp[0]; sp[0] = sp[-1]; sp[-1] = v;
    }
    //CG1 op
    void opRotThree () {
        auto v = sp[0]; sp[0] = sp[-1]; sp[-1] = sp[-2]; sp[-2] = v;
    }

    //CG1 op s
    void opJump (int arg) {
        ip += arg;
        loopCheck(arg);
    }
    //CG1 op s
    void opPopJumpIfFalse (int arg) {
        if (!sp->truthy()) {
            ip += arg;
            loopCheck(arg);
        }
        --sp;
    }
    //CG1 op s
    void opJumpIfFalseOrPop (int arg) {
        if (!sp->truthy()) {
            ip += arg;
            loopCheck(arg);
        } else
            --sp;
    }
    //CG1 op s
    void opPopJumpIfTrue (int arg) {
        if (sp->truthy()) {
            ip += arg;
            loopCheck(arg);
        }
        --sp;
    }
    //CG1 op s
    void opJumpIfTrueOrPop (int arg) {
        if (sp->truthy()) {
            ip += arg;
            loopCheck(arg);
        } else
            --sp;
    }

    //CG1 op q
    void opLoadName (char const* arg) {
        assert(frame().locals.isObj());
        *++sp = frame().locals.obj().getAt(arg);
        assert(!sp->isNil());
    }
    //CG1 op q
    void opStoreName (char const* arg) {
        auto& l = frame().locals;
        if (!l.isObj())
            l = new Dict (&context->globals());
        l.obj().setAt(arg, *sp--);
    }
    //CG1 op q
    void opDeleteName (char const* arg) {
        assert(frame().locals.isObj());
        frame().locals.obj().setAt(arg, {});
    }
    //CG1 op q
    void opLoadGlobal (char const* arg) {
        *++sp = context->globals().at(arg);
        assert(!sp->isNil());
    }
    //CG1 op q
    void opStoreGlobal (char const* arg) {
        context->globals().at(arg) = *sp--;
    }
    //CG1 op q
    void opDeleteGlobal (char const* arg) {
        context->globals().at(arg) = {};
    }
    //CG1 op q
    void opLoadAttr (char const* arg) {
        Value self;
        *sp = sp->obj().attr(arg, self);
        assert(!sp->isNil());
        // TODO should this be moved into Inst::attr ???
        auto f = sp->ifType<Callable>();
        if (!self.isNil() && f != 0)
            *sp = new BoundMeth (*f, self);
    }
    //CG1 op q
    void opStoreAttr (char const* arg) {
        assert(&sp->obj().type().type() == &Class::info);
        sp->obj().setAt(arg, sp[-1]);
        sp -= 2;
    }
    //CG1 op
    void opLoadSubscr () {
        --sp;
        *sp = sp->asObj().getAt(sp[1]);
    }
    //CG1 op
    void opStoreSubscr () {
        --sp; // val [obj] key
        assert(sp->isObj());
        sp->obj().setAt(sp[1], sp[-1]);
        sp -= 2;
    }

    //CG1 op v
    void opBuildSlice (int arg) {
        sp -= arg - 1;
        *sp = Slice::create({*context, arg, (int) (sp - context->begin())});
    }
    //CG1 op v
    void opBuildTuple (int arg) {
        sp -= arg - 1;
        *sp = Tuple::create({*context, arg, (int) (sp - context->begin())});
    }
    //CG1 op v
    void opBuildList (int arg) {
        sp -= arg - 1;
        *sp = List::create({*context, arg, (int) (sp - context->begin())});
    }
    //CG1 op v
    void opBuildSet (int arg) {
        sp -= arg - 1;
        *sp = Set::create({*context, arg, (int) (sp - context->begin())});
    }
    //CG1 op v
    void opBuildMap (int arg) {
        *++sp = Dict::create({*context, arg, 0});
    }
    //CG1 op
    void opStoreMap () {
        sp -= 2;
        sp->obj().setAt(sp[2], sp[1]); // TODO optimise later: no key check
    }

    //CG1 op o
    void opSetupExcept (int arg) {
        auto exc = context->excBase(1);
        exc[0] = ip - context->ipBase() + arg;
        exc[1] = sp - context->begin();
        exc[2] = {};
    }
    //CG1 op o
    void opPopExceptJump (int arg) {
        context->excBase(-1);
        ip += arg;
    }
    //CG1 op
    void opRaiseLast () {
        context->raise(""); // TODO
    }
    //CG1 op
    void opRaiseObj () {
        context->raise(*sp);
    }
    //CG1 op
    void opRaiseFrom () {
        context->raise(*--sp); // exception chaining is not supported
    }
    //CG1 op s
    void opUnwindJump (int arg) {
        int ep = frame().ep;
        frame().ep = ep - *ip; // TODO hardwired for simplest case
        ip += arg;
        loopCheck(arg);
    }

    //CG1 op
    void opLoadBuildClass () {
        *++sp = Class::info;
    }
    //CG1 op q
    void opLoadMethod (char const* arg) {
        sp[1] = *sp;
        *sp = sp->asObj().attr(arg, sp[1]);
        assert(!sp->isNil());
        ++sp;
        assert(!sp->isNil());
    }
    //CG1 op q
    void opLoadSuperMethod (char const* arg) {
        (void) arg; assert(false); // TODO
    }
    //CG1 op v
    void opCallMethod (int arg) {
        uint8_t nargs = arg, nkw = arg >> 8;
        sp -= nargs + 2 * nkw + 1;
        ArgVec avec = {*context, arg + 1, (int) (sp + 1 - context->begin())};
        auto v = contextAdjuster([=]() -> Value {
            return sp->obj().call(avec);
        });
        *sp = v;
    }
    //CG1 op v
    void opCallMethodVarKw (int arg) {
        (void) arg; assert(false); // TODO
    }
    //CG1 op v
    void opMakeFunction (int arg) {
        auto bc = context->callee->code.constAt(arg);
        *++sp = new Callable (bc);
    }
    //CG1 op v
    void opMakeFunctionDefargs (int arg) {
        auto bc = context->callee->code.constAt(arg);
        --sp;
        *sp = new Callable (bc, sp[0], sp[1]);
    }
    //CG1 op v
    void opCallFunction (int arg) {
        uint8_t nargs = arg, nkw = arg >> 8;
        sp -= nargs + 2 * nkw;
        ArgVec avec {*context, arg, (int) (sp + 1 - context->begin())};
        auto v = contextAdjuster([=]() -> Value {
            return sp->obj().call(avec);
        });
        *sp = v;
    }
    //CG1 op v
    void opCallFunctionVarKw (int arg) {
        (void) arg; assert(false); // TODO
    }

    //CG1 op v
    void opMakeClosure (int arg) {
        int num = *ip++;
        sp -= num - 1;
        auto bc = context->callee->code.constAt(arg);
        auto f = new Callable (bc);
        ArgVec avec {*context, num, (int) (sp - context->begin())};
        *sp = new Closure (*f, avec);
    }
    //CG1 op v
    void opMakeClosureDefargs (int arg) {
        int num = *ip++;
        sp -= 2 + num - 1;
        auto bc = context->callee->code.constAt(arg);
        auto f = new Callable (bc, sp[0], sp[1]);
        ArgVec avec {*context, num, (int) (sp + 2 - context->begin())};
        *sp = new Closure (*f, avec);
    }
    //CG1 op v
    void opLoadDeref (int arg) {
        *++sp = context->derefSlot(arg);
        assert(!sp->isNil());
    }
    //CG1 op v
    void opStoreDeref (int arg) {
        context->derefSlot(arg) = *sp--;
    }
    //CG1 op v
    void opDeleteDeref (int arg) {
        context->derefSlot(arg) = {};
    }

    //CG1 op
    void opYieldValue () {
        auto ctx = context;
        auto v = contextAdjuster([=]() -> Value {
            context = context->caller;
            return *sp;
        });
        ctx->caller = nullptr;
        *sp = v;
    }
    //CG1 op
    void opReturnValue () {
        auto v = contextAdjuster([=]() -> Value {
            return context->leave(*sp);
        });
        *sp = v;
    }

    //CG1 op
    void opGetIter () {
        assert(false); // TODO
    }
    //CG1 op
    void opGetIterStack () {
        // TODO yuck, the compiler assumes 4 stack entries are used!
        //  layout [seq,idx,nil,nil]
        *++sp = 0;
        *++sp = {};
        *++sp = {};
    }
    //CG1 op o
    void opForIter (int arg) {
        assert(sp->isNil());
        int n = sp[-2];
        Value v = sp[-3].obj().getAt(n++);
        if (v.isNil()) {
            sp -= 4;
            ip += arg;
        } else {
            sp[-2] = n;
            *++sp = v;
        }
    }

    //CG1 op m 64
    void opLoadConstSmallIntMulti (uint32_t arg) {
        *++sp = arg - 16;
    }
    //CG1 op m 16
    void opLoadFastMulti (uint32_t arg) {
        *++sp = context->fastSlot(arg);
        assert(!sp->isNil());
    }
    //CG1 op m 16
    void opStoreFastMulti (uint32_t arg) {
        context->fastSlot(arg) = *sp--;
    }
    //CG1 op m 7
    void opUnaryOpMulti (uint32_t arg) {
        *sp = sp->unOp((UnOp) arg);
    }
    //CG1 op m 35
    void opBinaryOpMulti (uint32_t arg) {
        --sp;
        *sp = sp->binOp((BinOp) arg, sp[1]);
    }

    void inner () {
        sp = context->begin() + context->spOff;
        ip = context->ipBase() + context->ipOff;

        do {
            instructionTrace();
            switch ((Op) *ip++) {

                //CG< op-emit d
                case Op::LoadNull:
                    opLoadNull();
                    break;
                case Op::LoadConstNone:
                    opLoadConstNone();
                    break;
                case Op::LoadConstFalse:
                    opLoadConstFalse();
                    break;
                case Op::LoadConstTrue:
                    opLoadConstTrue();
                    break;
                case Op::LoadConstString: {
                    char const* arg = fetchQ();
                    opLoadConstString(arg);
                    break;
                }
                case Op::LoadConstSmallInt:
                    opLoadConstSmallInt();
                    break;
                case Op::LoadConstObj: {
                    int arg = fetchV();
                    opLoadConstObj(arg);
                    break;
                }
                case Op::LoadFastN: {
                    int arg = fetchV();
                    opLoadFastN(arg);
                    break;
                }
                case Op::StoreFastN: {
                    int arg = fetchV();
                    opStoreFastN(arg);
                    break;
                }
                case Op::DeleteFast: {
                    int arg = fetchV();
                    opDeleteFast(arg);
                    break;
                }
                case Op::DupTop:
                    opDupTop();
                    break;
                case Op::DupTopTwo:
                    opDupTopTwo();
                    break;
                case Op::PopTop:
                    opPopTop();
                    break;
                case Op::RotTwo:
                    opRotTwo();
                    break;
                case Op::RotThree:
                    opRotThree();
                    break;
                case Op::Jump: {
                    int arg = fetchO()-0x8000;
                    opJump(arg);
                    break;
                }
                case Op::PopJumpIfFalse: {
                    int arg = fetchO()-0x8000;
                    opPopJumpIfFalse(arg);
                    break;
                }
                case Op::JumpIfFalseOrPop: {
                    int arg = fetchO()-0x8000;
                    opJumpIfFalseOrPop(arg);
                    break;
                }
                case Op::PopJumpIfTrue: {
                    int arg = fetchO()-0x8000;
                    opPopJumpIfTrue(arg);
                    break;
                }
                case Op::JumpIfTrueOrPop: {
                    int arg = fetchO()-0x8000;
                    opJumpIfTrueOrPop(arg);
                    break;
                }
                case Op::LoadName: {
                    char const* arg = fetchQ();
                    opLoadName(arg);
                    break;
                }
                case Op::StoreName: {
                    char const* arg = fetchQ();
                    opStoreName(arg);
                    break;
                }
                case Op::DeleteName: {
                    char const* arg = fetchQ();
                    opDeleteName(arg);
                    break;
                }
                case Op::LoadGlobal: {
                    char const* arg = fetchQ();
                    opLoadGlobal(arg);
                    break;
                }
                case Op::StoreGlobal: {
                    char const* arg = fetchQ();
                    opStoreGlobal(arg);
                    break;
                }
                case Op::DeleteGlobal: {
                    char const* arg = fetchQ();
                    opDeleteGlobal(arg);
                    break;
                }
                case Op::LoadAttr: {
                    char const* arg = fetchQ();
                    opLoadAttr(arg);
                    break;
                }
                case Op::StoreAttr: {
                    char const* arg = fetchQ();
                    opStoreAttr(arg);
                    break;
                }
                case Op::LoadSubscr:
                    opLoadSubscr();
                    break;
                case Op::StoreSubscr:
                    opStoreSubscr();
                    break;
                case Op::BuildSlice: {
                    int arg = fetchV();
                    opBuildSlice(arg);
                    break;
                }
                case Op::BuildTuple: {
                    int arg = fetchV();
                    opBuildTuple(arg);
                    break;
                }
                case Op::BuildList: {
                    int arg = fetchV();
                    opBuildList(arg);
                    break;
                }
                case Op::BuildSet: {
                    int arg = fetchV();
                    opBuildSet(arg);
                    break;
                }
                case Op::BuildMap: {
                    int arg = fetchV();
                    opBuildMap(arg);
                    break;
                }
                case Op::StoreMap:
                    opStoreMap();
                    break;
                case Op::SetupExcept: {
                    int arg = fetchO();
                    opSetupExcept(arg);
                    break;
                }
                case Op::PopExceptJump: {
                    int arg = fetchO();
                    opPopExceptJump(arg);
                    break;
                }
                case Op::RaiseLast:
                    opRaiseLast();
                    break;
                case Op::RaiseObj:
                    opRaiseObj();
                    break;
                case Op::RaiseFrom:
                    opRaiseFrom();
                    break;
                case Op::UnwindJump: {
                    int arg = fetchO()-0x8000;
                    opUnwindJump(arg);
                    break;
                }
                case Op::LoadBuildClass:
                    opLoadBuildClass();
                    break;
                case Op::LoadMethod: {
                    char const* arg = fetchQ();
                    opLoadMethod(arg);
                    break;
                }
                case Op::LoadSuperMethod: {
                    char const* arg = fetchQ();
                    opLoadSuperMethod(arg);
                    break;
                }
                case Op::CallMethod: {
                    int arg = fetchV();
                    opCallMethod(arg);
                    break;
                }
                case Op::CallMethodVarKw: {
                    int arg = fetchV();
                    opCallMethodVarKw(arg);
                    break;
                }
                case Op::MakeFunction: {
                    int arg = fetchV();
                    opMakeFunction(arg);
                    break;
                }
                case Op::MakeFunctionDefargs: {
                    int arg = fetchV();
                    opMakeFunctionDefargs(arg);
                    break;
                }
                case Op::CallFunction: {
                    int arg = fetchV();
                    opCallFunction(arg);
                    break;
                }
                case Op::CallFunctionVarKw: {
                    int arg = fetchV();
                    opCallFunctionVarKw(arg);
                    break;
                }
                case Op::MakeClosure: {
                    int arg = fetchV();
                    opMakeClosure(arg);
                    break;
                }
                case Op::MakeClosureDefargs: {
                    int arg = fetchV();
                    opMakeClosureDefargs(arg);
                    break;
                }
                case Op::LoadDeref: {
                    int arg = fetchV();
                    opLoadDeref(arg);
                    break;
                }
                case Op::StoreDeref: {
                    int arg = fetchV();
                    opStoreDeref(arg);
                    break;
                }
                case Op::DeleteDeref: {
                    int arg = fetchV();
                    opDeleteDeref(arg);
                    break;
                }
                case Op::YieldValue:
                    opYieldValue();
                    break;
                case Op::ReturnValue:
                    opReturnValue();
                    break;
                case Op::GetIter:
                    opGetIter();
                    break;
                case Op::GetIterStack:
                    opGetIterStack();
                    break;
                case Op::ForIter: {
                    int arg = fetchO();
                    opForIter(arg);
                    break;
                }
                //CG>

                // TODO
                case Op::EndFinally:
                case Op::ImportFrom:
                case Op::ImportName:
                case Op::ImportStar:
                case Op::SetupFinally:
                case Op::SetupWith:
                case Op::StoreComp:
                case Op::UnpackEx:
                case Op::UnpackSequence:
                case Op::WithCleanup:
                case Op::YieldFrom:

                default: {
                    //CG< op-emit m
                    if ((uint32_t) (ip[-1] - Op::LoadConstSmallIntMulti) < 64) {
                        uint32_t arg = ip[-1] - Op::LoadConstSmallIntMulti;
                        opLoadConstSmallIntMulti(arg);
                        break;
                    }
                    if ((uint32_t) (ip[-1] - Op::LoadFastMulti) < 16) {
                        uint32_t arg = ip[-1] - Op::LoadFastMulti;
                        opLoadFastMulti(arg);
                        break;
                    }
                    if ((uint32_t) (ip[-1] - Op::StoreFastMulti) < 16) {
                        uint32_t arg = ip[-1] - Op::StoreFastMulti;
                        opStoreFastMulti(arg);
                        break;
                    }
                    if ((uint32_t) (ip[-1] - Op::UnaryOpMulti) < 7) {
                        uint32_t arg = ip[-1] - Op::UnaryOpMulti;
                        opUnaryOpMulti(arg);
                        break;
                    }
                    if ((uint32_t) (ip[-1] - Op::BinaryOpMulti) < 35) {
                        uint32_t arg = ip[-1] - Op::BinaryOpMulti;
                        opBinaryOpMulti(arg);
                        break;
                    }
                    //CG>
                    assert(false);
                }
            }

        } while (pending == 0);

        if (context == nullptr)
            return; // last frame popped, there's no context left

        context->spOff = sp - context->begin();
        context->ipOff = ip - context->ipBase();

        if (pendingBit(0))
            context->caught();
    }

    void outer () {
        while (true) {
            INNER_HOOK              // optional, to simulate interrupts

            auto irq = nextPending();
            if (irq >= 0) {         // there's a pending soft-irq
                auto h = handlers[irq];
                if (h.isObj())
                    h.obj().next(); // resume the triggered handler
            }

            if (context == nullptr)
                break;              // no runnable context left

            inner();                // go process lots of bytecodes

            if (gcCheck()) {
                archMode(RunMode::GC);
                markAll();
                sweep();
                compact();
                archMode(RunMode::Run);
            }
        }
    }

public:
    PyVM (Callable const& init) {
        auto ctx = new Monty::Context;
        ctx->enter(init);
        ctx->frame().locals = &init.mo;

        tasks.insert(0);
        tasks[0] = ctx;
    }

    void runner () {
        while (true) {
            outer();
            if (tasks.len() == 0)
                break;
            auto& ctx = tasks.getAt(0).asType<Context>();
            resume(ctx);
        }
        // no current context and no runnable tasks at this point
    }
};

} // namespace Monty
