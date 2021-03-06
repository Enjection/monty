struct AltPins { uint8_t pin, dev :4, alt :4; };

constexpr auto Pin (char const* s) -> uint8_t {
    int r = *s++ - 'A', n = 0;
    while ('0' <= *s && *s <= '9')
        n = 10 * n + *s++ - '0';
    return (r << 4) | (n & 0x1F);
}

template< uint32_t N >
constexpr auto findAlt (AltPins const (&map) [N], int pin, int dev) -> int {
    for (auto e : map)
        if (pin == e.pin && dev == e.dev)
            return e.alt;
    return -1;
}

template< uint32_t N >
constexpr auto findAlt (AltPins const (&map) [N], char const* name, int dev) {
    return findAlt(map, Pin(name), dev);
}

#if STM32F0
AltPins const altTX [] = {
    { Pin("A0" ), 4, 4 }, { Pin("A2" ), 1, 1 }, { Pin("A2" ), 2, 1 },
    { Pin("A4" ), 6, 5 }, { Pin("A9" ), 1, 1 }, { Pin("A14"), 1, 1 },
    { Pin("A14"), 2, 1 }, { Pin("B3" ), 5, 4 }, { Pin("B6" ), 1, 0 },
    { Pin("B10"), 3, 4 }, { Pin("C0" ), 6, 2 }, { Pin("C0" ), 7, 1 },
    { Pin("C2" ), 8, 2 }, { Pin("C4" ), 3, 1 }, { Pin("C6" ), 7, 1 },
    { Pin("C8" ), 8, 1 }, { Pin("C10"), 3, 1 }, { Pin("C10"), 4, 0 },
    { Pin("C12"), 5, 2 }, { Pin("D5" ), 2, 0 }, { Pin("D8" ), 3, 0 },
    { Pin("D13"), 8, 0 }, { Pin("E8" ), 4, 1 }, { Pin("E10"), 5, 1 },
    { Pin("F2" ), 7, 1 }, { Pin("F9" ), 6, 1 },
};
AltPins const altRX [] = {
    { Pin("A1" ), 4, 4 }, { Pin("A3" ), 1, 1 }, { Pin("A3" ), 2, 1 },
    { Pin("A5" ), 6, 5 }, { Pin("A10"), 1, 1 }, { Pin("A15"), 1, 1 },
    { Pin("A15"), 2, 1 }, { Pin("B4" ), 5, 4 }, { Pin("B7" ), 1, 0 },
    { Pin("B11"), 3, 4 }, { Pin("C1" ), 6, 2 }, { Pin("C1" ), 7, 1 },
    { Pin("C3" ), 8, 2 }, { Pin("C5" ), 3, 1 }, { Pin("C7" ), 7, 1 },
    { Pin("C9" ), 8, 1 }, { Pin("C11"), 3, 1 }, { Pin("C11"), 4, 0 },
    { Pin("D2" ), 5, 2 }, { Pin("D6" ), 2, 0 }, { Pin("D9" ), 3, 0 },
    { Pin("D14"), 8, 0 }, { Pin("E9" ), 4, 1 }, { Pin("E11"), 5, 1 },
    { Pin("F3" ), 7, 1 }, { Pin("F10"), 6, 1 },
};
AltPins const altMOSI [] = {
    { Pin("A7" ), 1, 0 }, { Pin("B5" ), 1, 0 }, { Pin("B15"), 1, 0 },
    { Pin("B15"), 2, 0 }, { Pin("C3" ), 2, 1 }, { Pin("D4" ), 2, 1 },
    { Pin("E15"), 1, 1 },
};
AltPins const altMISO [] = {
    { Pin("A6" ), 1, 0 }, { Pin("B4" ), 1, 0 }, { Pin("B14"), 1, 0 },
    { Pin("B14"), 2, 0 }, { Pin("C2" ), 2, 1 }, { Pin("D3" ), 2, 1 },
    { Pin("E14"), 1, 1 },
};
AltPins const altSCK [] = {
    { Pin("A5" ), 1, 0 }, { Pin("B3" ), 1, 0 }, { Pin("B10"), 2, 5 },
    { Pin("B13"), 1, 0 }, { Pin("B13"), 2, 0 }, { Pin("D1" ), 2, 1 },
    { Pin("E13"), 1, 1 },
};
AltPins const altNSS [] = {
    { Pin("A4" ), 1, 0 }, { Pin("A15"), 1, 0 }, { Pin("B9" ), 2, 5 },
    { Pin("B12"), 1, 0 }, { Pin("B12"), 2, 0 }, { Pin("D0" ), 2, 1 },
    { Pin("E12"), 1, 1 },
};
#endif

#if STM32F2
AltPins const altTX [] = {
    { Pin("A0" ), 4, 8 }, { Pin("A2" ), 2, 7 }, { Pin("A9" ), 1, 7 },
    { Pin("B6" ), 1, 7 }, { Pin("B10"), 3, 7 }, { Pin("C6" ), 6, 8 },
    { Pin("C10"), 3, 7 }, { Pin("C10"), 4, 8 }, { Pin("C12"), 5, 8 },
    { Pin("D5" ), 2, 7 }, { Pin("D8" ), 3, 7 }, { Pin("G14"), 6, 8 },
};
AltPins const altRX [] = {
    { Pin("A1" ), 4, 8 }, { Pin("A3" ), 2, 7 }, { Pin("A10"), 1, 7 },
    { Pin("B7" ), 1, 7 }, { Pin("B11"), 3, 7 }, { Pin("C7" ), 6, 8 },
    { Pin("C11"), 3, 7 }, { Pin("C11"), 4, 8 }, { Pin("D2" ), 5, 8 },
    { Pin("D6" ), 2, 7 }, { Pin("D9" ), 3, 7 }, { Pin("G9" ), 6, 8 },
};
AltPins const altMOSI [] = {
    { Pin("A7" ), 1, 5 }, { Pin("B5" ), 1, 5 }, { Pin("B5" ), 3, 6 },
    { Pin("B15"), 2, 5 }, { Pin("C3" ), 2, 5 }, { Pin("C12"), 3, 6 },
    { Pin("I3" ), 2, 5 },
};
AltPins const altMISO [] = {
    { Pin("A6" ), 1, 5 }, { Pin("B4" ), 1, 5 }, { Pin("B4" ), 3, 6 },
    { Pin("B14"), 2, 5 }, { Pin("C2" ), 2, 5 }, { Pin("C11"), 3, 6 },
    { Pin("I2" ), 2, 5 },
};
AltPins const altSCK [] = {
    { Pin("A5" ), 1, 5 }, { Pin("B3" ), 1, 5 }, { Pin("B3" ), 3, 6 },
    { Pin("B10"), 2, 5 }, { Pin("B13"), 2, 5 }, { Pin("C10"), 3, 6 },
    { Pin("I1" ), 2, 5 },
};
AltPins const altNSS [] = {
    { Pin("A4" ), 1, 5 }, { Pin("A4" ), 3, 6 }, { Pin("A15"), 1, 5 },
    { Pin("A15"), 3, 6 }, { Pin("B9" ), 2, 5 }, { Pin("B12"), 2, 5 },
    { Pin("I0" ), 2, 5 },
};
#endif

#if STM32F3
AltPins const altTX [] = {
    { Pin("A2" ), 2, 7 }, { Pin("A9" ), 1, 7 }, { Pin("A14"), 2, 7 },
    { Pin("B3" ), 2, 7 }, { Pin("B6" ), 1, 7 }, { Pin("B8" ), 3, 7 },
    { Pin("B9" ), 3, 7 }, { Pin("B10"), 3, 7 }, { Pin("C4" ), 1, 7 },
    { Pin("C10"), 3, 7 }, { Pin("C10"), 4, 5 }, { Pin("C12"), 5, 5 },
    { Pin("D5" ), 2, 7 }, { Pin("D8" ), 3, 7 }, { Pin("E0" ), 1, 7 },
};
AltPins const altRX [] = {
    { Pin("A3" ), 2, 7 }, { Pin("A10"), 1, 7 }, { Pin("A15"), 2, 7 },
    { Pin("B4" ), 2, 7 }, { Pin("B7" ), 1, 7 }, { Pin("B8" ), 3, 7 },
    { Pin("B9" ), 3, 7 }, { Pin("B11"), 3, 7 }, { Pin("C5" ), 1, 7 },
    { Pin("C11"), 3, 7 }, { Pin("C11"), 4, 5 }, { Pin("D2" ), 5, 5 },
    { Pin("D6" ), 2, 7 }, { Pin("D9" ), 3, 7 }, { Pin("E1" ), 1, 7 },
    { Pin("E15"), 3, 7 },
};
AltPins const altMOSI [] = {
    { Pin("A3" ), 3, 6 }, { Pin("A7" ), 1, 5 }, { Pin("A10"), 2, 5 },
    { Pin("A11"), 2, 5 }, { Pin("B0" ), 1, 5 }, { Pin("B5" ), 1, 5 },
    { Pin("B5" ), 3, 6 }, { Pin("B15"), 2, 5 }, { Pin("C3" ), 2, 5 },
    { Pin("C9" ), 1, 5 }, { Pin("C12"), 3, 6 }, { Pin("D4" ), 2, 5 },
    { Pin("E6" ), 4, 5 }, { Pin("E14"), 4, 5 }, { Pin("F6" ), 1, 5 },
};
AltPins const altMISO [] = {
    { Pin("A2" ), 3, 6 }, { Pin("A6" ), 1, 5 }, { Pin("A9" ), 2, 5 },
    { Pin("A10"), 2, 5 }, { Pin("A13"), 1, 6 }, { Pin("B4" ), 1, 5 },
    { Pin("B4" ), 3, 6 }, { Pin("B14"), 2, 5 }, { Pin("C2" ), 2, 5 },
    { Pin("C8" ), 1, 5 }, { Pin("C11"), 3, 6 }, { Pin("D3" ), 2, 5 },
    { Pin("E5" ), 4, 5 }, { Pin("E13"), 4, 5 },
};
AltPins const altSCK [] = {
    { Pin("A1" ), 3, 6 }, { Pin("A5" ), 1, 5 }, { Pin("A8" ), 2, 5 },
    { Pin("A12"), 1, 6 }, { Pin("B3" ), 1, 5 }, { Pin("B3" ), 3, 6 },
    { Pin("B8" ), 2, 5 }, { Pin("B10"), 2, 5 }, { Pin("B13"), 2, 5 },
    { Pin("C7" ), 1, 5 }, { Pin("C10"), 3, 6 }, { Pin("D7" ), 2, 5 },
    { Pin("D8" ), 2, 5 }, { Pin("E2" ), 4, 5 }, { Pin("E12"), 4, 5 },
    { Pin("F1" ), 2, 5 }, { Pin("F9" ), 2, 5 }, { Pin("F10"), 2, 5 },
};
AltPins const altNSS [] = {
    { Pin("A4" ), 1, 5 }, { Pin("A4" ), 3, 6 }, { Pin("A11"), 1, 6 },
    { Pin("A11"), 2, 5 }, { Pin("A15"), 1, 5 }, { Pin("A15"), 3, 6 },
    { Pin("B9" ), 2, 5 }, { Pin("B12"), 2, 5 }, { Pin("C6" ), 1, 5 },
    { Pin("D6" ), 2, 5 }, { Pin("D15"), 2, 6 }, { Pin("E3" ), 4, 5 },
    { Pin("E4" ), 4, 5 }, { Pin("E11"), 4, 5 }, { Pin("F0" ), 2, 5 },
};
#endif

#if STM32F4
AltPins const altTX [] = {
    { Pin("A0" ), 4, 8 }, { Pin("A2" ), 2, 7 }, { Pin("A9" ), 1, 7 },
    { Pin("A11"), 6, 8 }, { Pin("A12"), 4,11 }, { Pin("A15"), 1, 7 },
    { Pin("A15"), 7, 8 }, { Pin("B4" ), 7, 8 }, { Pin("B6" ), 1, 7 },
    { Pin("B6" ), 5,11 }, { Pin("B9" ), 5,11 }, { Pin("B10"), 3, 7 },
    { Pin("B13"), 5,11 }, { Pin("C6" ), 6, 8 }, { Pin("C10"), 3, 7 },
    { Pin("C10"), 4, 8 }, { Pin("C12"), 5, 8 }, { Pin("D1" ), 4,11 },
    { Pin("D5" ), 2, 7 }, { Pin("D8" ), 3, 7 }, { Pin("D10"), 4, 8 },
    { Pin("D15"), 9,11 }, { Pin("E1" ), 8, 8 }, { Pin("E3" ),10,11 },
    { Pin("E8" ), 5, 8 }, { Pin("E8" ), 7, 8 }, { Pin("F7" ), 7, 8 },
    { Pin("F9" ), 8, 8 }, { Pin("G1" ), 9,11 }, { Pin("G12"),10,11 },
    { Pin("G14"), 6, 8 },
};
AltPins const altRX [] = {
    { Pin("A1" ), 4, 8 }, { Pin("A3" ), 2, 7 }, { Pin("A8" ), 7, 8 },
    { Pin("A10"), 1, 7 }, { Pin("A11"), 4,11 }, { Pin("A12"), 6, 8 },
    { Pin("B3" ), 1, 7 }, { Pin("B3" ), 7, 8 }, { Pin("B5" ), 5,11 },
    { Pin("B7" ), 1, 7 }, { Pin("B8" ), 5,11 }, { Pin("B11"), 3, 7 },
    { Pin("B12"), 5,11 }, { Pin("C5" ), 3, 7 }, { Pin("C7" ), 6, 8 },
    { Pin("C11"), 3, 7 }, { Pin("C11"), 4, 8 }, { Pin("D0" ), 4,11 },
    { Pin("D2" ), 5, 8 }, { Pin("D6" ), 2, 7 }, { Pin("D9" ), 3, 7 },
    { Pin("D14"), 9,11 }, { Pin("E0" ), 8, 8 }, { Pin("E2" ),10,11 },
    { Pin("E7" ), 5, 8 }, { Pin("E7" ), 7, 8 }, { Pin("F6" ), 7, 8 },
    { Pin("F8" ), 8, 8 }, { Pin("G0" ), 9,11 }, { Pin("G9" ), 6, 8 },
    { Pin("G11"),10,11 },
};
AltPins const altMOSI [] = {
    { Pin("A1" ), 4, 5 }, { Pin("A7" ), 1, 5 }, { Pin("A10"), 2, 5 },
    { Pin("A10"), 5, 6 }, { Pin("B0" ), 3, 7 }, { Pin("B2" ), 3, 7 },
    { Pin("B5" ), 1, 5 }, { Pin("B5" ), 3, 6 }, { Pin("B8" ), 5, 6 },
    { Pin("B15"), 2, 5 }, { Pin("C1" ), 2, 5 }, { Pin("C1" ), 3, 5 },
    { Pin("C3" ), 2, 5 }, { Pin("C12"), 3, 6 }, { Pin("D0" ), 3, 6 },
    { Pin("D6" ), 3, 5 }, { Pin("E6" ), 4, 5 }, { Pin("E6" ), 5, 6 },
    { Pin("E14"), 4, 5 }, { Pin("E14"), 5, 6 }, { Pin("F9" ), 5, 5 },
    { Pin("F11"), 5, 5 }, { Pin("G13"), 4, 6 }, { Pin("G14"), 6, 5 },
    { Pin("I3" ), 2, 5 },
};
AltPins const altMISO [] = {
    { Pin("A6" ), 1, 5 }, { Pin("A11"), 4, 6 }, { Pin("A12"), 2, 5 },
    { Pin("A12"), 5, 6 }, { Pin("B4" ), 1, 5 }, { Pin("B4" ), 3, 6 },
    { Pin("B14"), 2, 5 }, { Pin("C2" ), 2, 5 }, { Pin("C11"), 3, 6 },
    { Pin("D0" ), 4, 5 }, { Pin("E5" ), 4, 5 }, { Pin("E5" ), 5, 6 },
    { Pin("E13"), 4, 5 }, { Pin("E13"), 5, 6 }, { Pin("F8" ), 5, 5 },
    { Pin("G12"), 4, 6 }, { Pin("G12"), 6, 5 }, { Pin("H7" ), 5, 5 },
    { Pin("I2" ), 2, 5 },
};
AltPins const altSCK [] = {
    { Pin("A5" ), 1, 5 }, { Pin("A9" ), 2, 5 }, { Pin("B0" ), 5, 6 },
    { Pin("B3" ), 1, 5 }, { Pin("B3" ), 3, 6 }, { Pin("B10"), 2, 5 },
    { Pin("B12"), 3, 7 }, { Pin("B13"), 2, 5 }, { Pin("B13"), 4, 6 },
    { Pin("C7" ), 2, 5 }, { Pin("C10"), 3, 6 }, { Pin("D3" ), 2, 5 },
    { Pin("E2" ), 4, 5 }, { Pin("E2" ), 5, 6 }, { Pin("E12"), 4, 5 },
    { Pin("E12"), 5, 6 }, { Pin("F7" ), 5, 5 }, { Pin("G11"), 4, 6 },
    { Pin("G13"), 6, 5 }, { Pin("H6" ), 5, 5 }, { Pin("I1" ), 2, 5 },
};
AltPins const altNSS [] = {
    { Pin("A4" ), 1, 5 }, { Pin("A4" ), 3, 6 }, { Pin("A11"), 2, 5 },
    { Pin("A15"), 1, 5 }, { Pin("A15"), 3, 6 }, { Pin("B1" ), 5, 6 },
    { Pin("B4" ), 2, 7 }, { Pin("B9" ), 2, 5 }, { Pin("B12"), 2, 5 },
    { Pin("B12"), 4, 6 }, { Pin("D1" ), 2, 7 }, { Pin("E4" ), 4, 5 },
    { Pin("E4" ), 5, 6 }, { Pin("E11"), 4, 5 }, { Pin("E11"), 5, 6 },
    { Pin("F6" ), 5, 5 }, { Pin("G8" ), 6, 5 }, { Pin("G14"), 4, 6 },
    { Pin("H5" ), 5, 5 }, { Pin("I0" ), 2, 5 },
};
#endif

#if STM32F7
AltPins const altTX [] = {
    { Pin("A0" ), 4, 8 }, { Pin("A2" ), 2, 7 }, { Pin("A9" ), 1, 7 },
    { Pin("A12"), 4, 6 }, { Pin("A15"), 7,12 }, { Pin("B4" ), 7,12 },
    { Pin("B6" ), 1, 7 }, { Pin("B6" ), 5, 1 }, { Pin("B9" ), 5, 7 },
    { Pin("B10"), 3, 7 }, { Pin("B13"), 5, 8 }, { Pin("B14"), 1, 4 },
    { Pin("C6" ), 6, 8 }, { Pin("C10"), 3, 7 }, { Pin("C10"), 4, 8 },
    { Pin("C12"), 5, 8 }, { Pin("D1" ), 4, 8 }, { Pin("D5" ), 2, 7 },
    { Pin("D8" ), 3, 7 }, { Pin("E1" ), 8, 8 }, { Pin("E8" ), 7, 8 },
    { Pin("F7" ), 7, 8 }, { Pin("G14"), 6, 8 }, { Pin("H13"), 4, 8 },
};
AltPins const altRX [] = {
    { Pin("A1" ), 4, 8 }, { Pin("A3" ), 2, 7 }, { Pin("A8" ), 7,12 },
    { Pin("A10"), 1, 7 }, { Pin("A11"), 4, 6 }, { Pin("B3" ), 7,12 },
    { Pin("B5" ), 5, 1 }, { Pin("B7" ), 1, 7 }, { Pin("B8" ), 5, 7 },
    { Pin("B11"), 3, 7 }, { Pin("B12"), 5, 8 }, { Pin("B15"), 1, 4 },
    { Pin("C7" ), 6, 8 }, { Pin("C11"), 3, 7 }, { Pin("C11"), 4, 8 },
    { Pin("D0" ), 4, 8 }, { Pin("D2" ), 5, 8 }, { Pin("D6" ), 2, 7 },
    { Pin("D9" ), 3, 7 }, { Pin("E0" ), 8, 8 }, { Pin("E7" ), 7, 8 },
    { Pin("F6" ), 7, 8 }, { Pin("G9" ), 6, 8 }, { Pin("H14"), 4, 8 },
    { Pin("I9" ), 4, 8 },
};
AltPins const altMOSI [] = {
    { Pin("A7" ), 1, 5 }, { Pin("A7" ), 6, 8 }, { Pin("B2" ), 3, 7 },
    { Pin("B5" ), 1, 5 }, { Pin("B5" ), 3, 6 }, { Pin("B5" ), 6, 8 },
    { Pin("B15"), 2, 5 }, { Pin("C1" ), 2, 5 }, { Pin("C3" ), 2, 5 },
    { Pin("C12"), 3, 6 }, { Pin("D6" ), 3, 5 }, { Pin("D7" ), 1, 5 },
    { Pin("E6" ), 4, 5 }, { Pin("E14"), 4, 5 }, { Pin("F9" ), 5, 5 },
    { Pin("F11"), 5, 5 }, { Pin("G14"), 6, 5 }, { Pin("I3" ), 2, 5 },
};
AltPins const altMISO [] = {
    { Pin("A6" ), 1, 5 }, { Pin("A6" ), 6, 8 }, { Pin("B4" ), 1, 5 },
    { Pin("B4" ), 3, 6 }, { Pin("B4" ), 6, 8 }, { Pin("B14"), 2, 5 },
    { Pin("C2" ), 2, 5 }, { Pin("C11"), 3, 6 }, { Pin("E5" ), 4, 5 },
    { Pin("E13"), 4, 5 }, { Pin("F8" ), 5, 5 }, { Pin("G9" ), 1, 5 },
    { Pin("G12"), 6, 5 }, { Pin("H7" ), 5, 5 }, { Pin("I2" ), 2, 5 },
};
AltPins const altSCK [] = {
    { Pin("A5" ), 1, 5 }, { Pin("A5" ), 6, 8 }, { Pin("A9" ), 2, 5 },
    { Pin("A12"), 2, 5 }, { Pin("B3" ), 1, 5 }, { Pin("B3" ), 3, 6 },
    { Pin("B3" ), 6, 8 }, { Pin("B10"), 2, 5 }, { Pin("B13"), 2, 5 },
    { Pin("C10"), 3, 6 }, { Pin("D3" ), 2, 5 }, { Pin("E2" ), 4, 5 },
    { Pin("E12"), 4, 5 }, { Pin("F7" ), 5, 5 }, { Pin("G11"), 1, 5 },
    { Pin("G13"), 6, 5 }, { Pin("H6" ), 5, 5 }, { Pin("I1" ), 2, 5 },
};
AltPins const altNSS [] = {
    { Pin("A4" ), 1, 5 }, { Pin("A4" ), 3, 6 }, { Pin("A4" ), 6, 8 },
    { Pin("A11"), 2, 5 }, { Pin("A15"), 1, 5 }, { Pin("A15"), 3, 6 },
    { Pin("A15"), 6, 7 }, { Pin("B4" ), 2, 7 }, { Pin("B9" ), 2, 5 },
    { Pin("B12"), 2, 5 }, { Pin("E4" ), 4, 5 }, { Pin("E11"), 4, 5 },
    { Pin("F6" ), 5, 5 }, { Pin("G8" ), 6, 5 }, { Pin("G10"), 1, 5 },
    { Pin("H5" ), 5, 5 }, { Pin("I0" ), 2, 5 },
};
#endif

#if STM32G0
AltPins const altTX [] = {
    { Pin("A0" ), 4, 4 }, { Pin("A2" ), 2, 1 }, { Pin("A4" ), 6, 3 },
    { Pin("A5" ), 3, 4 }, { Pin("A9" ), 1, 1 }, { Pin("A14"), 2, 1 },
    { Pin("B0" ), 5, 8 }, { Pin("B2" ), 3, 4 }, { Pin("B3" ), 5, 3 },
    { Pin("B6" ), 1, 0 }, { Pin("B8" ), 3, 4 }, { Pin("B8" ), 6, 8 },
    { Pin("B10"), 3, 4 }, { Pin("C0" ), 6, 4 }, { Pin("C4" ), 1, 1 },
    { Pin("C4" ), 3, 0 }, { Pin("C10"), 3, 0 }, { Pin("C10"), 4, 1 },
    { Pin("C12"), 5, 3 }, { Pin("D3" ), 5, 3 }, { Pin("D5" ), 2, 0 },
    { Pin("D8" ), 3, 0 }, { Pin("E8" ), 4, 0 }, { Pin("E10"), 5, 3 },
    { Pin("F9" ), 6, 3 },
};
AltPins const altRX [] = {
    { Pin("A1" ), 4, 4 }, { Pin("A3" ), 2, 1 }, { Pin("A5" ), 6, 3 },
    { Pin("A10"), 1, 1 }, { Pin("A15"), 2, 1 }, { Pin("B0" ), 3, 4 },
    { Pin("B1" ), 5, 8 }, { Pin("B4" ), 5, 3 }, { Pin("B7" ), 1, 0 },
    { Pin("B9" ), 3, 4 }, { Pin("B9" ), 6, 8 }, { Pin("B11"), 3, 4 },
    { Pin("C1" ), 6, 4 }, { Pin("C5" ), 1, 1 }, { Pin("C5" ), 3, 0 },
    { Pin("C11"), 3, 0 }, { Pin("C11"), 4, 1 }, { Pin("D2" ), 5, 3 },
    { Pin("D6" ), 2, 0 }, { Pin("D9" ), 3, 0 }, { Pin("E9" ), 4, 0 },
    { Pin("E11"), 5, 3 }, { Pin("F10"), 6, 3 },
};
AltPins const altMOSI [] = {
    { Pin("A2" ), 1, 0 }, { Pin("A4" ), 2, 1 }, { Pin("A7" ), 1, 0 },
    { Pin("A10"), 2, 0 }, { Pin("A12"), 1, 0 }, { Pin("B5" ), 1, 0 },
    { Pin("B5" ), 3, 9 }, { Pin("B7" ), 2, 1 }, { Pin("B11"), 2, 0 },
    { Pin("B15"), 2, 0 }, { Pin("C3" ), 2, 1 }, { Pin("C12"), 3, 4 },
    { Pin("D4" ), 2, 1 }, { Pin("D6" ), 1, 1 }, { Pin("E15"), 1, 0 },
};
AltPins const altMISO [] = {
    { Pin("A3" ), 2, 0 }, { Pin("A6" ), 1, 0 }, { Pin("A9" ), 2, 4 },
    { Pin("A11"), 1, 0 }, { Pin("B2" ), 2, 1 }, { Pin("B4" ), 1, 0 },
    { Pin("B4" ), 3, 9 }, { Pin("B6" ), 2, 4 }, { Pin("B14"), 2, 0 },
    { Pin("C2" ), 2, 1 }, { Pin("C11"), 3, 4 }, { Pin("D3" ), 2, 1 },
    { Pin("D5" ), 1, 1 }, { Pin("E14"), 1, 0 },
};
AltPins const altSCK [] = {
    { Pin("A0" ), 2, 0 }, { Pin("A1" ), 1, 0 }, { Pin("A5" ), 1, 0 },
    { Pin("B3" ), 1, 0 }, { Pin("B3" ), 3, 9 }, { Pin("B8" ), 2, 1 },
    { Pin("B10"), 2, 5 }, { Pin("B13"), 2, 0 }, { Pin("C10"), 3, 4 },
    { Pin("D1" ), 2, 1 }, { Pin("D8" ), 1, 1 }, { Pin("E13"), 1, 0 },
};
AltPins const altNSS [] = {
    { Pin("A0" ), 2, 1 }, { Pin("A4" ), 1, 0 }, { Pin("A4" ), 3, 9 },
    { Pin("A6" ), 3, 4 }, { Pin("A6" ), 6, 3 }, { Pin("A8" ), 2, 1 },
    { Pin("A11"), 1, 1 }, { Pin("A15"), 1, 0 }, { Pin("A15"), 3, 9 },
    { Pin("B0" ), 1, 0 }, { Pin("B4" ), 1, 4 }, { Pin("B6" ), 5, 8 },
    { Pin("B7" ), 4, 4 }, { Pin("B9" ), 2, 5 }, { Pin("B12"), 2, 0 },
    { Pin("B13"), 3, 4 }, { Pin("B15"), 6, 8 }, { Pin("D0" ), 2, 1 },
    { Pin("D3" ), 2, 0 }, { Pin("D5" ), 5, 3 }, { Pin("D9" ), 1, 1 },
    { Pin("D11"), 3, 0 }, { Pin("E12"), 1, 0 }, { Pin("F7" ), 5, 3 },
    { Pin("F12"), 6, 3 },
};
#endif

#if STM32G4
AltPins const altTX [] = {
    { Pin("A2" ), 2, 7 }, { Pin("A9" ), 1, 7 }, { Pin("A14"), 2, 7 },
    { Pin("B3" ), 2, 7 }, { Pin("B6" ), 1, 7 }, { Pin("B9" ), 3, 7 },
    { Pin("B10"), 3, 7 }, { Pin("C4" ), 1, 7 }, { Pin("C10"), 3, 7 },
    { Pin("C10"), 4, 5 }, { Pin("C12"), 5, 5 }, { Pin("D5" ), 2, 7 },
    { Pin("D8" ), 3, 7 }, { Pin("E0" ), 1, 7 }, { Pin("G9" ), 1, 7 },
};
AltPins const altRX [] = {
    { Pin("A3" ), 2, 7 }, { Pin("A10"), 1, 7 }, { Pin("A15"), 2, 7 },
    { Pin("B4" ), 2, 7 }, { Pin("B7" ), 1, 7 }, { Pin("B8" ), 3, 7 },
    { Pin("B11"), 3, 7 }, { Pin("C5" ), 1, 7 }, { Pin("C11"), 3, 7 },
    { Pin("C11"), 4, 5 }, { Pin("D2" ), 5, 5 }, { Pin("D6" ), 2, 7 },
    { Pin("D9" ), 3, 7 }, { Pin("E1" ), 1, 7 }, { Pin("E15"), 3, 7 },
};
AltPins const altMOSI [] = {
    { Pin("A7" ), 1, 5 }, { Pin("A11"), 2, 5 }, { Pin("B5" ), 1, 5 },
    { Pin("B5" ), 3, 6 }, { Pin("B15"), 2, 5 }, { Pin("C12"), 3, 6 },
    { Pin("E6" ), 4, 5 }, { Pin("E14"), 4, 5 }, { Pin("G4" ), 1, 5 },
};
AltPins const altMISO [] = {
    { Pin("A6" ), 1, 5 }, { Pin("A10"), 2, 5 }, { Pin("B4" ), 1, 5 },
    { Pin("B4" ), 3, 6 }, { Pin("B14"), 2, 5 }, { Pin("C11"), 3, 6 },
    { Pin("E5" ), 4, 5 }, { Pin("E13"), 4, 5 }, { Pin("G3" ), 1, 5 },
};
AltPins const altSCK [] = {
    { Pin("A5" ), 1, 5 }, { Pin("B3" ), 1, 5 }, { Pin("B3" ), 3, 6 },
    { Pin("B13"), 2, 5 }, { Pin("C10"), 3, 6 }, { Pin("E2" ), 4, 5 },
    { Pin("E12"), 4, 5 }, { Pin("F1" ), 2, 5 }, { Pin("F9" ), 2, 5 },
    { Pin("F10"), 2, 5 }, { Pin("G2" ), 1, 5 }, { Pin("G9" ), 3, 6 },
};
AltPins const altNSS [] = {
    { Pin("A0" ), 2, 7 }, { Pin("A4" ), 1, 5 }, { Pin("A4" ), 3, 6 },
    { Pin("A11"), 1, 7 }, { Pin("A13"), 3, 7 }, { Pin("A15"), 1, 5 },
    { Pin("A15"), 3, 6 }, { Pin("B12"), 2, 5 }, { Pin("B13"), 3, 7 },
    { Pin("D3" ), 2, 7 }, { Pin("D11"), 3, 7 }, { Pin("D15"), 2, 6 },
    { Pin("E3" ), 4, 5 }, { Pin("E4" ), 4, 5 }, { Pin("E11"), 4, 5 },
    { Pin("F0" ), 2, 5 }, { Pin("G5" ), 1, 5 },
};
#endif

#if STM32H7
AltPins const altTX [] = {
    { Pin("A0" ), 4, 8 }, { Pin("A2" ), 2, 7 }, { Pin("A9" ), 1, 7 },
    { Pin("A12"), 4, 6 }, { Pin("A15"), 7,11 }, { Pin("B4" ), 7,11 },
    { Pin("B6" ), 1, 7 }, { Pin("B6" ), 5,14 }, { Pin("B9" ), 4, 8 },
    { Pin("B10"), 3, 7 }, { Pin("B13"), 5,14 }, { Pin("B14"), 1, 4 },
    { Pin("C6" ), 6, 7 }, { Pin("C10"), 3, 7 }, { Pin("C10"), 4, 8 },
    { Pin("C12"), 5, 8 }, { Pin("D1" ), 4, 8 }, { Pin("D5" ), 2, 7 },
    { Pin("D8" ), 3, 7 }, { Pin("D15"), 9,11 }, { Pin("E1" ), 8, 8 },
    { Pin("E3" ),10,11 }, { Pin("E8" ), 7, 7 }, { Pin("F7" ), 7, 7 },
    { Pin("G1" ), 9,11 }, { Pin("G12"),10,11 }, { Pin("G14"), 6, 7 },
    { Pin("H13"), 4, 8 }, { Pin("J8" ), 8, 8 },
};
AltPins const altRX [] = {
    { Pin("A1" ), 4, 8 }, { Pin("A3" ), 2, 7 }, { Pin("A8" ), 7,11 },
    { Pin("A10"), 1, 7 }, { Pin("A11"), 4, 6 }, { Pin("B3" ), 7,11 },
    { Pin("B5" ), 5,14 }, { Pin("B7" ), 1, 7 }, { Pin("B8" ), 4, 8 },
    { Pin("B11"), 3, 7 }, { Pin("B12"), 5,14 }, { Pin("B15"), 1, 4 },
    { Pin("C7" ), 6, 7 }, { Pin("C11"), 3, 7 }, { Pin("C11"), 4, 8 },
    { Pin("D0" ), 4, 8 }, { Pin("D2" ), 5, 8 }, { Pin("D6" ), 2, 7 },
    { Pin("D9" ), 3, 7 }, { Pin("D14"), 9,11 }, { Pin("E0" ), 8, 8 },
    { Pin("E2" ),10,11 }, { Pin("E7" ), 7, 7 }, { Pin("F6" ), 7, 7 },
    { Pin("G0" ), 9,11 }, { Pin("G9" ), 6, 7 }, { Pin("G11"),10,11 },
    { Pin("H14"), 4, 8 }, { Pin("I9" ), 4, 8 }, { Pin("J9" ), 8, 8 },
};
AltPins const altMOSI [] = {
    { Pin("A7" ), 1, 5 }, { Pin("A7" ), 6, 8 }, { Pin("B2" ), 3, 7 },
    { Pin("B5" ), 1, 5 }, { Pin("B5" ), 3, 7 }, { Pin("B5" ), 6, 8 },
    { Pin("B15"), 2, 5 }, { Pin("C1" ), 2, 5 }, { Pin("C3" ), 2, 5 },
    { Pin("C12"), 3, 6 }, { Pin("D6" ), 3, 5 }, { Pin("D7" ), 1, 5 },
    { Pin("E6" ), 4, 5 }, { Pin("E14"), 4, 5 }, { Pin("F9" ), 5, 5 },
    { Pin("F11"), 5, 5 }, { Pin("G14"), 6, 5 }, { Pin("I3" ), 2, 5 },
    { Pin("J10"), 5, 5 },
};
AltPins const altMISO [] = {
    { Pin("A6" ), 1, 5 }, { Pin("A6" ), 6, 8 }, { Pin("B4" ), 1, 5 },
    { Pin("B4" ), 3, 6 }, { Pin("B4" ), 6, 8 }, { Pin("B14"), 2, 5 },
    { Pin("C2" ), 2, 5 }, { Pin("C11"), 3, 6 }, { Pin("E5" ), 4, 5 },
    { Pin("E13"), 4, 5 }, { Pin("F8" ), 5, 5 }, { Pin("G9" ), 1, 5 },
    { Pin("G12"), 6, 5 }, { Pin("H7" ), 5, 5 }, { Pin("I2" ), 2, 5 },
    { Pin("J11"), 5, 5 },
};
AltPins const altSCK [] = {
    { Pin("A5" ), 1, 5 }, { Pin("A5" ), 6, 8 }, { Pin("A9" ), 2, 5 },
    { Pin("A12"), 2, 5 }, { Pin("B3" ), 1, 5 }, { Pin("B3" ), 3, 6 },
    { Pin("B3" ), 6, 8 }, { Pin("B10"), 2, 5 }, { Pin("B13"), 2, 5 },
    { Pin("C10"), 3, 6 }, { Pin("C12"), 6, 5 }, { Pin("D3" ), 2, 5 },
    { Pin("E2" ), 4, 5 }, { Pin("E12"), 4, 5 }, { Pin("F7" ), 5, 5 },
    { Pin("G11"), 1, 5 }, { Pin("G13"), 6, 5 }, { Pin("H6" ), 5, 5 },
    { Pin("I1" ), 2, 5 }, { Pin("K0" ), 5, 5 },
};
AltPins const altNSS [] = {
    { Pin("A0" ), 2, 7 }, { Pin("A0" ), 6, 5 }, { Pin("A4" ), 1, 5 },
    { Pin("A4" ), 3, 6 }, { Pin("A4" ), 6, 8 }, { Pin("A11"), 1, 7 },
    { Pin("A11"), 2, 5 }, { Pin("A15"), 1, 5 }, { Pin("A15"), 3, 6 },
    { Pin("A15"), 6, 7 }, { Pin("B4" ), 2, 7 }, { Pin("B9" ), 2, 5 },
    { Pin("B12"), 2, 5 }, { Pin("B13"), 3, 7 }, { Pin("D3" ), 2, 7 },
    { Pin("D11"), 3, 7 }, { Pin("E4" ), 4, 5 }, { Pin("E11"), 4, 5 },
    { Pin("F6" ), 5, 5 }, { Pin("G8" ), 6, 5 }, { Pin("G10"), 1, 5 },
    { Pin("G13"),10,11 }, { Pin("G13"), 6, 7 }, { Pin("G15"), 6, 7 },
    { Pin("H5" ), 5, 5 }, { Pin("I0" ), 2, 5 }, { Pin("K1" ), 5, 5 },
};
#endif

#if STM32L0
AltPins const altTX [] = {
    { Pin("A0" ), 4, 6 }, { Pin("A2" ), 2, 4 }, { Pin("A9" ), 1, 4 },
    { Pin("A9" ), 2, 4 }, { Pin("A14"), 2, 4 }, { Pin("B3" ), 5, 6 },
    { Pin("B6" ), 1, 0 }, { Pin("B6" ), 2, 0 }, { Pin("B8" ), 2, 0 },
    { Pin("C10"), 4, 6 }, { Pin("C12"), 5, 2 }, { Pin("D5" ), 2, 0 },
    { Pin("E8" ), 4, 6 }, { Pin("E10"), 5, 6 },
};
AltPins const altRX [] = {
    { Pin("A0" ), 2, 0 }, { Pin("A1" ), 4, 6 }, { Pin("A3" ), 2, 4 },
    { Pin("A10"), 1, 4 }, { Pin("A10"), 2, 4 }, { Pin("A15"), 2, 4 },
    { Pin("B4" ), 5, 6 }, { Pin("B7" ), 1, 0 }, { Pin("B7" ), 2, 0 },
    { Pin("C11"), 4, 6 }, { Pin("D2" ), 5, 6 }, { Pin("D6" ), 2, 0 },
    { Pin("E9" ), 4, 6 }, { Pin("E11"), 5, 6 },
};
AltPins const altMOSI [] = {
    { Pin("A7" ), 1, 0 }, { Pin("A12"), 1, 0 }, { Pin("B1" ), 1, 1 },
    { Pin("B5" ), 1, 0 }, { Pin("B15"), 1, 0 }, { Pin("B15"), 2, 0 },
    { Pin("C3" ), 2, 2 }, { Pin("D4" ), 2, 1 }, { Pin("E15"), 1, 2 },
};
AltPins const altMISO [] = {
    { Pin("A6" ), 1, 0 }, { Pin("A11"), 1, 0 }, { Pin("A14"), 1, 5 },
    { Pin("B0" ), 1, 1 }, { Pin("B4" ), 1, 0 }, { Pin("B14"), 1, 0 },
    { Pin("B14"), 2, 0 }, { Pin("C2" ), 2, 2 }, { Pin("D3" ), 2, 2 },
    { Pin("E14"), 1, 2 },
};
AltPins const altSCK [] = {
    { Pin("A5" ), 1, 0 }, { Pin("A13"), 1, 5 }, { Pin("B3" ), 1, 0 },
    { Pin("B10"), 2, 5 }, { Pin("B13"), 1, 0 }, { Pin("B13"), 2, 0 },
    { Pin("D1" ), 2, 1 }, { Pin("E13"), 1, 2 },
};
AltPins const altNSS [] = {
    { Pin("A4" ), 1, 0 }, { Pin("A15"), 1, 0 }, { Pin("B8" ), 1, 5 },
    { Pin("B9" ), 2, 5 }, { Pin("B12"), 1, 0 }, { Pin("B12"), 2, 0 },
    { Pin("D0" ), 2, 1 }, { Pin("E12"), 1, 2 },
};
#endif

#if STM32L1
AltPins const altTX [] = {
    { Pin("A2" ), 2, 7 }, { Pin("A9" ), 1, 7 }, { Pin("B6" ), 1, 7 },
    { Pin("B10"), 3, 7 }, { Pin("C10"), 3, 7 }, { Pin("C10"), 4, 8 },
    { Pin("C12"), 5, 8 }, { Pin("D5" ), 2, 7 }, { Pin("D8" ), 3, 7 },
};
AltPins const altRX [] = {
    { Pin("A3" ), 2, 7 }, { Pin("A10"), 1, 7 }, { Pin("B7" ), 1, 7 },
    { Pin("B11"), 3, 7 }, { Pin("C11"), 3, 7 }, { Pin("C11"), 4, 8 },
    { Pin("D2" ), 5, 8 }, { Pin("D6" ), 2, 7 }, { Pin("D9" ), 3, 7 },
};
AltPins const altMOSI [] = {
    { Pin("A7" ), 1, 5 }, { Pin("A12"), 1, 5 }, { Pin("B5" ), 1, 5 },
    { Pin("B5" ), 3, 6 }, { Pin("B15"), 2, 5 }, { Pin("C12"), 3, 6 },
    { Pin("D4" ), 2, 5 }, { Pin("E15"), 1, 5 },
};
AltPins const altMISO [] = {
    { Pin("A6" ), 1, 5 }, { Pin("A11"), 1, 5 }, { Pin("B4" ), 1, 5 },
    { Pin("B4" ), 3, 6 }, { Pin("B14"), 2, 5 }, { Pin("C11"), 3, 6 },
    { Pin("D3" ), 2, 5 }, { Pin("E14"), 1, 5 },
};
AltPins const altSCK [] = {
    { Pin("A5" ), 1, 5 }, { Pin("B3" ), 1, 5 }, { Pin("B3" ), 3, 6 },
    { Pin("B13"), 2, 5 }, { Pin("C10"), 3, 6 }, { Pin("D1" ), 2, 5 },
    { Pin("E13"), 1, 5 },
};
AltPins const altNSS [] = {
    { Pin("A4" ), 1, 5 }, { Pin("A4" ), 3, 6 }, { Pin("A15"), 1, 5 },
    { Pin("A15"), 3, 6 }, { Pin("B12"), 2, 5 }, { Pin("D0" ), 2, 5 },
    { Pin("E12"), 1, 5 },
};
#endif

#if STM32L4
AltPins const altTX [] = {
    { Pin("A0" ), 4, 8 }, { Pin("A2" ), 2, 7 }, { Pin("A9" ), 1, 7 },
    { Pin("B6" ), 1, 7 }, { Pin("B10"), 3, 7 }, { Pin("C4" ), 3, 7 },
    { Pin("C10"), 3, 7 }, { Pin("C10"), 4, 8 }, { Pin("C12"), 5, 8 },
    { Pin("D5" ), 2, 7 }, { Pin("D8" ), 3, 7 }, { Pin("G9" ), 1, 7 },
};
AltPins const altRX [] = {
    { Pin("A1" ), 4, 8 }, { Pin("A3" ), 2, 7 }, { Pin("A10"), 1, 7 },
    { Pin("A15"), 2, 3 }, { Pin("B7" ), 1, 7 }, { Pin("B11"), 3, 7 },
    { Pin("C5" ), 3, 7 }, { Pin("C11"), 3, 7 }, { Pin("C11"), 4, 8 },
    { Pin("D2" ), 5, 8 }, { Pin("D6" ), 2, 7 }, { Pin("D9" ), 3, 7 },
    { Pin("G10"), 1, 7 },
};
AltPins const altMOSI [] = {
    { Pin("A7" ), 1, 5 }, { Pin("A12"), 1, 5 }, { Pin("B5" ), 1, 5 },
    { Pin("B5" ), 3, 6 }, { Pin("B15"), 2, 5 }, { Pin("C1" ), 2, 3 },
    { Pin("C3" ), 2, 5 }, { Pin("C12"), 3, 6 }, { Pin("D4" ), 2, 5 },
    { Pin("D6" ), 3, 5 }, { Pin("E15"), 1, 5 }, { Pin("G4" ), 1, 5 },
    { Pin("G11"), 3, 6 }, { Pin("I3" ), 2, 5 },
};
AltPins const altMISO [] = {
    { Pin("A6" ), 1, 5 }, { Pin("A11"), 1, 5 }, { Pin("B4" ), 1, 5 },
    { Pin("B4" ), 3, 6 }, { Pin("B14"), 2, 5 }, { Pin("C2" ), 2, 5 },
    { Pin("C11"), 3, 6 }, { Pin("D3" ), 2, 5 }, { Pin("E14"), 1, 5 },
    { Pin("G3" ), 1, 5 }, { Pin("G10"), 3, 6 }, { Pin("I2" ), 2, 5 },
};
AltPins const altSCK [] = {
    { Pin("A1" ), 1, 5 }, { Pin("A5" ), 1, 5 }, { Pin("A9" ), 2, 3 },
    { Pin("B3" ), 1, 5 }, { Pin("B3" ), 3, 6 }, { Pin("B10"), 2, 5 },
    { Pin("B13"), 2, 5 }, { Pin("C10"), 3, 6 }, { Pin("D1" ), 2, 5 },
    { Pin("D3" ), 2, 3 }, { Pin("E13"), 1, 5 }, { Pin("G2" ), 1, 5 },
    { Pin("G9" ), 3, 6 }, { Pin("I1" ), 2, 5 },
};
AltPins const altNSS [] = {
    { Pin("A0" ), 2, 7 }, { Pin("A4" ), 1, 5 }, { Pin("A4" ), 3, 6 },
    { Pin("A6" ), 3, 7 }, { Pin("A11"), 1, 7 }, { Pin("A15"), 1, 5 },
    { Pin("A15"), 3, 6 }, { Pin("B0" ), 1, 5 }, { Pin("B4" ), 1, 7 },
    { Pin("B9" ), 2, 5 }, { Pin("B12"), 2, 5 }, { Pin("B13"), 3, 7 },
    { Pin("D0" ), 2, 5 }, { Pin("D3" ), 2, 7 }, { Pin("D11"), 3, 7 },
    { Pin("E12"), 1, 5 }, { Pin("G5" ), 1, 5 }, { Pin("G11"), 1, 7 },
    { Pin("G12"), 3, 6 }, { Pin("I0" ), 2, 5 },
};
#endif

#if STM32L5
AltPins const altTX [] = {
    { Pin("A0" ), 4, 8 }, { Pin("A2" ), 2, 7 }, { Pin("A9" ), 1, 7 },
    { Pin("B6" ), 1, 7 }, { Pin("B10"), 3, 7 }, { Pin("C4" ), 3, 7 },
    { Pin("C10"), 3, 7 }, { Pin("C10"), 4, 8 }, { Pin("C12"), 5, 8 },
    { Pin("D5" ), 2, 7 }, { Pin("D8" ), 3, 7 }, { Pin("G9" ), 1, 7 },
};
AltPins const altRX [] = {
    { Pin("A1" ), 4, 8 }, { Pin("A3" ), 2, 7 }, { Pin("A10"), 1, 7 },
    { Pin("A15"), 2, 3 }, { Pin("B7" ), 1, 7 }, { Pin("B11"), 3, 7 },
    { Pin("C5" ), 3, 7 }, { Pin("C11"), 3, 7 }, { Pin("C11"), 4, 8 },
    { Pin("D2" ), 5, 8 }, { Pin("D6" ), 2, 7 }, { Pin("D9" ), 3, 7 },
    { Pin("G10"), 1, 7 },
};
AltPins const altMOSI [] = {
    { Pin("A7" ), 1, 5 }, { Pin("A12"), 1, 5 }, { Pin("B5" ), 1, 5 },
    { Pin("B5" ), 3, 6 }, { Pin("B15"), 2, 5 }, { Pin("C1" ), 2, 3 },
    { Pin("C3" ), 2, 5 }, { Pin("C12"), 3, 6 }, { Pin("D4" ), 2, 5 },
    { Pin("D6" ), 3, 5 }, { Pin("E15"), 1, 5 }, { Pin("G4" ), 1, 5 },
    { Pin("G11"), 3, 6 },
};
AltPins const altMISO [] = {
    { Pin("A6" ), 1, 5 }, { Pin("A11"), 1, 5 }, { Pin("B4" ), 1, 5 },
    { Pin("B4" ), 3, 6 }, { Pin("B14"), 2, 5 }, { Pin("C2" ), 2, 5 },
    { Pin("C11"), 3, 6 }, { Pin("D3" ), 2, 5 }, { Pin("E14"), 1, 5 },
    { Pin("G3" ), 1, 5 }, { Pin("G10"), 3, 6 },
};
AltPins const altSCK [] = {
    { Pin("A1" ), 1, 5 }, { Pin("A5" ), 1, 5 }, { Pin("A9" ), 2, 3 },
    { Pin("B3" ), 1, 5 }, { Pin("B3" ), 3, 6 }, { Pin("B10"), 2, 5 },
    { Pin("B13"), 2, 5 }, { Pin("C10"), 3, 6 }, { Pin("D1" ), 2, 5 },
    { Pin("D3" ), 2, 3 }, { Pin("E13"), 1, 5 }, { Pin("G2" ), 1, 5 },
    { Pin("G9" ), 3, 6 },
};
AltPins const altNSS [] = {
    { Pin("A0" ), 2, 7 }, { Pin("A4" ), 1, 5 }, { Pin("A4" ), 3, 6 },
    { Pin("A6" ), 3, 7 }, { Pin("A11"), 1, 7 }, { Pin("A15"), 1, 5 },
    { Pin("A15"), 3, 6 }, { Pin("B0" ), 1, 5 }, { Pin("B4" ), 1, 7 },
    { Pin("B9" ), 2, 5 }, { Pin("B12"), 2, 5 }, { Pin("B13"), 3, 7 },
    { Pin("D0" ), 2, 5 }, { Pin("D3" ), 2, 7 }, { Pin("D11"), 3, 7 },
    { Pin("E12"), 1, 5 }, { Pin("G5" ), 1, 5 }, { Pin("G11"), 1, 7 },
    { Pin("G12"), 3, 6 },
};
#endif

#if STM32WB
AltPins const altTX [] = {
    { Pin("A9" ), 1, 7 }, { Pin("B6" ), 1, 7 },
};
AltPins const altRX [] = {
    { Pin("A10"), 1, 7 }, { Pin("B7" ), 1, 7 },
};
AltPins const altMOSI [] = {
    { Pin("A5" ), 1, 4 }, { Pin("A7" ), 1, 5 }, { Pin("A12"), 1, 5 },
    { Pin("A13"), 1, 5 }, { Pin("B5" ), 1, 5 }, { Pin("B15"), 2, 5 },
    { Pin("C1" ), 2, 3 }, { Pin("C3" ), 2, 5 }, { Pin("D4" ), 2, 5 },
};
AltPins const altMISO [] = {
    { Pin("A6" ), 1, 5 }, { Pin("A11"), 1, 5 }, { Pin("B4" ), 1, 5 },
    { Pin("B14"), 2, 5 }, { Pin("C2" ), 2, 5 }, { Pin("D3" ), 2, 5 },
};
AltPins const altSCK [] = {
    { Pin("A1" ), 1, 5 }, { Pin("A5" ), 1, 5 }, { Pin("A9" ), 2, 5 },
    { Pin("B3" ), 1, 5 }, { Pin("B10"), 2, 5 }, { Pin("B13"), 2, 5 },
    { Pin("D1" ), 2, 5 }, { Pin("D3" ), 2, 3 },
};
AltPins const altNSS [] = {
    { Pin("A4" ), 1, 5 }, { Pin("A11"), 1, 7 }, { Pin("A14"), 1, 5 },
    { Pin("A15"), 1, 5 }, { Pin("B2" ), 1, 5 }, { Pin("B4" ), 1, 7 },
    { Pin("B6" ), 1, 5 }, { Pin("B9" ), 2, 5 }, { Pin("B12"), 2, 5 },
    { Pin("D0" ), 2, 5 },
};
#endif

#if STM32WL
AltPins const altTX [] = {
    { Pin("A2" ), 2, 7 }, { Pin("A9" ), 1, 7 }, { Pin("B6" ), 1, 7 },
};
AltPins const altRX [] = {
    { Pin("A3" ), 2, 7 }, { Pin("A10"), 1, 7 }, { Pin("B7" ), 1, 7 },
};
AltPins const altMOSI [] = {
    { Pin("A7" ), 1, 5 }, { Pin("A10"), 2, 5 }, { Pin("A12"), 1, 5 },
    { Pin("B5" ), 1, 5 }, { Pin("B15"), 2, 5 }, { Pin("C1" ), 2, 3 },
    { Pin("C3" ), 2, 5 },
};
AltPins const altMISO [] = {
    { Pin("A5" ), 2, 3 }, { Pin("A6" ), 1, 5 }, { Pin("A11"), 1, 5 },
    { Pin("B4" ), 1, 5 }, { Pin("B14"), 2, 5 }, { Pin("C2" ), 2, 5 },
};
AltPins const altSCK [] = {
    { Pin("A1" ), 1, 5 }, { Pin("A5" ), 1, 5 }, { Pin("A8" ), 2, 5 },
    { Pin("A9" ), 2, 5 }, { Pin("B3" ), 1, 5 }, { Pin("B10"), 2, 5 },
    { Pin("B13"), 2, 5 },
};
AltPins const altNSS [] = {
    { Pin("A0" ), 2, 7 }, { Pin("A4" ), 1, 5 }, { Pin("A9" ), 2, 3 },
    { Pin("A11"), 1, 7 }, { Pin("A15"), 1, 5 }, { Pin("B2" ), 1, 5 },
    { Pin("B4" ), 1, 7 }, { Pin("B9" ), 2, 5 }, { Pin("B12"), 2, 5 },
};
#endif
