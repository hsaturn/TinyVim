#pragma once
#include <list>
#include <memory>
#include <map>
#include <vector>
#include "TinyApp.h"
#include <LittleFS.h>

#define FS LittleFS


namespace tiny_vim
{

//                                       0         5            10        15         20            25 
static constexpr const char* actions = "i,a,R,J,C,cw,x,p,P,U,.,o,h,j,k,l,w,b,$,G,yy,yw,dd,dw,dt,q,0:^,n";
enum class Action {
      VIM_INSERT, VIM_APPEND, VIM_REPLACE, VIM_JOIN, VIM_CHANGE,
      VIM_CHANGE_WORD, VIM_DELETE, VIM_PUT_AFTER, VIM_PUT_BEFORE, VIM_UNDO, VIM_REPEAT,
      VIM_OPEN_LINE, VIM_MOVE_LEFT, VIM_MOVE_DOWN, VIM_MOVE_UP, VIM_MOVE_RIGHT,
      VIM_NEXT_WORD, VIM_PREV_WORD, VIM_MOVE_LINE_END, VIM_MOVE_DOC_END, VIM_COPY_LINE,
      VIM_COPY_WORD, VIM_DELETE_LINE, VIM_DELETE_WORD, VIM_QUIT, VIM_DELETE_TILL,
      VIM_MOVE_LINE_BEGIN, VIM_SEARCH_NEXT, VIM_UNKNOWN, VIM_UNTERMINATED
};

using Wid=uint16_t;
using string=std::string;

void error(const char*);

class Splitter;
class Buffer;
class Window;
class Vim;
struct VimSettings;

struct Cursor
{
  using type = int16_t;
  type row;
  type col;
  Cursor() : row(1), col(1) {}
  Cursor(type row, type col) : row(row), col(col) {}
  friend Stream& operator << (Stream& out, const Cursor& c)
  {
    out << '(' << c.row << ',' << c.col << ')';
    return out;
  }
  friend bool operator == (const Cursor& l, const Cursor& r)
  { return l.row==r.row and l.col==r.col; }
  friend bool operator != (const Cursor& l, const Cursor& r)
  { return not (l==r); }
  friend Cursor operator+(const Cursor& l, const Cursor& r)
  { return Cursor(l.row+r.row, l.col+r.col); }
  friend Cursor operator-(const Cursor& l, const Cursor& r)
  { return Cursor(l.row-r.row, l.col-r.col); }
  Cursor& operator -=(const Cursor& c)
  { row -= c.row; col -= c.col; return *this; }
  Cursor& operator +=(const Cursor& c)
  { row += c.row; col += c.col; return *this; }
};

class WindowBuffer
{
  public:
    WindowBuffer(Buffer& buffer) : pos(1,1), buff(buffer) { cursor=pos; }
    void draw(const Window& win, TinyTerm& term, uint16_t first=0, uint16_t last=0);
    void focus(TinyTerm& term);
    // returns true if end of command
    void onKey(TinyTerm::KeyCode, const Window&, Vim&);
    void onAction(Action, const Window&, Vim&);
    ~WindowBuffer() { Term << "~WindowBuffer "; }
    Cursor buffCursor() const;  // compute position in file from pos and cursor (screen)
    void gotoWord(int dir, Cursor&);
    bool save(const std::string& filename, bool force);

  private:
    void validateCursor(const Window& win, Vim& term);
    Cursor pos;     // Top left of document (min is 1,1)
    Cursor cursor;  // Cursor position (1,1 is top left)
    Buffer& buff;
};

class Buffer
{
  public:

    void redraw(Wid wid, TinyTerm* term, Splitter*);

    string filename() const { return filename_; }
    void reset();
    bool read(const char* filename);
    bool save(std::string filename, bool force);
    const string& getLine(Cursor::type line) const;
    string& takeLine(Cursor::type line);
    void insertLine(Cursor::type nr);
    std::string deleteLine(Cursor::type nr);
    Cursor::type lines() const;
    bool modified() const { return modified_; }
    void addWindow(Wid wid);
    void removeWindow(Wid wid) { wbuffs.erase(wid); }
    void setFileName(const std::string& filename) { filename_ = filename; }
    WindowBuffer* getWBuff(Wid wid);
    ~Buffer() { Term << "~Buffer "; }

  private:
    std::map<Wid, std::unique_ptr<WindowBuffer>> wbuffs;
    std::map<unsigned int, string> buffer;
    bool modified_;
    char cr1=0; // crlf
    char cr2=0;
    string filename_;
};

struct Window
{
  int16_t top;
  int16_t left;
  int16_t width;  // inner size (number of visible chars)
  int16_t height;

  Window(){};
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
  static void calcSplitWids(Wid in, Wid& wid_0, Wid& wid_1);
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
    bool split(Wid, char v_h, uint16_t size);
    void close(Wid);
    void draw(Window win, TinyTerm& term, Wid wid_base=0x8000);
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

struct VimSettings
{
  uint8_t scrolloff = 5;
  uint8_t sidescrolloff = 0;
  uint8_t mode = 0;
};

class Vim : public tiny_bash::TinyApp
{
  public:
    using Record=std::vector<TinyTerm::KeyCode>;
    enum {
      VISUAL = 0,
      COMMAND = 1,
      INSERT = 2,
      REPLACE = 3,
      EDIT_MODE = 2,  // Mask for replace or insert (editions modes)
    };


    Vim(TinyTerm* term, string args);
    ~Vim() = default;

    void onKey(TinyTerm::KeyCode) override;
    void onMouse(const TinyTerm::MouseEvent&) override;

    void loop() override;
    TinyTerm& getTerm() const { return *term; }

    VimSettings settings;

    void clip(const std::string&);
    const std::string& clipboard() const { return clipboard_; }
    void setMode(uint8_t);

  private:
    void play(const Record&, uint8_t count);
    bool calcWindow(Wid, Window&);
    void error(const char*);
    Action getAction(const char* command);

    WindowBuffer* getWBuff(Wid);
    std::map<string, Buffer> buffers;
    Splitter splitter;
    Wid curwid;
    TinyTerm* term;
    uint8_t rpt_count=0;
    bool last_was_digit=false;
    Record  record;
    bool playing=false;
    std::string scmd;
    std::string clipboard_;
};

}

using TinyVim=tiny_vim::Vim;