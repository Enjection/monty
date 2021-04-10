namespace lcd {
    // RK043FN48H
    enum { WIDTH=480,HEIGHT=272, HSYN=41,HBP=13,HFP=32,VSYN=10,VBP=2,VFP=2 };

    // shorthand to set both horizontal and vertical parameters
    constexpr auto hv (auto h, auto l) { return (h << 16) | l; }

    // FIXME RCC name clash mcb/device
    constexpr auto RCC  = io32<0x4002'3800>;
    constexpr auto LTDC = io32<device::LTDC>;

    void init () {
        RCC(0x44)[26] = 1;            // LTDCEN in APB2ENR

        RCC(0x00)[28] = 0;            // ~PLLSAION in CR
        RCC(0x88).mask(6, 9) = 192;   // PLLSAIN in PLLSAICFGR 192 MHz
        RCC(0x88).mask(28, 3) = 5;    // PLLSAIR in PLLSAICFGR 38.4 MHz
        RCC(0x88).mask(16, 2) = 1;    // PLLSAIDIVR in DKCFGR1 9.6 MHz
        RCC(0x00)[28] = 1;            // PLLSAION in CR
        while (RCC(0x00)[29] == 0) {} // wait for PLLSAIRDY in CR

        LTDC(0x08) = hv(HSYN-1, VSYN-1);                              // SSCR
        LTDC(0x0C) = hv(HSYN+HBP-1, VSYN+VBP-1);                      // BPCR
        LTDC(0x10) = hv(HSYN+HBP+WIDTH-1, VSYN+VBP+HEIGHT-1);         // AWCR
        LTDC(0x14) = hv(HSYN+HBP+WIDTH+HFP-1, VSYN+VBP+HEIGHT+VFP-1); // TWCR

        LTDC(0x2C) = 0;                // BCCR black
        LTDC(0x18) = (1<<16) | (1<<0); // DEN & LTDCEN in GCR
        LTDC(0x24)[0] = 1;             // IMR in SRCR

        mcu::Pin pins [28];
        Pin::define("I15:PV14,J0,J1,J2,J3,J4,J5,J6,"     // R0..7
                    "J7,J8,J9,J10,J11,K0,K1,K2,"         // G0..7
                    "E4,J13,J14,J15,G12,K4,K5,K6,"       // B0..7
                    "I14,K7,I10,I9", pins, sizeof pins); // CLK DE HSYN VSYN

        mcu::Pin disp;
        disp.define("G12:PV9"); // fix B4, which uses alt mode 9 iso 14
        disp.define("I12:P");   // ... then define as the display-on pin

        disp = 1;

        mcu::Pin backlight;
        backlight.define("K3:U");    // turn backlight on
    }

    template <int N>
    struct FrameBuffer {
        constexpr static auto LAYER = io32<device::LTDC+0x80*N>;
        static_assert(N == 1 || N == 2);

        void init () {
            LAYER(0x04) = 0; // ~LEN in LxCR
            LAYER(0x08) = hv(HSYN+HBP+WIDTH-1, HSYN+HBP);  // LxWHPCR
            LAYER(0x0C) = hv(VSYN+VBP+HEIGHT-1, VSYN+VBP); // LxWVPCR
            LAYER(0x10) = 0;                               // LxCKCR
            LAYER(0x14) = N == 1 ? 0b101 : 0b110;          // LxPFCR L8/AL44
            LAYER(0x2C) = (uint32_t) data;                 // LxCFBAR
            LAYER(0x30) = hv(WIDTH, WIDTH+3);              // LxCFBLR
            LAYER(0x34) = HEIGHT;                          // LxCFBLNR

            if constexpr (N == 1)
                for (int i = 0; i < 256; ++i) {
                    auto r = i>>5, g = (i>>2) & 7, b = i & 3;
                    auto rgb = (r<<21)|(r<<18)|(g<<13)|(g<<10)|(b<<6)|(b<<4)|(b<<2);
                    //auto rgb = (i << 16) | (i << 8) | (i << 0); // greyscale
                    LAYER(0x44) = (i<<24) | rgb;
                }
            else
                for (int i = 0; i < 16; ++i) {
                    uint8_t r = -((i>>2)&1), g = -((i>>1)&1), b = -(i&1);
                    if (i < 8) {
                        r >>= 2; g >>= 2; b >>= 2;
                    }
                    LAYER(0x44) = (i<<24) | (r<<16) | (g<<8) | b;
                }

            LAYER(0x04) = (1<<4) | (1<<0); // CLUTEN & LEN in LxCR
            LAYER(0x1C) = 0;               // LxDCCR transparent

            LTDC(0x24)[0] = 1; // IMR in SRCR
        }

        auto& operator() (int x, int y) { return data[y][x]; }

        uint8_t data [HEIGHT][WIDTH];
    };
}
