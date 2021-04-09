// shorthand to set both horizontal and vertical parameters
constexpr auto hv = [](auto h, auto l) { return (h << 16) | l; };

// RK043FN48H
enum { WIDTH=480,HEIGHT=272, HSYN=41,HBP=13,HFP=32,VSYN=10,VBP=2,VFP=2 };

template <int N>
struct FrameBuffer {
    constexpr static auto LTDC = io32<device::LTDC>;
    constexpr static auto LAYER = io32<device::LTDC+0x80*N>;

    void init () {
        LAYER(0x04) = 0; // ~LEN in LxCR
        LAYER(0x08) = hv(HSYN+HBP+WIDTH-1, HSYN+HBP);  // LxWHPCR
        LAYER(0x0C) = hv(VSYN+VBP+HEIGHT-1, VSYN+VBP); // LxWVPCR
        LAYER(0x10) = 0;                               // LxCKCR
        LAYER(0x14) = 0b101;                           // LxPFCR L8
        LAYER(0x2C) = (uint32_t) data;                 // LxCFBAR
        LAYER(0x30) = hv(WIDTH, WIDTH+3);              // LxCFBLR
        LAYER(0x34) = HEIGHT;                          // LxCFBLNR

        for (int i = 0; i < 256; ++i) {
            auto rgb = ((i&0xE0) << 16) | ((i&0x1C) << 11) | ((i&0x03) << 6);
            //auto rgb = (i << 16) | (i << 8) | (i << 0); // greyscale
            LAYER(0x44) = (i<<24) | rgb;
        }

        LAYER(0x04) = (1<<4) | (1<<0); // CLUTEN & LEN in LxCR
        LAYER(0x1C) = 0;               // LxDCCR transparent

        LTDC(0x24)[0] = 1; // IMR in SRCR
    }

    uint8_t data [HEIGHT][WIDTH];
};

void lcdTest () {
    // FIXME RCC name clash mcb/device
    constexpr auto RCC  = io32<0x4002'3800>;
    constexpr auto LTDC = io32<device::LTDC>;

    RCC(0x44)[26] = 1;            // LTDCEN in APB2ENR

    RCC(0x00)[28] = 0;            // ~PLLSAION in CR
    RCC(0x88).mask(6, 9) = 192;   // PLLSAIN in PLLSAICFGR
    RCC(0x88).mask(28, 3) = 5;    // PLLSAIR in PLLSAICFGR
    RCC(0x88).mask(16, 2) = 1;    // PLLSAIDIVR in DKCFGR1
    RCC(0x00)[28] = 1;            // PLLSAION in CR
    while (RCC(0x00)[29] == 0) {} // wait for PLLSAIRDY in CR

    LTDC(0x08) = hv(HSYN-1, VSYN-1);                              // SSCR
    LTDC(0x0C) = hv(HSYN+HBP-1, VSYN+VBP-1);                      // BPCR
    LTDC(0x10) = hv(HSYN+HBP+WIDTH-1, VSYN+VBP+HEIGHT-1);         // AWCR
    LTDC(0x14) = hv(HSYN+HBP+WIDTH+HFP-1, VSYN+VBP+HEIGHT+VFP-1); // TWCR

    LTDC(0x2C) = 0;    // BCCR black
    LTDC(0x18) = 1;    // LTDCEN in GCR
    LTDC(0x24)[0] = 1; // IMR in SRCR

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

    static FrameBuffer<1> fb;
    fb.init();

    for (int y = 0; y < HEIGHT; ++y)
        for (int x = 0; x < WIDTH; ++x)
            fb.data[y][x] = x ^ y;
}
