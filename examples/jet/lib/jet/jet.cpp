#include <monty.h>
#include <extend.h>
#include "jet.h"

using namespace monty;

//CG: module jet

void Gadget::marker () const {
    // ...
}

Flow::Flow () : _fanout ('N'), _wires ('H'), _index ('H') {
    printf("Flow %d b\n", (int) sizeof *this);
}

void Flow::marker () const {
    mark(_fanout);
    mark(_wires);
    mark(_index);
    markVec(_state);
}

Type Flow::info ("jet.flow");

//CG1 bind flow
static auto f_flow (ArgVec const& args) -> Value {
    //CG: args
    return new Flow;
}

//CG1 wrappers
static Lookup::Item const jet_map [] = {
};

//CG: module-end
