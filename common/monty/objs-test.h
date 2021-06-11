int created, destroyed, marked, failed;

struct MarkObj : Obj {
    MarkObj (Obj* o =0) : other (o) { ++created; }
    ~MarkObj () override               { ++destroyed; }

private:
    void marker () const override      { ++marked; mark(other); }

    Obj* other;
};

TEST_CASE("mem") {
    uint8_t memory [3*1024];
    objInit(memory, sizeof memory);
    uint32_t memAvail = gcMax();
    created = destroyed = marked = failed = 0;
    panicOutOfMemory = []() -> void* { ++failed; return nullptr; };

    SUBCASE("sizes") {
        CHECK(sizeof (void*) == sizeof (Obj));
    }

    SUBCASE("init") {
        CHECK(sizeof memory > gcMax());
        CHECK(sizeof memory - 50 < gcMax());
    }

    SUBCASE("new") {
        MarkObj o1; // on the stack
        CHECK(!o1.isCollectable());
        CHECK(sizeof (MarkObj) == sizeof o1);
        CHECK(memAvail == gcMax());

        auto p1 = new MarkObj; // allocated in pool
        CHECK(p1 != nullptr);
        CHECK(p1->isCollectable());

        auto avail1 = gcMax();
        CHECK(memAvail > avail1);

        auto p2 = new MarkObj; // second object in pool
        CHECK(p2->isCollectable());
        CHECK(p1 != p2);

        auto avail2 = gcMax();
        CHECK(avail1 > avail2);
        CHECK(memAvail - avail1 == avail1 - avail2);

        auto p3 = new (0) MarkObj; // same as without the extra size
        CHECK(p3->isCollectable());

        auto avail3 = gcMax();
        CHECK(avail2 > avail3);
        CHECK(avail1 - avail2 == avail2 - avail3);

        auto p4 = new (20) MarkObj; // extra space at end of object
        CHECK(p4->isCollectable());

        auto avail4 = gcMax();
        CHECK(avail3 > avail4);
        CHECK(avail1 - avail2 < avail3 - avail4);
    }

    SUBCASE("markObj") {
        auto p1 = new MarkObj;
        CHECK(1 == created);

        auto p2 = new MarkObj (p1);
        CHECK(2 == created);

        mark(p2);
        CHECK(2 == marked);

        mark(p2);
        CHECK(2 == marked);

        Obj::sweep();
        CHECK(0 == destroyed);

        mark(p2);
        CHECK(4 == marked); // now everything is marked again

        Obj::sweep();
        CHECK(0 == destroyed);
        Obj::sweep();
        CHECK(2 == destroyed);
    }

    SUBCASE("markThrough") {
        auto p1 = new MarkObj;
        MarkObj o1 (p1); // not allocated, but still traversed when marking
        auto p2 = new MarkObj (&o1);
        CHECK(3 == created);

        mark(p2);
        CHECK(3 == marked);

        mark(p2);
        CHECK(3 == marked);

        Obj::sweep();
        CHECK(0 == destroyed);
        Obj::sweep();
        CHECK(2 == destroyed);
    }

    SUBCASE("reuseObjs") {
        auto p1 = new MarkObj;          // [ p1 ]
        delete p1;                      // [ ]
        CHECK(memAvail == gcMax());

        auto p2 = new MarkObj;          // [ p2 ]
        /* p3: */ new MarkObj;          // [ p3 p2 ]
        delete p2;                      // [ p3 gap ]
        CHECK(p1 == p2);
        CHECK(memAvail > gcMax());

        auto p4 = new MarkObj;          // [ p3 p4 ]
        CHECK(p1 == p4);

        Obj::sweep();                // [ ]
        CHECK(memAvail == gcMax());
    }

    SUBCASE("mergeNext") {
        auto p1 = new MarkObj;
        auto p2 = new MarkObj;
        /* p3: */ new MarkObj;          // [ p3 p2 p1 ]

        delete p1;
        delete p2;                      // [ p3 gap ]

        auto p4 = new (1) MarkObj;      // [ p3 p4 ]
        (void) p4; // TODO changed: CHECK(p2 == p4);
    }

    SUBCASE("mergePrevious") {
        auto p1 = new MarkObj;
        auto p2 = new MarkObj;
        auto p3 = new MarkObj;          // [ p3 p2 p1 ]

        delete p2;                      // [ p3 gap p1 ]
        delete p1;                      // [ p3 gap gap ]

        auto p4 = new (1) MarkObj;      // [ p3 p4 ]
        // TODO changed: CHECK(p2 == p4);

        delete p4;                      // [ p3 gap ]
        delete p3;                      // [ ]
        CHECK(memAvail == gcMax());
    }

    SUBCASE("mergeMulti") {
        auto p1 = new (100) MarkObj;
        auto p2 = new (100) MarkObj;
        auto p3 = new (100) MarkObj;
        /* p4: */ new (100) MarkObj;    // [ p4 p3 p2 p1 ]

        delete p3;                      // [ p4 gap p2 p1 ]
        delete p2;                      // [ p4 gap gap p1 ]
        delete p1;                      // [ p4 gap gap gap ]

        auto p5 = new (300) MarkObj;    // [ p4 p5 ]
        (void) p5; // TODO changed: CHECK(p3 == p5);
    }

    SUBCASE("outOfObjs") {
#if 0 // can't test this anymore, since panic can't return
        constexpr auto N = 400;
        for (int i = 0; i < 12; ++i)
            new (N) MarkObj;
        CHECK(N > gcMax());
        CHECK(12 == created);
        CHECK(5 == failed);

        destroyed += failed; // avoid assertion in tearDown()
#endif
    }

    SUBCASE("gcRomOrRam") {
#if 0 // can't test on native
        struct RomObj {
            virtual void blah () {} // virtual is romable
        };
        static const RomObj romObj;
        static       RomObj ramObj;

        struct DataObj {
            virtual ~DataObj () {} // but not if virtual destructor (!)
        };
        static const DataObj dataObj;

        struct BssObj { // with plain destructor, it needs run-time init (?)
            ~BssObj () {}
        };
        static const BssObj bssObj;

        //extern int _etext [];
        extern int _sdata [];
        extern int _edata [];
        extern int _sbss [];
        extern int _ebss [];

        auto rom = (void*) &romObj;
        CHECK(rom < _sdata);                  // in flash, i.e. .rodata

        auto ram = (void*) &ramObj;                 // not const
        CHECK(_sdata <= ram && ram < _edata); // pre-inited, i.e. in .data

        auto data = (void*) &dataObj;
        CHECK(_sdata <= data && data < _edata);

        auto bss = (void*) &bssObj;
        CHECK(_sbss <= bss && bss < _ebss);
#endif

        // work around the fact that Obj::Obj() is protected
        Obj stackObj;
        auto heapObj = new Obj;

        auto heap = (void*) heapObj;
        CHECK(memory <= heap);
        CHECK(heap < memory + sizeof memory);

        auto stack = (void*) &stackObj;
        // heap is on the stack as well
        //CHECK(heap <= stack);
        CHECK(heap > stack);
    }

    Obj::sweep();
    CHECK(memAvail == gcMax());
}
