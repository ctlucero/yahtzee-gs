// A small QuickDraw II look-alike: a 320x200, 16-colour indexed screen with
// per-scanline palettes, a window "port", pens, rounded rectangles, masks and
// bitmap text. Its algorithms match tools/qdemu.py, the toolbox emulator the
// original game was run under, so renders can be compared pixel for pixel.
#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace gs {

struct Rect {
    int top = 0, left = 0, bottom = 0, right = 0;
    bool contains(int h, int v) const { return h >= left && h < right && v >= top && v < bottom; }
};
inline Rect makeRect(int left, int top, int right, int bottom) { return {top, left, bottom, right}; }

class Font {
public:
    explicit Font(const char* source);         // parses the assets/fonts text format
    int ascent() const { return ascent_; }
    int descent() const { return descent_; }
    int advance(unsigned char c) const;        // glyph width + 1 spacing column
    const std::vector<std::string>& glyph(unsigned char c) const;
private:
    int ascent_ = 7, descent_ = 2;
    std::array<std::vector<std::string>, 128> glyphs_;
};

const Font& systemFont();                       // GS8

// QuickDraw transfer modes (low 3 bits). Text modes 4..7 are the "fore"
// variants that leave background pixels alone.
enum Mode { modeCopy = 0, modeOR = 1, modeXOR = 2, modeBIC = 3, modeForeCopy = 4,
            modeForeOR = 5, modeForeXOR = 6, modeForeBIC = 7 };

class Canvas {
public:
    static constexpr int W = 320, H = 200;

    Canvas();

    // --- screen ----------------------------------------------------------
    uint8_t pixel(int x, int y) const { return pix_[y][x]; }
    void setPixel(int x, int y, uint8_t c) { pix_[y][x] = c & 15; }
    uint8_t scb(int y) const { return scb_[y]; }
    void setSCB(int y, uint8_t v) { scb_[y] = v; }
    void setColorTable(int table, const std::array<uint16_t, 16>& colors) { palettes_[table] = colors; }
    uint16_t color(int table, int index) const { return palettes_[table][index]; }
    // 0x00RRGGBB for the pixel, through its scanline's palette.
    uint32_t rgb(int x, int y) const;

    // --- port --------------------------------------------------------------
    // Drawing coordinates are local to a port whose origin is at (ox, oy)
    // on screen and which clips to its own rectangle (local coordinates).
    void setPort(int ox, int oy, const Rect& clip) { ox_ = ox; oy_ = oy; clip_ = clip; }
    void setBackColor(int c) { back_ = c; }     // also the erase pattern colour
    void setBackPattern(int solidColor) { backPat_ = solidColor; }

    // --- pen -----------------------------------------------------------
    void moveTo(int h, int v) { penX_ = h; penY_ = v; }
    void move(int dh, int dv) { penX_ += dh; penY_ += dv; }
    void lineTo(int h, int v);
    void line(int dh, int dv) { lineTo(penX_ + dh, penY_ + dv); }
    void setPenSize(int w, int h) { penW_ = w; penH_ = h; }
    int penWidth() const { return penW_; }
    int penHeight() const { return penH_; }
    void setPenMode(int m) { penMode_ = m; }
    void setSolidPenPat(int c) { penColor_ = c; }
    void setPenMask(const std::array<uint8_t, 8>& m) { penMask_ = m; }
    const std::array<uint8_t, 8>& penMask() const { return penMask_; }
    static std::array<uint8_t, 8> solidMask() { return {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; }
    static std::array<uint8_t, 8> grayMask() { return {0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55}; }

    // --- shapes -----------------------------------------------------------
    void paintRect(const Rect& r);
    void frameRect(const Rect& r);
    void eraseRect(const Rect& r);
    void paintRRect(const Rect& r, int ovalW, int ovalH);
    void frameRRect(const Rect& r, int ovalW, int ovalH);
    void paintPoints(const std::vector<std::pair<int, int>>& pts);   // a "region"

    // --- text ------------------------------------------------------------
    void setFont(const Font* f, int scale) { font_ = f; fontScale_ = scale; }
    int fontScale() const { return fontScale_; }
    void setForeColor(int c) { fore_ = c; }
    void setTextMode(int m) { textMode_ = m; }
    int textMode() const { return textMode_; }
    void setTextFace(int f) { textFace_ = f; }       // bit 0: bold
    int charWidth(unsigned char c) const;
    int stringWidth(const std::string& s) const;
    void drawChar(unsigned char c);
    void drawString(const std::string& s);
    int penH() const { return penX_; }
    int penV() const { return penY_; }

    void clear(uint8_t c);

private:
    void combine(int x, int y, int src, int mode);
    void penPixel(int x, int y);
    template <class F> void forRRect(const Rect& r, int ow, int oh, F f) const;
    bool inRRect(const Rect& r, int ow, int oh, int x, int y) const;

    uint8_t pix_[H][W];
    uint8_t scb_[H];
    std::array<std::array<uint16_t, 16>, 16> palettes_;
    int ox_ = 0, oy_ = 0;
    Rect clip_{0, 0, H, W};
    int penX_ = 0, penY_ = 0, penW_ = 1, penH_ = 1, penMode_ = 0, penColor_ = 0;
    std::array<uint8_t, 8> penMask_ = solidMask();
    int fore_ = 15, back_ = 0, backPat_ = 0, textMode_ = 0, textFace_ = 0;
    const Font* font_ = nullptr;
    int fontScale_ = 1;
};

} // namespace gs
