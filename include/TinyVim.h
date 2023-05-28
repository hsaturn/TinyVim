#pragma once
#include <list>
#include <memory>
#include <map>
#include "TinyApp.h"
#include <LittleFS.h>

#define FS LittleFS


namespace tiny_vim
{

using Wid=uint16_t;
using string=std::string;

void error(const char*);

class Splitter;
class Buffer;
class Window;

struct Cursor
{
  uint16_t row;
  uint16_t col;
  Cursor() : row(1), col(1) {}
  Cursor(uint16_t row, uint16_t col) : row(row), col(col) {}
  friend Stream& operator << (Stream& out, const Cursor& c)
  {
    out << '(' << c.row << ',' << c.col << ')';
    return out;
  }
};

class WindowBuffer
{
  public:
    WindowBuffer(Buffer& buffer) : buff(buffer) {}
    void draw(const Window& win, TinyTerm& term);
    void focus(TinyTerm& term);
    void onKey(TinyTerm::KeyCode key);

  private:
    Cursor pos;     // Top left of document
    Cursor cursor;  // Cursor position
    Buffer& buff;
};

class Buffer
{
  public:

    void redraw(Wid wid, TinyTerm* term, Splitter*);

    string filename() const { return filename_; }
    void reset();
    bool read(const char* filename);
    bool save();
    const string& getLine(int line) const;
    unsigned int lines() const { return lines_; }
    bool modified() const { return modified_; }
    void addWindow(Wid wid);
    void removeWindow(Wid wid) { wbuffs.erase(wid); }
    WindowBuffer* getWBuff(Wid wid);

  private:
    std::map<Wid, WindowBuffer> wbuffs;
    std::map<unsigned int, string> buffer;
    bool modified_;
    char cr1=0; // crlf
    char cr2=0;
    unsigned int lines_;
    string filename_;
};

struct Window
{
  int16_t top;
  int16_t left;
  int16_t width;  // inner size (number of visible chars)
  int16_t height;

  Window(int16_t top, int16_t left, int16_t width, int16_t height)
  : top(top), left(left), width(width), height(height){}

  bool isInside(const Cursor& c) const
  { return (top<=c.row) and (left<=c.col) and (top+height-1>=c.row) and (left+width-1>=c.col); }

  friend Stream& operator << (Stream& out, const Window& w)
  {
    out << '[' << w.top << ',' << w.left << ' ' << w.width << 'x' << w.height << ']';
    return out;
  }

  void frame(TinyTerm& term);
};

/*
Windows are 'virtual'. A window is a wid (the wid)
When one needs a window properties, it has to use
Splitter::calcWindow()
The most significant but of wid indicates if the window is
side_1 (left/top) or side_0 (right/bottom) side of the split.
There is at least one hsplit: the topmost one with
  up: first buffer opened
  down: status bar
The window is then 'computed' by shifting the wid until wid==0x8000
The last bit of wid set to 1 indicates the end of the wid.
*/
class Splitter
{
  struct TypeSize {
    uint16_t vertical : 1;
    uint16_t size : 15; // inner size
    friend Stream& operator << (Stream& out, const TypeSize& ts)
    { out << (ts.vertical ? 'V' : 'H') << ts.size; return out; }
  };
  public:
    Splitter(bool vertical, uint16_t size);
    Splitter(const char c, uint16_t size) : Splitter(c=='v', size){}
    ~Splitter();

    /* window is input/output
       if window is not found, window.top=-1
    */
    Wid findWindow(Window&, const Cursor&);
    // care : Window is modified
    bool calcWindow(Wid, Window&, Splitter* start=nullptr);
    bool split(Wid, Window from, bool vertical, bool side_1, uint16_t size);
    void close(Wid);
    void draw(Window win, Wid wid_base=0x8000);
    bool forEachWindow(Window& from,
      std::function<bool(const Window&, Wid wid, const Splitter* cur_split)>,
      Wid wid=0x8000);

    void dump(Window, string indent="", Wid cur_wid=0x8000);
    void dump2(Window);

  private:
    TypeSize split_;
    Splitter* side_1 = nullptr; // left if vertical, up if not vertical
    Splitter* side_0 = nullptr; // right if vertical, down if not vertical
};

class Vim : public tiny_bash::TinyApp
{
  public:
    Vim(TinyTerm* term, string args);
    ~Vim() = default;

    void onKey(TinyTerm::KeyCode) override;
    void onMouse(const TinyTerm::MouseEvent&) override;

    void loop() override;

  private:
    void error(const char*);
    WindowBuffer* getWBuff(Wid);
    std::map<string, Buffer> buffers;
    Splitter splitter;
    Wid curwid;
    TinyTerm* term;
};

}

using TinyVim=tiny_vim::Vim;