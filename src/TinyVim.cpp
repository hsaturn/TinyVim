#include "string_util.h"
#include "file_util.h"
#include <TinyStreaming.h>
#include <TinyTerm.h>
#include "TinyVim.h"

namespace tiny_vim
{

static std::map<std::string, int> pos;
void vim_debug(std::string key)
{
  int row = pos[key];
  if (row==0)
  {
    row = 10+pos.size();
    pos[key]=row;
  }
  Term << TinyTerm::hide_cur;
  Term.saveCursor().gotoxy(row,60);
}

#if 1
#define vdebug(key, all)
#else
#define vdebug(key, all) \
{ vim_debug(key); \
Term << key << ' ' << all << "       "; \
Term.restoreCursor(); \
Term << TinyTerm::show_cur; }
#endif

void error(const char* err)
{
  Term << TinyTerm::red << "Error: " << err << TinyTerm::white << endl;
}

// Search index of string in indexes
// return 0..n index of string
//    or -1 not found
//    or -2 found but not terminated
int16_t getIndex(const char* haystack, const char* needle)
{
  const char* hayPtr(haystack);
  int16_t index = 0;

  while (*hayPtr) {
      const char* needlePtr = needle;

      while (*needlePtr and (*hayPtr == *needlePtr))
      { ++hayPtr; ++needlePtr; }
      if ((*hayPtr==0 or *hayPtr==',' or *hayPtr==':') and *needlePtr==0)
        return index;
      if (*hayPtr==':') { hayPtr++; continue; }
      if (*needlePtr==0) return -2;

      while (*hayPtr && *hayPtr != ',') ++hayPtr;
      if (*hayPtr) hayPtr++;

      ++index;
  }

  return -1;
}

Action Vim::getAction(const char* action)
{
  int16_t index=getIndex(actions, action);
  if (index==-1) return Action::VIM_UNKNOWN;
  if (index==-2) return Action::VIM_UNTERMINATED;
  return (Action)index;
}

void Vim::redraw()
{
  term->clear();
  drawSplitter();
  Window win(1, 1, term->sx, term->sy);
  splitter.forEachWindow(win, [this](const Window& win, Wid wid, const Splitter* split)
  {
    WindowBuffer* wbuff = getWBuff(wid);
    if (wbuff)
    {
      wbuff->draw(win, *term);
      if (wid==curwid) wbuff->focus(*term);
    }
    return true;
  });
}

Vim::Vim(TinyTerm* term, const tiny_bash::TinyEnv& e, string args)
  : TinyApp(term,e)
  , splitter('h', term->sy-3), term(term)
{
  Wid unused_side_0;

  if (term==nullptr or not term->isTerm() or term->sx==0 or term->sy==0)
  {
    terminate();
    return;
  }
  // TODO Should be done periodically
  term->saveCursor();
  term->getTermSize();
  term->restoreCursor();

  char orientation = 'v';
  bool first_split = true;
  trim(args);
  auto rows=term->sy-3;
  auto cols=term->sx;
  WindowBuffer* last_wbuff = nullptr;
  curwid=0xC000;
  while(args.length())
  {
    std::string arg=getWord(args);
    if (arg[0]=='+')
    {
      if (last_wbuff) last_wbuff->gotoxy(getInt(arg));
    }
    else
    {
      std::string file(getFile(env.cwd, arg));
      if (buffers.find(file)==buffers.end())
      {
        if (first_split)
        {
          first_split = false;
        }
        else
        {
          if (orientation=='h')
            rows /= 2;
          else
            cols /= 2;
          auto size = (orientation ? rows : cols);
          splitter.split(curwid, orientation, size);

          orientation = orientation == 'h' ? 'v' : 'h';
          Window::calcSplitWids(curwid, unused_side_0, curwid);
        }
        buffers[file].read(file.c_str());
        buffers[file].setFileName(file.c_str());
        last_wbuff = buffers[file].addWindow(curwid);
     //   buffers[file].redraw(curwid, term, &splitter);
      }
    }
  }
  buffers[":"].addWindow(0x4000);
  redraw();
}

void Vim::drawSplitter()
{
  Window split_win(1,1,term->sx, term->sy);
  splitter.draw(split_win, *term);
}

void Vim::loop()
{

}

void Window::frame(TinyTerm& term)
{
  term.saveCursor();
  auto line=[&term, this]() {
    for(int i=0; i<width; i++) term << "\u2500";
  };
  auto side=[&term, this]() {
    for(int i=0; i<height; i++) term << "\u2502\033[1B\033[1D";
  };
  uint16_t has_right = left + width;
  if (has_right > term.sx) has_right = 0;
  uint16_t has_bottom = top + height;
  if (has_bottom > term.sy) has_bottom = 0;

  if (top>1)
  {
    if (left>1) {
      term.gotoxy(top-1,left-1); term << F("\u250C");
    }
    else
      term.gotoxy(top-1, left);
    line();
    if (has_right) term << F("\u2510");
  }
  if (left>1)
  {
    term.gotoxy(top,left-1);
    side();
  }
  if (has_right)
  {
    term.gotoxy(top, has_right);
    side();
  }
  if (has_bottom)
  {
    if (left>1) {
      term.gotoxy(has_bottom, left-1);
      term << F("\u2514");
    }
    else
      term.gotoxy(has_bottom, left);
    line();
    if (has_right) term << F("\u2518");
  }
  term.restoreCursor();
}

void Buffer::reset()
{
  buffer.clear();
  cr1=cr2=0;
  modified_=false;
  filename_.clear();
}

void WindowBuffer::gotoxy(uint16_t row, uint16_t col)
{
  cursor.row=row;
  cursor.col=col;
}

WindowBuffer* Buffer::addWindow(Wid wid)
{
  if (getWBuff(wid)==nullptr)
  {
    wbuffs.emplace(wid, std::unique_ptr<WindowBuffer>(new WindowBuffer(*this)));
    return wbuffs[wid].get();
  }
  return nullptr;
}

WindowBuffer* Buffer::getWBuff(Wid wid)
{
  auto it=wbuffs.find(wid);
  if (it == wbuffs.end()) return nullptr;

  return it->second.get();
}

Cursor::type Buffer::lines() const
{
  if (buffer.size() == 0) return 0;
  return buffer.rend()->first;
}

std::string Buffer::deleteLine(Cursor::type line)
{
  Cursor::type last=lines();
  if (line>last) return "";
  std::string s=buffer[line];
  while(line<last)
  {
    buffer[line]=buffer[line+1];
    line++;
  }
  buffer.erase(line);
  return s;
}

void Buffer::insertLine(Cursor::type line)
{
  Cursor::type last=lines();
  std::string s=getLine(line);
  takeLine(line).clear();
  while(line<=last)
    std::swap(takeLine(++line), s);
}

string& Buffer::takeLine(Cursor::type line)
{
  modified_ = true;
  return buffer[line];
}

const string& Buffer::getLine(Cursor::type line) const
{
  static string empty;
  auto it=buffer.find(line);
  if (it != buffer.end()) return it->second;
  return empty;
}

void Buffer::redraw(Wid wid, TinyTerm* term, Splitter* splitter)
{
  Window win(1,1,term->sx, term->sy);
  if (splitter->calcWindow(wid, win))
  {
    auto wit=wbuffs.find(wid);
    if (wit!=wbuffs.end())
    {
      wit->second->draw(win, *term);
      wit->second->focus(*term);
    }
  }
  else
    error("Buffer::redraw");
}

bool Buffer::read(const char* filename)
{
  uint16_t lines = 1;
  File file = FILE_SYSTEM.open(filename, "r");
  if (!file)
  {
    error("Unable to open file");
    return false;
  }
  string s;
  while (file.available())
  {
    auto c = file.read();
    if (c==13 or c==10)
    {
      if (cr1==0) cr1=c;
      if (c==cr1)
        buffer[lines++] = s;
      else if (cr2==0)
        cr2=c;
      else if (c!=cr2)
        error("bad eol");
      s.clear();
    }
    else
      s += (char)c;
    if (lines==0)
    {
      error("Document too long (don't save it)");
    }
 }
  if (s.length()) buffer[lines++] = s; // (no eol)
  return true;
}

bool Buffer::save(std::string filename, bool force)
{
  if (filename.length()==0) { filename = filename_; force=true; }
  if (filename.length())
  {
    if (cr1==0) { cr1=13; cr2=10; }
    Term << "TRYING " << filename << ", f=" << force << endl;
    if (force or not FILE_SYSTEM.exists(filename.c_str()))
    {
      File file=FILE_SYSTEM.open(filename.c_str(), "w");
      if (not file) return false;
      Term << "WRITING " << (file ? 1 : 0) << ' ' << lines() << " lines" << endl;
      for(Cursor::type l=1; l<=lines(); l++)
      {
        Term.clear();
        Term << "WRITE " << (int)l << ' ' << getLine(l) << '.' << endl;
        file << getLine(l).c_str() << cr1;
        if (cr2) file << cr2;
      }
      modified_ = false;
      return true;
    }
    else
      error("File exists");
  }
  return false;
}

bool WindowBuffer::save(const std::string& filename, bool force)
{
  return buff.save(filename, force);
}

void Vim::error(const char* err)
{
  tiny_vim::error(err); // FIXME
}

WindowBuffer* Vim::getWBuff(Wid wid)
{
  for(auto &buff: buffers)
  {
    WindowBuffer* wbuff = buff.second.getWBuff(wid);
    if (wbuff) return wbuff;
  }
  return nullptr;
}

void Vim::setMode(uint8_t mode)
{
  if (settings.mode != mode)
  {
    settings.mode = mode;

    // TODO draw status in status bar
    vdebug("mode",mode);
    // TODO draw status
  }
}

bool Vim::calcWindow(Wid wid, Window& win)
{
  win=Window(1,1, term->sx, term->sy);
  return splitter.calcWindow(wid, win);
}

void Vim::play(const Record& rec, uint8_t count)
{
  playing = true;
  while(count)
  {
    count--;
    for(auto key: rec) onKey(key);
  }
  playing = false;
}

void Vim::clip(const std::string& buff)
{
  clipboard_ = buff; 
}

char getChar(std::string& cmd)
{
  if (cmd.length()==0) return 0;
  char c=cmd[0];
  cmd.erase(0,1);
  return c;
}

bool Vim::onCommand(std::string cmd)
{
  bool ret=true;
  vdebug("EXEC", cmd << "   ");
  WindowBuffer *wbuff = getWBuff(curwid);
  while(cmd.length())
  {
    char c=getChar(cmd);
    bool force=cmd[0]=='!';
    bool ok=false;
    Term << "EVAL CMD " << (c ? (char)c : '?') << endl;
    switch (c)
    {
      case 'w':
        if (wbuff and wbuff->save(getFile(env.cwd, cmd), force))
          ok = true;
        break;
      case 'x':
        if (wbuff and wbuff->save(getFile(env.cwd, cmd), force)) terminate();
        break;
      case 'q':
        terminate();
        return true;
    }
    Term << "RAN " << c << " res=" << ok << endl;
    ret &= ok;
    if (not ok)
      error("Error in command");
  }
  vdebug("EXEC", "end exec, ret=" << ret);
  return ret;
}

void Vim::onKey(TinyTerm::KeyCode key)
{
  Action cmd = Action::VIM_UNKNOWN;
  vdebug("vimkey", "key:" << (key>31 and key<128 ? (char)key : ' ') << " (" << (int)key << "), recsize " << record.size() << ", rpt_count=" << rpt_count << ", play=" << playing << ", mode=" << settings.mode << "  ");

  if (key == TinyTerm::KEY_ESC)
  {
    record.clear(); // FIXME
    setMode(NORMAL);
    return;
  }
  else if (key==TinyTerm::KEY_LEFT) cmd=Action::VIM_MOVE_LEFT;
  else if (key==TinyTerm::KEY_RIGHT) cmd=Action::VIM_MOVE_RIGHT;
  else if (key==TinyTerm::KEY_UP) cmd=Action::VIM_MOVE_UP;
  else if (key==TinyTerm::KEY_DOWN) cmd=Action::VIM_MOVE_DOWN;
  else if (key==TinyTerm::KEY_CTRL_C)
  {
    terminate();
    return;
  }
  if (not playing)
  {
    if (settings.mode != NORMAL or key<'0' or key>'9')
      record.push_back(key);
  }

  Wid wid=settings.mode==COMMAND ? 0x4000 : curwid;
  WindowBuffer *wbuff = getWBuff(wid);
  Window win;
  if (!calcWindow(wid, win))
  {
    vdebug("vcalc_win", "bad " << hex(wid));
    wbuff=nullptr;
  }
  else
    vdebug("vcalc_win", hex(wid) << ", " << win);

  if (key == TinyTerm::KEY_CTRL_L)
  {
    redraw();
    return;
  }

  if (key == ':' and settings.mode == NORMAL)
  {
    settings.mode = COMMAND;
    scmd.clear();
    return;
  }
  else if (settings.mode == COMMAND)
  {
    // NOTE: mode command could be handled by a TinyConsole instance initialized with a virtual TinyTerm:
    // a term delimited by 0x4000 window.
    // TODO, WindowBuffer is nearly what is expected, especially clipping region.
    // WindowBuffer class should be splitted in VirtualTerm handling the window region.
    switch(key)
    {
      case TinyTerm::KEY_RETURN:
      {
        settings.mode = NORMAL;
        vdebug("COMMAND", "EXEC " << scmd);
        onCommand(scmd);
        scmd.clear();
        break;
      }
      case TinyTerm::KEY_BACK:
        if (scmd.length()) scmd.erase(scmd.length()-1,1);
        break;
      default:
        if (key>=' ' and key<=128)
          scmd += key;
        vdebug("COMMAND", scmd << "   ");
        break;
    }
    term->gotoxy(win.top, win.left);
    *term << scmd << "   ";
    term->gotoxy(win.top, win.left+scmd.length());
    return;
  }
  
  if (settings.mode == NORMAL or cmd!=Action::VIM_UNKNOWN)
  {
    if (key>='0' and key<='9' and not playing)
    {
      if (not last_was_digit) rpt_count=0;
      rpt_count = 10*rpt_count+key-'0';
      record.clear();
      vdebug("rec", rpt_count);
      last_was_digit=true;
      return;
    }
    last_was_digit=false;
    if ((key>=' ' and key<256 and key!=':'))
    {
      scmd += (char)key;
      cmd = getAction(scmd.c_str());
      vdebug("scmd", scmd << ", cmd " << (int)cmd);
      switch(cmd)
      {
        case Action::VIM_INSERT: setMode(INSERT); break;
        case Action::VIM_REPLACE: setMode(REPLACE); break;
        case Action::VIM_REPEAT: play(record, 1); break;
        case Action::VIM_UNKNOWN:
          scmd.clear();
          break;
        case Action::VIM_UNTERMINATED:
          vdebug("unterminated", scmd);
          return;
        default:
          if (wbuff)
          {
            wbuff->onAction(cmd, win, *this);
          }
          break;
      }
      scmd.clear();
      return;
    }
    else if (wbuff and cmd!=Action::VIM_UNKNOWN)
      wbuff->onAction(cmd, win, *this);
  }
  else
  {
    last_was_digit=false;
  }
  if (wbuff) wbuff->onKey(key, win, *this);
}

void Vim::onMouse(const TinyTerm::MouseEvent& e)
{
}

Splitter::Splitter(bool vertical, uint16_t size)
  : side_1(nullptr), side_0(nullptr)
{
  split_.vertical = vertical;
  split_.size = size;
}

Splitter::~Splitter()
{
  delete side_1;
  delete side_0;
}

Splitter* Splitter::split(Wid wid, char v_h, uint16_t size)
{
  bool vertical = v_h == 'v';
  bool side_1;
  Splitter* sthis=this;
  Splitter** splitter=&sthis;
  while((wid & 0x7FFF) and *splitter)
  {
    side_1 = wid & 0x8000;
    if (side_1) splitter=&((*splitter)->side_1);
    else splitter = &((*splitter)->side_0);
    wid <<=1;
  }
  if (*splitter==this) return nullptr;
  if (*splitter)
    (*splitter)->split_.size = size;
  *splitter = new Splitter(vertical, size);
  return *splitter;
}

void Window::calcSplitWids(Wid wid, Wid& wid_0, Wid& wid_1)
{
  Wid win_bit = wid-(wid & (wid-1)); // (last bit of cur_wid)
  wid |= win_bit>>1;
  wid_0 = wid & ~win_bit;
  wid_1 = wid;
}

void Splitter::draw(Window win, TinyTerm& term, Wid wid)
{
  Wid wid_0;
  Wid wid_1;
  Window::calcSplitWids(wid, wid_0, wid_1);
  TypeSize& split = split_;
  #if 0
  auto printWid = [](const Window& win, Wid wid){};
  #else
  auto printWid = [&term](const Window& win, Wid wid)
  {
      term.gotoxy(win.top+win.height/2,win.left+win.width/2-4);
      term << ' ' << hex(wid) << ' ';
      term.gotoxy(win.top+win.height/2+1,win.left+win.width/2-6);
      term << ' ' << win << ' ';
  };
  #endif
  if (split.vertical)
  {
    term.gotoxy(win.top, win.left+split.size);
    for(int i=win.top; i < win.top+win.height; i++)
    {
      term << "\u2502\033[1B\033[1D";
    }

    Window w1(win.top, win.left, split.size, win.height);
    if (side_1) side_1->draw(w1, term, wid_1);
    else printWid(w1, wid_1);

    win.left += split.size + 1;
    win.width += split.size - 1;
    if (side_0) side_0->draw(win, term, wid_0);
    else printWid(win, wid_0);
  }
  else
  {
    term.gotoxy(win.top+split.size, win.left);
    for(int i=0; i < win.width; i++) term << "\u2500";

    Window w1(win.top, win.left, win.width, split.size);
    if (side_1) side_1->draw(w1, term, wid_1);
    else printWid(w1, wid_1);

    win.height += - split.size - 1;
    win.top += split.size+1;
    if (side_0) side_0->draw(win, term, wid_0);
    else printWid(win, wid_0);
  }
}

void Splitter::dump2(Window from)
{
  forEachWindow(from,
    [](const Window& win, Wid wid, const Splitter* splitter) -> bool
    {
      Term << wid << ' ' << win << endl;
      return true;
    }
  );
}

bool Splitter::forEachWindow(Window& from,
  std::function<bool(const Window& win, Wid wid, const Splitter* cur_split)> fun,
  Wid wid)
{
  Wid wid_0;
  Wid wid_1;
  Window::calcSplitWids(wid, wid_0, wid_1);
  TypeSize& split = split_;
  Window win = from;
  if (split.vertical)
  {
    win.width = split.size;
    if (side_1)
    {
      if (not side_1->forEachWindow(win, fun, wid_1)) return false;
    }
    else if (not fun(win, wid_1, this)) return false;

    win.left = from.left+split.size+1;
    win.width = from.width-split.size - 1;
    if (side_0)
    {
      if (not side_0->forEachWindow(win, fun, wid_0)) return false;
    }
    else if (not fun(win, wid_0, this)) return false;
  }
  else
  {
    win.height = split.size;
    if (side_1)
    {
      if (not side_1->forEachWindow(win, fun, wid_1)) return false;
    }
    else if (not fun(win, wid_1, this)) return false;

    win.height = from.height - split.size - 1;
    win.top = from.top + split.size+1;
    if (side_0)
    {
      if (not side_0->forEachWindow(win, fun, wid_0)) return false;
    }
    else if (not fun(win, wid_0, this)) return false;
  }
  return true;
}

bool Splitter::calcWindow(Wid wid, Window& win, Splitter* splitter)
{
  splitter = this;
  while((wid & 0x7FFF) and splitter)
  {
    TypeSize& split = splitter->split_;
    bool side_1 = wid & 0x8000;
    if (split.vertical) {
      if (side_1) { win.left += split.size+1; win.width -= (split.size+1); }
      else { win.width = split.size; }
    }
    else {
      if (side_1) { win.height = split.size; }
      else { win.top += split.size+1; win.height -= (split.size+1); }
    };
    if (side_1)
      splitter=splitter->side_1;
    else
      splitter=splitter->side_0;
    wid <<= 1;
  }
  while(splitter)
  {
    vdebug("split", "should not be here " << hex(wid));
    if (splitter->split_.vertical)
      win.width = splitter->split_.size;
    else
      win.height = splitter->split_.size;
    splitter = splitter->side_1;
  }
  return wid==0x8000;
}

void Splitter::close(Wid)
{
}

Wid Splitter::findWindow(Window& term, const Cursor& point)
{
  Wid win=0;
  forEachWindow(term,
    [&point,&win](const Window& candidate, Wid wid, const Splitter* splitter) -> bool
    {
      if (candidate.isInside(point))
      {
        win = wid;
        return false; // exit forEachWindow
      }
      return true;  // continue
    }
  );
  return win;
}

void Splitter::dump(Window from, string indent, Wid cur_wid)
{
  Wid win_bit = cur_wid-(cur_wid & (cur_wid-1)); // (last bit of cur_wid)
  cur_wid |= win_bit>>1;
  Wid wid_0 = cur_wid & ~win_bit;
  Wid wid_1 = cur_wid;
  indent += "  ";
  Term << indent << "dump from ";
  Term << from << ' ' << (split_.vertical ? 'V' : 'H') << split_.size << endl;

  Window s1(from);
  if (split_.vertical)
  {
    s1.left += split_.size;
    s1.width -= split_.size+1;
  }
  else
  {
    s1.height = split_.size+1;
  }

  if (side_1)
  {
   Term << indent << "side_1:" << endl;
    side_1->dump(s1, indent, wid_1);
  }
  else
    Term << indent <<  "wid_1:" << hex(wid_1) << ' ' << s1 << endl;

  if (split_.vertical)
  {
    from.width = split_.size;
  }
  else
  {
    from.top += split_.size+1;
    from.height -= split_.size+1;
  }
  if (side_0)
  {
    Term << indent << "side_0:" << endl;
    side_0->dump(from, indent, wid_0);
  }
  else
    Term << indent << "wid_0:" << hex(wid_0) << ' ' << from << endl;
}

void WindowBuffer::status(const Window& win, TinyTerm& term)
{
  uint16_t title_row = win.top+win.height;
  if (title_row <= term.sy)
  {
    std::string title = buff.filename();
    title += buff.modified() ? '*' : ' ';
    int16_t col=win.left+win.width-1-title.length();
    while (col<win.left) { title.erase(0,1); col++; }
    term.gotoxy(title_row, win.left+1);
    term << ' ' << pos.row+cursor.row-1 << ' ' << pos.col+cursor.col-1 << "  ";
    term.gotoxy(title_row, col);
    term << title;
  }
}

void WindowBuffer::draw(const Window& win, TinyTerm& term, uint16_t first, uint16_t last)
{
 if (first==0)
  {
    last = first + win.height-1;
  }
  else
  {
    if (last==0) last=first;
    if (last < pos.row) return;
    if (first > pos.row+win.height) return;
    first -= pos.row;
    last -= pos.row;
  }
  if (last<first) return;
  term << TinyTerm::hide_cur << TinyTerm::save_cursor;
  for(uint16_t row=first; row<=last; row++)
  {
    term.gotoxy(win.top+row, win.left);
    string s=buff.getLine(pos.row+row);
    if (s.length()>(size_t)pos.col)
    {
      s = s.substr(pos.col-1, win.width);
      term << s;
    }
    else
      s.clear();
    if (pos.row+row > buff.lines()) s="~";
    if (win.width>(int)s.length())
      term << string(win.width-s.length(), ' ');
    yield();
  }
  status(win, term);
  term << TinyTerm::restore_cursor << TinyTerm::show_cur;
}

Cursor WindowBuffer::buffCursor() const
{
  return cursor+pos-Cursor(1,1);
}

void WindowBuffer::gotoWord(int dir, Cursor& cursor)
{
  auto isSep = [](char c) { return not(isalnum(c) or c=='_'); };
  cursor.col--;

  const std::string& s = buff.getLine(cursor.row);
  bool waitSep = not isSep(s[cursor.col]);
  while(waitSep or isSep(s[cursor.col]))
  {
    waitSep = waitSep and not isSep(s[cursor.col]);
    if (dir>0 and cursor.col==(int)s.length())
    {
      if (cursor.row==buff.lines()) { cursor.col++; return; }
      waitSep=false;
      cursor.row++;
      cursor.col=1;
    }
    else if (dir<0 and cursor.col==0)
    {
      if (cursor.row==1) { cursor.col++; return; }
      waitSep=false;
      cursor.row--;
      cursor.col = buff.getLine(cursor.row).length();
    }
    else
      cursor.col += dir;
  }
  cursor.col++;
}

void WindowBuffer::onAction(Action cmd, const Window& win, Vim& vim)
{
  Cursor buff_cur(buffCursor());
  Cursor redraw(0,0);           // (line_to_redraw / lines to redraw)
  redraw.row = buff_cur.row;    // Redraw current line by default
  Cursor del_from(0,0);
  int8_t mode=-1;

  vdebug("w.cmd", (int)cmd);
  vdebug("w.cursor", cursor);
  vdebug("w.buff_cur", buff_cur);
  vdebug("buff.lines", buff.lines());

  std::string& line=buff.takeLine(buff_cur.row);
  switch(cmd)
  {
    case Action::VIM_CHANGE:
      del_from = buff_cur;
      buff_cur.row++;
      mode=Vim::INSERT;
      break;
    case Action::VIM_PUT_BEFORE:
    case Action::VIM_PUT_AFTER:
    {
      bool after = cmd==Action::VIM_PUT_AFTER;
      std::string clip=vim.clipboard();
      size_t cr=clip.find('\r');
      if (cr!=std::string::npos)
      {
        if (not after) buff_cur.row--;
        while(clip.length())
        {
          redraw.col=buff.lines()+1;
          buff_cur.row++;
          if (buff_cur.row>buff.lines() and buff.lines())
            buff_cur.row = buff.lines();
          buff.insertLine(buff_cur.row);
          buff.takeLine(buff_cur.row) = clip.substr(0, cr-1);
          clip.erase(0,cr+1);
          cr=clip.find('\r');
          if (cr==std::string::npos) cr=clip.length();
          vdebug("crclip", '.' << clip << '.');
        }
      }
      else
      {
        if (buff_cur.col > (int)line.length()) buff_cur.col=line.length();
        line.insert(buff_cur.col - (after ? 0 : 1), clip);
        buff_cur.col += clip.length();
      }
      break;
    }
    case Action::VIM_DELETE:
      vim.clip(line.substr(buff_cur.col-1,1));
      line.erase(buff_cur.col-1,1);
      if (buff_cur.col>(int)line.length()) buff_cur.col--;
      break;
    case Action::VIM_JOIN:
    {
      std::string s=buff.deleteLine(buff_cur.row+1);
      if (line.length() and line[line.length()-1]==' ') line.erase(line.length()-1,1);
      trim(s);
      line+=' '+s;
      redraw={ buff_cur.row, buff.lines() };
      break;
    }
    case Action::VIM_COPY_WORD: break;   // FIXME
    case Action::VIM_COPY_LINE: vim.clip(line+'\r'); break;
    case Action::VIM_DELETE_LINE:
      vim.clip(line+'\r');
      buff.deleteLine(buff_cur.row);
      redraw.col=buff.lines();
      break;
    case Action::VIM_OPEN_LINE:
      buff_cur.col=1;
      buff.insertLine(++buff_cur.row);
      mode=Vim::INSERT;
      redraw.col=buff.lines();
      break;
    case Action::VIM_APPEND: mode=Vim::INSERT;
    case Action::VIM_MOVE_RIGHT: buff_cur.col++; redraw.row=0; break;
    case Action::VIM_MOVE_LEFT: buff_cur.col--; redraw.row=0; break;
    case Action::VIM_MOVE_UP: buff_cur.row--; redraw.row=0; break;
    case Action::VIM_MOVE_DOWN: buff_cur.row++; redraw.row=0; break;
    case Action::VIM_MOVE_LINE_END: buff_cur.col=line.length(); redraw.row=0; break;
    case Action::VIM_MOVE_LINE_BEGIN: buff_cur.col=1; redraw.row=0; break;
    case Action::VIM_MOVE_DOC_END: buff_cur.row=buff.lines(); break;
    case Action::VIM_CHANGE_WORD: mode=Vim::INSERT;
    case Action::VIM_DELETE_WORD: del_from = buff_cur;
    case Action::VIM_NEXT_WORD: gotoWord(1, buff_cur); break;
    case Action::VIM_PREV_WORD: gotoWord(-1, buff_cur); break;
  }
  if (mode>=0) vim.setMode(mode);
  if (del_from.row)
  {
    if (buff_cur.row==del_from.row)
    {
      vim.clip(line.substr(del_from.col));
      line.erase(del_from.col-1, buff_cur.col-del_from.col);
    }
    else
    {
      vim.clip(line.substr(del_from.col));
      line.erase(del_from.col-1);
    }
    buff_cur = del_from;
  }
  if (redraw.row) draw(win, vim.getTerm(), redraw.row, redraw.row+redraw.col);
  buff_cur -= buffCursor();
  cursor += buff_cur;
  validateCursor(win, vim);
}

void WindowBuffer::onKey(TinyTerm::KeyCode key, const Window& win, Vim& vim)
{
  uint8_t count=1;
  const VimSettings& settings(vim.settings);
  Cursor cdraw(0,0);
  Cursor buff_cur(buffCursor());

  bool edit_mode = settings.mode & Vim::EDIT_MODE;

  vdebug("key", key << " bufflines " << buff.lines());
  switch(key)
  {
    case TinyTerm::KEY_RETURN:
    {
      if (settings.mode == Vim::INSERT)
      {
        cdraw={ buff_cur.row, buff.lines()};
        buff.insertLine(buff_cur.row+1);
        string &s = buff.takeLine(buff_cur.row);
        buff_cur.row++;
        string &nl = buff.takeLine(buff_cur.row);
        while(s[nl.length()]==' ') nl += ' ';
        if (buff_cur.col < (int)s.length())
        {
          nl += s.substr(buff_cur.col-1);
          s.erase(buff_cur.col);
        }
        cursor.col = nl.length()+1;
      }
      else
        cursor.col=1;
      cursor.row++;
      break;
    }
    case TinyTerm::KEY_BACK:
    {
      std::string& line=buff.takeLine(buff_cur.row);
      if (buff_cur.col > 1)
      {
        cursor.col--;
        if (edit_mode and (int)line.length() >= buff_cur.col-1)
        {
          line.erase(buff_cur.col-2, 1);
          cdraw.row = buff_cur.row;
        }
      }
      break;
    }
    case TinyTerm::KEY_SUPPR:
    {
      std::string& line=buff.takeLine(buff_cur.row);
      if (edit_mode && buff_cur.col<=(int)line.length())
      {
        line.erase(buff_cur.col-1, 1);
        cdraw.row = buff_cur.row;
      }
      break;
    }
    case TinyTerm::KEY_HOME: pos.col=1; cursor.col=1; break;
    case TinyTerm::KEY_END: pos.col=buff.getLine(buff_cur.row).length(); break;
    case TinyTerm::KEY_CTRL_I:  // tab
      if (settings.mode) break;
      if (vim.settings.ts == 0) break;
      count = pos.col % vim.settings.ts;
      if (count==0) count=vim.settings.ts;
      break;
    default:
      if (key>=' ' && key<256 && edit_mode)
      {
        std::string& line=buff.takeLine(buff_cur.row);
        while((int)line.length()<buff_cur.col) line+=' ';
        while(count--)
        {
          if ((int)line.length()<buff_cur.col or settings.mode==Vim::INSERT)
            line.insert(buff_cur.col-1, 1, key);
          else
            line[buff_cur.col-1]=key;
        }
        cursor.col++;
        cdraw.row=buff_cur.row;
      }
      break;
  }

  if (cdraw.row) draw(win, vim.getTerm(), cdraw.row, cdraw.col);
  validateCursor(win, vim);
}

void adjust(Cursor::type& cursor, Cursor::type& pos, Cursor::type max, int scroll)
{
  vdebug("adjusting", cursor << ' ' << pos << ' ' << max << ' ' << scroll);
  int delta = 0;
  if (cursor>max)
    delta = cursor-max + scroll;
  else if (cursor <=0)
    delta = cursor+1-scroll;
  if (delta)
  {
    cursor -=delta;
    pos += delta;
  }
  auto maxcur = max-scroll;
  vdebug("adjusted 1=", delta << ' ' << cursor << ' ' << pos << ' ' << max << ' ' << scroll);
  while (cursor<scroll and cursor<maxcur and pos>1) { pos--; cursor++; }
  while (pos<1 and cursor>=1) { pos++; cursor--; }
  vdebug("adjusted 2=", delta << ' ' << cursor << ' ' << pos << ' ' << max << ' ' << scroll);
}

void WindowBuffer::validateCursor(const Window& win, Vim& vim)
{
  Cursor old_pos = pos;
  adjust(cursor.col, pos.col, win.width, vim.settings.scrolloff);
  // adjust(cursor.row, pos.row, win.height, vim.settings.scrolloff);

  if (cursor.col<=0)
  {
    pos.col += cursor.col-1;
    if (pos.col<0) pos.col=1;
    cursor.col=1;
  }
  if (cursor.row<=0)
  {
    pos.row += cursor.row-1;
    if (pos.row<0) pos.row=1;
    cursor.row=1;
  }
  if (pos.row > buff.lines()) pos.row = buff.lines();
  if (pos.row<1) pos.row=1;
  Cursor cur=buffCursor();
  auto l=buff.getLine(cur.row).length();
  if (l and pos.col>(int)l) pos.col=l;
  else if (pos.col<1) pos.col=1;
  vdebug("lines", buff.lines());
  if (old_pos != pos)
  {
    vdebug("val_draw", 'y' << pos << '/' << old_pos);
    draw(win, vim.getTerm());
  }
  else vdebug("val_draw", 'n' << pos << '/' << old_pos);
  vim.getTerm() << TinyTerm::hide_cur;
  status(win, vim.getTerm());
  vim.getTerm().gotoxy(win.top+cursor.row-1, win.left+cursor.col-1);
  vim.getTerm() << TinyTerm::show_cur;
}

void WindowBuffer::focus(TinyTerm& term)
{
  term.gotoxy(cursor.row, cursor.col);
}

}
