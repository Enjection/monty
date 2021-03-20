#include <cstdint>
#include <cstring>

namespace twodee {
    template< typename T >
    void swap (T& a, T& b)       { T t = a; a = b; b = t; }
    auto min (int a, int b)      { return a < b ? a : b; }
    auto abs (int a) -> unsigned { return a < 0 ? -a : a; }

    struct Point { int16_t x, y; };

    struct Rect : Point {
        uint16_t w, h;

        Rect () =default;
        Rect (int x, int y, unsigned w, unsigned h)
            : Point ({x, y}), w (w), h (h) {}
        Rect (Point p, unsigned w, unsigned h)
            : Point (p), w (w), h (h) {}
        Rect (Point p1, Point p2)
            : Point ({min(p1.x, p2.x), min(p1.y, p2.y)}),
            w (abs(p1.x - p2.x) + 1), h (abs(p1.y - p2.y) + 1) {}
    };

    // Bresenham's interpolations, implemented as iterators

    struct LineIter {
        LineIter (Point p1, Point p2) : x1 (p2.x-p1.x), y1 (p2.y-p1.y),
                                        steep (abs(y1) > abs(x1)) {
            if (steep) { swap(x0, y0); swap(x1, y1); }
            if (x0 > x1) { swap(x0, x1); swap(y0, y1); }
            dx = x1-x0; dy = abs(y1-y0); err = dx/2;
        }

        operator bool () const { return x0 <= x1; }

        auto next () {
            Point p {steep ? y0 : x0, steep ? x0 : y0};
            ++x0;
            err -= dy;
            if (err < 0) {
                err += dx;
                y0 += y0 < y1 ? 1 : -1;
            }
            return p;
        }

    private:
        int16_t x0 =0, y0 =0, x1, y1, err, dx, dy;
        bool steep;
    };

    struct ArcIter {
        ArcIter (unsigned r) : r (r), x (-r), y (0), err (2-2*r) {}

        operator bool () const { return x <= 0; }

        auto next () {
            Point p {x, y};
            auto e2 = err;
            if (e2 < y) {
                err += 2 * ++y + 1;
                if (-x == y && e2 <= x)
                    e2 = 0;
            }
            if (e2 > x)
                err += 2 * ++x + 1;
            return p;
        }

    private:
        int16_t r, x, y, err;
    };

    struct Font {
        // this decodes and renders fonts from u8g2
        Font (uint8_t const* p) : glyphs (p) {
            if (p != nullptr) {
                memcpy(&nGl, p, &deC - &nGl + 1);
                deG = 256 - deG;

                // get the big-endian 16-bit values
                oUp  = (p[17]<<8) + p[18];
                oLo  = (p[19]<<8) + p[20];
                o100 = (p[21]<<8) + p[22];
            }
        }

        auto at (char c) const -> uint8_t const* {
            if (glyphs != nullptr) {
                auto q = glyphs + 23 + (c >= 'a' ? oLo : c >= 'A' ? oUp : 0);
                for (int i = 0; c > q[0]; ++i)
                    if (q[1] != 0)
                        q += q[1];
                    else
                        return nullptr;
                if (c == q[0])
                    return q + 2; // start of bit fields
            }
            return nullptr;
        }

        auto width (char c) const {
            auto p = at(c);
            return p != nullptr ? p[0] & ((1<<bcW) - 1) : 0;
        }

        auto height () const -> unsigned { return asA + deG; }

        // the order of these fields must match the u8g2 font header
        uint8_t nGl, bbM, m0b, m1b, bcW, bcH, bcX, bcY, bcD,
                bbW, bbH, bbX, bbY, asA, deG, asO, deC;
    private:
        uint8_t const* glyphs;
        uint16_t oUp, oLo, o100;
    };

    struct Glyph : Rect {
        struct Run { uint8_t off, on; };

        Glyph (Font const& font) : font (font) {}

        auto select (char c) -> int {
            data = font.at(c);
            if (data == nullptr)
                return 0;

            seen = bits = fill = 0;

            w = take(font.bcW);
            h = take(font.bcH);
            x = takes(font.bcX); // left of font bbox
            y = font.asA - takes(font.bcY) - h; // top of font bbox

            return takes(font.bcD);
        }

        operator bool () const { return seen < w * h; }

        auto next () {
            if (seen == 0 || take(1) == 0) {
                run.off = take(font.m0b);
                run.on = take(font.m1b);
            }
            seen += run.off + run.on;
            return run;
        }

    private:
        auto take (int n) -> unsigned {
            if (n > fill) {
                bits |= *data++ << fill;
                fill += 8;
            }
            unsigned r = bits & ((1<<n) - 1);
            bits >>= n;
            fill -= n;
            return r;
        }

        auto takes (int n) -> int {
            return take(n) - (1 << (n-1)); // signed version
        }

        Font const& font;
        uint8_t const* data;
        uint16_t seen, bits;
        int8_t fill;
        Run run;
    };

    template< typename G >
    struct TwoDee : G {
        static void pixel (Point p, bool f =true) {
            G::pos(p);
            G::set(f);
            G::end();
        }

        // these functions pick the most suitable looping style

        static void hLine (Point p, unsigned w, bool f =true) {
            switch (G::mode) {
                case 'H': horLine(p, w, f); break;
                case 'V': for (unsigned i = 0; i < w; ++i, ++p.x)
                              pixel(p, f);
                          break;
                default:  pixFlood({p, w, 1}, f); break;
            }
        }
        static void vLine (Point p, unsigned h, bool f =true) {
            switch (G::mode) {
                case 'H': for (unsigned i = 0; i < h; ++i, ++p.y)
                              pixel(p, f);
                          break;
                case 'V': verLine(p, h, f); break;
                default:  pixFlood({p, 1, h}, f); break;
            }
        }
        static void bFill (Point p, unsigned w, unsigned h, bool f =true) {
            switch (G::mode) {
                case 'H':
                    for (unsigned i = 0; i < h; ++i, ++p.y)
                        horLine(p, w, f);
                    break;
                case 'V':
                    for (unsigned i = 0; i < w; ++i, ++p.x)
                        verLine(p, h, f);
                    break;
                default:
                    pixFlood({p, w, h}, f); break;
            }
        }

        // more functions, built on top of the above

        static void box (Point p, unsigned w, unsigned h) {
            hLine(p, w+1);
            vLine(p, h+1);
            hLine({p.x,p.y+h}, w+1);
            vLine({p.x+w,p.y}, h+1);
        }

        static void line (Point p1, Point p2) {
            if (p1.x == p2.x)
                vLine({p1.x, min(p1.y, p2.y)}, abs(p1.y - p2.y) + 1);
            else if (p1.y == p2.y)
                hLine({min(p1.x, p2.x), p1.y}, abs(p1.x - p2.x) + 1);
            else {
                LineIter it {p1, p2};
                while (it) {
                    auto [x, y] = it.next();
                    pixel(p1.x+x, p1.y+y);
                }
            }
        }

        static void dashed (Point p1, Point p2, unsigned pattern =0x55555555) {
            LineIter it {p1, p2};
            while (it) {
                auto [x, y] = it.next();
                bool f = pattern & 1;
                if (f || G::fg != G::bg) // check for transparency
                    pixel(p1.x+x, p1.y+y, f);
                pattern = (pattern >> 1) | (pattern << 31);
            }
        }

        static void triangle (Point p1, Point p2, Point p3) {
            line(p1, p2);
            line(p2, p3);
            line(p3, p1);
        }

        static void tFill (Point p1, Point p2, Point p3) {
            // TODO not 100% correct and definitely not optimal
            LineIter it {p1, p2};
            while (it) {
                auto [x, y] = it.next();
                line({p1.x+x,p1.y+y}, p3);
            }
        }

        static void circle (Point p, unsigned n) {
            ArcIter it {n};
            while (it) {
                auto [x, y] = it.next();
                pixel(p.x-x, p.y+y);
                pixel(p.x+x, p.y+y);
                pixel(p.x+x, p.y-y);
                pixel(p.x-x, p.y-y);
            }
        }

        static void cFill (Point p, unsigned n) {
            ArcIter it {n};
            while (it) {
                auto [x, y] = it.next();
                vLine({p.x-x,p.y-y}, 2*y+1);
                vLine({p.x+x,p.y-y}, 2*y+1);
            }
        }

        static void round (Point p, unsigned w, unsigned h, unsigned n) {
            hLine({p.x+n,p.y  }, w-2*n);
            hLine({p.x+n,p.y+h}, w-2*n);
            vLine({p.x  ,p.y+n}, h-2*n);
            vLine({p.x+w,p.y+n}, h-2*n);

            ArcIter it {n};
            while (it) {
                auto [x, y] = it.next();
                pixel(p.x-x+w-n, p.y+y+h-n);
                pixel(p.x+x  +n, p.y+y+h-n);
                pixel(p.x+x  +n, p.y-y  +n);
                pixel(p.x-x+w-n, p.y-y  +n);
            }
        }

        static void rFill (Point p, unsigned w, unsigned h, unsigned n) {
            bFill({p.x+n,p.y}, w-2*n+1, h+1);

            ArcIter it {n};
            while (it) {
                auto [x, y] = it.next();
                vLine({p.x-x+w-n,p.y-y+n}, h-2*n+2*y+1);
                vLine({p.x+x+n,  p.y-y+n}, h-2*n+2*y+1);
            }
        }

        // alternate parameter variants, including bounding box adjustments

        static void pixel (int x, int y, bool f =true) { pixel({x,y}, f); }

        static void box (Rect const& r)       { box(r, r.w, r.h); }
        static void box (Point p1, Point p2)  { box({p1, p2}); }

        static void bFill (Rect const& r)      { bFill(r, r.w, r.h); }
        static void bFill (Point p1, Point p2) { bFill({p1, p2}); }

        static void round (Rect const& r, unsigned n) { round(r, r.w, r.h, n); }
        static void round (Point p1, Point p2, unsigned n) { round({p1, p2}, n); }

        static void rFill (Rect const& r, unsigned n) { rFill(r, r.w, r.h, n); }
        static void rFill (Point p1, Point p2, unsigned n) { rFill({p1, p2}, n); }

        // font rendering, returns number of x-pixels "consumed"
        static auto writes (Font const& f, Point p, char const* s, int n =-1) {
            Glyph g (f);
            Rect r {p, 0, f.height()};
            if (n < 0)
                n = strlen(s);
            for (int i = 0; i < n; ++i)
                write1(g, r, s[i]);
            return r.x - p.x;
        }

    protected:
        // various ways of looping

        static void horLine (Point p, unsigned w, bool f) {
            G::pos(p);
            for (unsigned i = 0; i < w; ++i)
                G::set(f);
            G::end();
        }
        static void verLine (Point p, unsigned h, bool f) {
            G::pos(p);
            for (unsigned i = 0; i < h; ++i)
                G::set(f);
            G::end();
        }
        static void pixFlood (Rect const& r, bool f) {
            G::lim(r);
            for (int i = 0; i < r.w * r.h; ++i)
                G::set(f);
            G::end();
        }

        // this decodes and renders RLE glyphs from u8g2 fonts
        static void write1 (Glyph& g, Rect& r, char c) {
            r.w = g.select(c);

            if (G::bg != G::fg)
                bFill(r, r.w, r.h, false);

            unsigned x = r.x+g.x, y = r.y+g.y, limit = x+g.w;
            while (g) {
                auto [off, on] = g.next();
                x += off;
                while (x >= limit) {
                    x -= g.w;
                    ++y;
                }
                while (on > 0) {
                    auto step = min(on, limit - x);
                    hLine({x,y}, step);
                    x += step;
                    on -= step;
                    if (x >= limit) {
                        x -= g.w;
                        ++y;
                    }
                }
            }

            r.x += r.w;
        }
    };
}
