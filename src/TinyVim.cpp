#include "string_util.h"
#include <TinyStreaming.h>
#include <TinyTerm.h>
#include "TinyVim.h"

namespace tiny_vim
{

static std::map<const char*, int> pos;
void vim_debug(const char* key)
{
  int row = pos[key];
  if (row==0)
  {
    row = 20+pos.size();
    pos[key]=row;
  }
  Term.saveCursor().gotoxy(row,120);
}

#if 0
#define vdebug(key, all)
#else
#define vdebug(key, all) \
{ vim_debug(key); \
Term << key << ' ' << all << "       "; \
Term.restoreCursor(); }
#endif

void error(const char* err)
{
  Term << TinyTerm::red << "Error: " << err << TinyTerm::white << endl;
}

Command Vim::getCommand(const char* command)
{
  const char* cmd(commands);
  int cmdIndex = 0;

  while (*cmd) {
      const char* commandIter = command;

      while (*commandIter and (*cmd == *commandIter))
      { ++cmd; ++commandIter; }
      if ((*cmd==0 or *cmd==',') and *commandIter==0)
        return (Command)cmdIndex;
      if (*commandIter==0) return Command::VIM_UNTERMINATED;

      while (*cmd && *cmd != ',') ++cmd;
      if (*cmd) cmd++;

      ++cmdIndex;
  }

  return Command::VIM_UNKNOWN;
}

Vim::Vim(TinyTerm* term, string args)
  : TinyApp(term), splitter('h', term->sy-3), term(term)
{
  term->gotoxy(26, 100);
  *term << "TERM.sy-3=" << term->sy-3 << endl;

  Wid status;
  Window::calcSplitWids(0x8000, status, curwid);
  splitter.split(curwid, 'v', 20);
  Window::calcSplitWids(0x8000, status, curwid);
  vdebug("curwid", hex(curwid));
  // curwid=0xC000; // above splitter

  if (term==nullptr or not term->isTerm() or term->sx==0 or term->sy==0)
  {
    terminate();
    return;
  }
  term->saveCursor();
  term->getTermSize();
  term->restoreCursor();
  // TODO should be a loop on all args
  Window split_win(1,1,term->sx, term->sy);
  splitter.draw(split_win, *term);
  buffers[args].read(args.c_str());
  buffers[args].addWindow(curwid);
  buffers[args].redraw(curwid, term, &splitter);

  buffers[":"].addWindow(0x4000);
  buffers[":"].redraw(0x4000, term, &splitter);
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

void Buffer::addWindow(Wid wid)
{
  if (getWBuff(wid)==nullptr)
  {
    wbuffs.emplace(wid, std::unique_ptr<WindowBuffer>(new WindowBuffer(*this)));
  }
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
  if (line>last) line=last;
  std::string s=getLine(line);
  while(line<=last)
    std::swap(takeLine(++line), s);
}

string& Buffer::takeLine(Cursor::type line)
{
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
      wit->second->draw(win, *term);
  }
  else
    error("Buffer::redraw");
}

bool Buffer::read(const char* filename)
{
  reset();
  uint16_t lines = 1;
  filename_ = filename;
  File file = LittleFS.open(filename, "r");
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
  static int c=0;
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

void Vim::onKey(TinyTerm::KeyCode key)
{
  Command cmd = Command::VIM_UNKNOWN;
  vdebug("vimkey", "recsize " << record.size() << ", rpt_count=" << rpt_count << ", play=" << playing << ", mode=" << settings.mode << "  ");

  if (key == TinyTerm::KEY_ESC)
  {
    setMode(VISUAL);
    return;
  }
  else if (key==TinyTerm::KEY_LEFT) cmd=Command::VIM_MOVE_LEFT;
  else if (key==TinyTerm::KEY_RIGHT) cmd=Command::VIM_MOVE_RIGHT;
  else if (key==TinyTerm::KEY_UP) cmd=Command::VIM_MOVE_UP;
  else if (key==TinyTerm::KEY_DOWN) cmd=Command::VIM_MOVE_DOWN;
  else if (key==TinyTerm::KEY_CTRL_C)
  {
    terminate();
    return;
  }
  if (not playing)
  {
    if (settings.mode != VISUAL or key<'0' or key>'9')
      record.push_back(key);
  }

  vdebug("vcmd", (int)cmd);

  WindowBuffer *wbuff = getWBuff(curwid);
  Window win;
  vdebug("vcalc_win", win);
  if (!calcWindow(curwid, win))
    wbuff=nullptr;
  else
  {
    vdebug("vcalc_bad", curwid);
  }
  
  if (settings.mode == VISUAL or cmd!=Command::VIM_UNKNOWN)
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
    if ((key>='a' and key<='z') or (key>='A' and key<='Z'))
    {
      scmd += (char)key;
      cmd = getCommand(scmd.c_str());
      vdebug("scmd", scmd);
      switch(cmd)
      {
        case Command::VIM_INSERT: setMode(INSERT); break;
        case Command::VIM_REPLACE: setMode(REPLACE); break;
        case Command::VIM_REPEAT: play(record, 1); break;
        case Command::VIM_UNKNOWN:
          scmd.clear();
          break;
        case Command::VIM_UNTERMINATED:
          vdebug("unterminated", scmd);
          return;
        default:
          if (wbuff)
          {
            wbuff->execCmd(cmd, win, *this);
          }
          break;
      }
      scmd.clear();
      return;
    }
    else if (wbuff and cmd!=Command::VIM_UNKNOWN)
      wbuff->execCmd(cmd, win, *this);

    switch(key)
    {
      case 'a': setMode(INSERT); key=TinyTerm::KEY_RIGHT; break;
    }
  }
  else
  {
    last_was_digit=false;

    if (wbuff)
    {
      if (key==TinyTerm::KEY_CTRL_L)
        wbuff->draw(win, *term);
      else
        wbuff->onKey(key, win, *this);
    }
  }
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

bool Splitter::split(Wid wid, char v_h, uint16_t size)
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
  if (*splitter==this) return false;
  if (*splitter)
    (*splitter)->split_.size = size;
  *splitter = new Splitter(vertical, size);
  return true;
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
    vdebug("split", "should not be here");
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
    if (s.length()>pos.col)
    {
      s = s.substr(pos.col-1, win.width);
      term << s;
    }
    else
      s.clear();
    if (win.width>s.length())
      term << string(win.width-s.length(), ' ');
    yield();
  }
  term << TinyTerm::restore_cursor << TinyTerm::show_cur;
}

Cursor WindowBuffer::buffCursor() const
{
  return cursor+pos-Cursor(1,1);
}

void WindowBuffer::gotoWord(int dir, Cursor& cursor)
{
  auto isSep = [](char c) { return not(isalnum(c) or c=='_'); };
  bool waitSep = not isSep(buff.getLine(cursor.row)[cursor.col]);
  bool inside=true;

  const std::string& s = buff.getLine(cursor.row);
  while(waitSep or isSep(s[cursor.col-1]))
  {
    waitSep = waitSep and not isSep(s[cursor.col-1]);
    if (dir>0 and cursor.col==s.length())
    {
      if (cursor.row==buff.lines()) return;
      waitSep=false;
      cursor.row++;
      cursor.col=1;
    }
    else if (dir<0 and cursor.col==1)
    {
      if (cursor.row==1) return;
      waitSep=false;
      cursor.row--;
      cursor.col = buff.getLine(cursor.row).length();
    }
    else
      cursor.col += dir;
  }
}

void WindowBuffer::execCmd(Command cmd, const Window& win, Vim& vim)
{
  Cursor buff_cur(buffCursor());
  Cursor redraw(0,0);           // (line_to_redraw / lines to redraw)
  redraw.row = buff_cur.row;    // Redraw current line by default

  vdebug("w.pos", pos);
  vdebug("w.cursor", cursor);
  vdebug("w.buff_cur", buff_cur);

  std::string& line=buff.takeLine(buff_cur.row);
  switch(cmd)
  {
    case Command::VIM_CHANGE:
      vim.clip(line.substr(buff_cur.col));
      line.erase(buff_cur.col);
      vim.onKey(TinyTerm::KEY_INS);
      break;
    case Command::VIM_PUT_AFTER:
    {
      std::string clip=vim.clipboard();
      size_t cr=clip.find('\r');
      if (cr!=std::string::npos)
      {
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
          vdebug("cr", cr);
          vdebug("crclip", '.' << clip << '.');
        }
      }
      else
      {
        line.insert(buff_cur.col, clip);
        buff_cur.col += clip.length();
      }
      break;
    }
    case Command::VIM_DELETE:
      vim.clip(line.substr(buff_cur.col-1,1));
      line.erase(buff_cur.col-1,1);
      break;
    case Command::VIM_DELETE_LINE:
      vim.clip(line+'\r');
      buff.deleteLine(buff_cur.row);
      redraw.col=buff.lines();
      break;
    case Command::VIM_MOVE_LEFT: buff_cur.col--; redraw.row=0; break;
    case Command::VIM_MOVE_RIGHT: buff_cur.col++; redraw.row=0; break;
    case Command::VIM_MOVE_UP: buff_cur.row--; redraw.row=0; break;
    case Command::VIM_MOVE_DOWN: buff_cur.row++; redraw.row=0; break;
      break;
  }
  if (redraw.row) draw(win, vim.getTerm(), redraw.row, redraw.row+redraw.col);
  buff_cur -= buffCursor();
  vdebug("delta_cur", buff_cur);
  cursor += buff_cur;
  validateCursor(win, vim);
  Term.gotoxy(cursor.row, cursor.col);
}

void WindowBuffer::onKey(TinyTerm::KeyCode key, const Window& win, Vim& vim)
{
  Cursor cdraw(0,0);
  static constexpr TinyTerm::KeyCode zero=(TinyTerm::KeyCode)0;
  const VimSettings& settings(vim.settings);
  Cursor buff_cur(buffCursor());

  if (settings.mode==Vim::VISUAL)
  {
    switch(key)
    {
      case 'x': key=TinyTerm::KEY_SUPPR; break;
      case '$': key=TinyTerm::KEY_END; break;
      case 'J':
      {
        std::string s=buff.deleteLine(buff_cur.row+1);
        std::string& l=buff.takeLine(buff_cur.row);
        if (l.length() and l[l.length()-1]==' ') l.erase(l.length()-1,1);
        trim(s);
        l+=' '+s;
        cdraw={ buff_cur.row, buff.lines() };     
        key=zero;
        break;
      }
    }
  }
  vdebug("ket", key);
  switch(key)
  {
    case TinyTerm::KEY_RETURN:
    {
      string s = buff.getLine(buff_cur.row);
      if (settings.mode==Vim::INSERT)
      {
        buff.insertLine(buff_cur.row+1);
        buff.takeLine(buff_cur.row)=s.substr(0, buff_cur.col-1);
        buff.takeLine(buff_cur.row+1)=s.substr(buff_cur.col-1);
      }
      cursor.row++;
      cursor.col=1;
      cdraw={ buff_cur.row, buff.lines()};
    }
    case TinyTerm::KEY_HOME: pos.col=1; cursor.col=1; break;
    case TinyTerm::KEY_END: cursor.col=buff.getLine(buff_cur.row).length(); break;
   default:
      if (settings.mode==Vim::INSERT or settings.mode==Vim::REPLACE)
      {
        std::string& s=buff.takeLine(buff_cur.row);
        while(s.length()<buff_cur.col) s+=' ';
        if (s.length()<buff_cur.col or settings.mode==Vim::INSERT)
          s.insert(buff_cur.col-1, 1, key);
        else
          s[buff_cur.col-1]=key;
        cdraw.row=buff_cur.row;
        cursor.col++;
      }
      else
      {
        switch(key)
        {
          case 'd':
          {
          }
          case 'w': gotoWord(1, buff_cur); break;
          case 'b': gotoWord(-1, buff_cur); break;
          default:
            vdebug("norm key", key);
        }
      }
  }

  if (cdraw.row) draw(win, vim.getTerm(), cdraw.row, cdraw.col);
  validateCursor(win, vim);
  Term.gotoxy(cursor.row, cursor.col);
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
  if (l and pos.col>l) pos.col=l;
  else if (pos.col<1) pos.col=1;
  vdebug("lines", buff.lines());
  if (old_pos != pos)
  {
    vdebug("val_draw", 'y' << pos << '/' << old_pos);
    draw(win, vim.getTerm());
  }
  else vdebug("val_draw", 'n' << pos << '/' << old_pos);
}

void WindowBuffer::focus(TinyTerm& term)
{
  term.gotoxy(cursor.row, cursor.col);
}

}