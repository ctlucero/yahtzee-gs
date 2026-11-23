#include "canvas.h"
#include <cmath>
#include <algorithm>
#include <cstring>
#include <sstream>

namespace gs {

// ---------------------------------------------------------------- fonts --

Font::Font(const char* source)
{
    std::istringstream in(source);
    std::string line;
    int cur = -1;
    std::vector<std::string> rows;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        bool pixels = !line.empty() && line.find_first_not_of("#.") == std::string::npos;
        if (!pixels && !line.empty() && line[0] == '#') continue;          // comment
        if (line.rfind("ascent ", 0) == 0) { ascent_ = std::stoi(line.substr(7)); continue; }
        if (line.rfind("descent ", 0) == 0) { descent_ = std::stoi(line.substr(8)); continue; }
        if (line.rfind("char ", 0) == 0 && line.size() >= 6) { cur = (unsigned char)line[5]; rows.clear(); continue; }
        if (line.rfind("code ", 0) == 0) { cur = std::stoi(line.substr(5)); rows.clear(); continue; }
        if (cur >= 0 && pixels) {
            rows.push_back(line);
            if ((int)rows.size() == ascent_ + descent_) {
                if (cur < 128) glyphs_[cur] = rows;
                cur = -1;
            }
        }
    }
}

const std::vector<std::string>& Font::glyph(unsigned char c) const
{
    if (c < 128 && !glyphs_[c].empty()) return glyphs_[c];
    return glyphs_['?'];
}

int Font::advance(unsigned char c) const { return int(glyph(c)[0].size()) + 1; }

static const char kGS8[] =
#include "gs8_font.inc"
;

const Font& systemFont()
{
    static const Font f(kGS8);
    return f;
}

// --------------------------------------------------------------- canvas --

static const std::array<uint16_t, 16> kStdPalette = {
    0x000, 0x777, 0x841, 0x72C, 0x00F, 0x080, 0xF70, 0xD00,
    0xFA9, 0xFF0, 0x0E0, 0x4DF, 0xDAF, 0x78F, 0xCCC, 0xFFF};

Canvas::Canvas()
{
    std::memset(pix_, 0, sizeof pix_);
    std::memset(scb_, 0, sizeof scb_);
    for (auto& p : palettes_) p = kStdPalette;
    font_ = &systemFont();
}

void Canvas::clear(uint8_t c) { std::memset(pix_, c & 15, sizeof pix_); }

uint32_t Canvas::rgb(int x, int y) const
{
    uint16_t c = palettes_[scb_[y] & 15][pix_[y][x]];
    return (((c >> 8) & 15) * 17u) << 16 | (((c >> 4) & 15) * 17u) << 8 | ((c & 15) * 17u);
}

void Canvas::combine(int x, int y, int src, int mode)
{
    if (!clip_.contains(x, y)) return;
    int gx = x + ox_, gy = y + oy_;
    if (gx < 0 || gx >= W || gy < 0 || gy >= H) return;
    int dst = pix_[gy][gx], v = src;
    switch (mode & 7) {
    case 0: v = src; break;
    case 1: v = dst | src; break;
    case 2: v = dst ^ src; break;
    case 3: v = dst & ~src; break;
    case 4: v = ~src; break;
    case 5: v = dst | ~src; break;
    case 6: v = dst ^ ~src; break;
    case 7: v = dst & src; break;
    }
    pix_[gy][gx] = v & 15;
}

void Canvas::penPixel(int x, int y)
{
    int gx = x + ox_, gy = y + oy_;
    if (gy < 0 || gx < 0) return;
    if (penMask_[gy % 8] & (0x80 >> (gx % 8))) combine(x, y, penColor_, penMode_);
}

void Canvas::lineTo(int x1, int y1)
{
    int x0 = penX_, y0 = penY_;
    int dx = std::abs(x1 - x0), dy = std::abs(y1 - y0);
    int sx = x1 >= x0 ? 1 : -1, sy = y1 >= y0 ? 1 : -1;
    int err = dx - dy, x = x0, y = y0;
    // Collect first so overlapping pen footprints are drawn once (XOR safe).
    std::vector<std::pair<int, int>> pts;
    for (;;) {
        for (int j = 0; j < penH_; ++j)
            for (int i = 0; i < penW_; ++i) pts.push_back({x + i, y + j});
        if (x == x1 && y == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x += sx; }
        if (e2 < dx) { err += dx; y += sy; }
    }
    std::sort(pts.begin(), pts.end());
    pts.erase(std::unique(pts.begin(), pts.end()), pts.end());
    for (auto& p : pts) penPixel(p.first, p.second);
    penX_ = x1; penY_ = y1;
}

// Row inset of a rounded rectangle, as in qdemu.rrect_pts (round half to even).
static int rrInset(int yy, int hgt, int ow, int oh)
{
    double a = ow / 2.0, bb = oh / 2.0;
    if (bb <= 0) return 0;
    double dy;
    if (yy < bb) dy = bb - yy - 0.5;
    else if (yy >= hgt - bb) dy = yy - (hgt - bb) + 0.5;
    else return 0;
    double f = 1 - (dy / bb) * (dy / bb);
    return int(std::nearbyint(a - a * std::sqrt(f > 0 ? f : 0)));
}

bool Canvas::inRRect(const Rect& r, int ow, int oh, int x, int y) const
{
    if (y < r.top || y >= r.bottom) return false;
    int in = rrInset(y - r.top, r.bottom - r.top, ow, oh);
    return x >= r.left + in && x < r.right - in;
}

template <class F> void Canvas::forRRect(const Rect& r, int ow, int oh, F f) const
{
    for (int y = r.top; y < r.bottom; ++y) {
        int in = rrInset(y - r.top, r.bottom - r.top, ow, oh);
        for (int x = r.left + in; x < r.right - in; ++x) f(x, y);
    }
}

void Canvas::paintRect(const Rect& r)
{
    for (int y = r.top; y < r.bottom; ++y)
        for (int x = r.left; x < r.right; ++x) penPixel(x, y);
}

void Canvas::frameRect(const Rect& r)
{
    Rect in{r.top + penH_, r.left + penW_, r.bottom - penH_, r.right - penW_};
    for (int y = r.top; y < r.bottom; ++y)
        for (int x = r.left; x < r.right; ++x)
            if (!(y >= in.top && y < in.bottom && x >= in.left && x < in.right)) penPixel(x, y);
}

void Canvas::eraseRect(const Rect& r)
{
    for (int y = r.top; y < r.bottom; ++y)
        for (int x = r.left; x < r.right; ++x) combine(x, y, backPat_, modeCopy);
}

void Canvas::paintRRect(const Rect& r, int ow, int oh)
{
    forRRect(r, ow, oh, [&](int x, int y) { const_cast<Canvas*>(this)->penPixel(x, y); });
}

void Canvas::frameRRect(const Rect& r, int ow, int oh)
{
    Rect in{r.top + penH_, r.left + penW_, r.bottom - penH_, r.right - penW_};
    int iow = ow - 2 * penW_ > 0 ? ow - 2 * penW_ : 0;
    int ioh = oh - 2 * penH_ > 0 ? oh - 2 * penH_ : 0;
    forRRect(r, ow, oh, [&](int x, int y) {
        if (!inRRect(in, iow, ioh, x, y)) const_cast<Canvas*>(this)->penPixel(x, y);
    });
}

void Canvas::paintPoints(const std::vector<std::pair<int, int>>& pts)
{
    for (auto& p : pts) penPixel(p.first, p.second);
}

// ----------------------------------------------------------------- text --

int Canvas::charWidth(unsigned char c) const
{
    return (font_->advance(c) + (textFace_ & 1)) * fontScale_;
}

int Canvas::stringWidth(const std::string& s) const
{
    int w = 0;
    for (unsigned char c : s) w += charWidth(c);
    return w;
}

void Canvas::drawChar(unsigned char c)
{
    const auto& g = font_->glyph(c);
    const bool bold = textFace_ & 1;
    const int gw = int(g[0].size());
    const int cellW = gw + 1 + (bold ? 1 : 0);
    const int m = textMode_ & 7, sc = fontScale_;
    for (int r = 0; r < int(g.size()); ++r) {
        for (int i = 0; i < cellW; ++i) {
            bool on = (i < gw && g[r][i] == '#') || (bold && i >= 1 && i - 1 < gw && g[r][i - 1] == '#');
            for (int dy = 0; dy < sc; ++dy)
                for (int dx = 0; dx < sc; ++dx) {
                    int x = penX_ + i * sc + dx, y = penY_ - font_->ascent() * sc + r * sc + dy;
                    if (on) combine(x, y, fore_, m & 3);
                    else if (m < 4) combine(x, y, back_, m & 3);
                }
        }
    }
    penX_ += cellW * sc;
}

void Canvas::drawString(const std::string& s)
{
    for (unsigned char c : s) drawChar(c);
}

} // namespace gs
