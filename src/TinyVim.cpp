#include "TinyVim.h"
#include <TinyStreaming.h>

namespace tiny_vim
{

void error(const char* err)
{
  Term << TinyTerm::red << "error: " << err << TinyTerm::white << endl;
}

Vim::Vim(TinyTerm* term, std::string args)
  : TinyApp(term), splitter('h', term->sy-2)
{
  curwid=0xC000; // above splitter
  if (term==nullptr or not term->isTerm() or term->sx==0 or term->sy==0)
  {
    terminate();
    return;
  }
  term->getTermSize();
  // TODO should be a loop on all args
  *term << "vim started (" << args << ")" << endl;
  buffers[args].read(args.c_str());
  buffers[args].addWindow(curwid);
  buffers[args].redraw(curwid, term, &splitter);
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
  lines_=0;
  buffer.clear();
  cr1=cr2=0;
  modified_=false;
  filename_.clear();
}

void Buffer::redraw(Wid wid, TinyTerm* term, Splitter* splitter)
{
  Window win(1,1,term->sx, term->sy);
  if (splitter->calcWindow(wid, win))
  {

  }
  else
    error("Buffer::redraw");
}

bool Buffer::read(const char* filename)
{
  reset();
  filename_ = filename;
  File file = LittleFS.open(filename, "r");
  if (!file)
  {
    error("Unable to open file");
    return false;
  }
  std::string s;
  while (file.available())
  {
    auto c = file.read();
    if (c==13 or c==10)
    {
      if (cr1==0) cr1=c;
      if (c==cr1)
        buffer[++lines_] = s;
      else if (cr2==0)
        cr2=c;
      else if (c!=cr2)
        error("bad eol");
      s.clear();
    }
    else
      s += (char)c;
  }
  if (s.length()) buffer[++lines_] = s; // (no eol)
  return true;
}

void Vim::onKey(TinyTerm::KeyCode key)
{
  if (key==TinyTerm::KEY_CTRL_C) terminate();
  Term << F("vim key ") << key << endl;
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

bool Splitter::split(Wid wid, Window from, bool vertical, bool on_side_1, uint16_t size)
{
  Term << "--- split " << hex(wid) << ", sz=" << size << ", side=" << on_side_1 << endl;
  Splitter* splitter=this;
  if (calcWindow(wid, from, splitter))
  {
    Term << "splitting " << from << endl;
    if ((vertical and from.width > size) or (not vertical and from.height > size))
    {
      // FIXME if side exists, vertical should be side.vertical
      if (on_side_1)
      {
        if (side_1) side_1->split_.size = size;
        else side_1 = new Splitter(vertical, size);
      }
      else
      {
        if (side_0) side_0->split_.size = size;
        else side_0 = new Splitter(vertical, size);
      }
      return true;
    }
  }

  Term << "split fail (bad win ?)" << endl;
  return false;
}

void Splitter::draw(Window win, Wid wid)
{
  Wid win_bit = wid-(wid & (wid-1)); // (last bit of cur_wid)
  wid |= win_bit>>1;
  Wid wid_0 = wid & ~win_bit;
  Wid wid_1 = wid;
  TypeSize& split = split_;
  auto printWid = [](const Window& win, Wid wid)
  {
      Term.gotoxy(win.top+win.height/2,win.left+win.width/2-4);
      Term << ' ' << hex(wid) << ' ';
      Term.gotoxy(win.top+win.height/2+1,win.left+win.width/2-6);
      Term << ' ' << win << ' ';
  };
  if (split.vertical)
  {
    Term.gotoxy(win.top, win.left+split.size);
    for(int i=win.top; i < win.top+win.height; i++)
    {
      Term << "\u2502\033[1B\033[1D";
    }

    Window w1(win.top, win.left, split.size, win.height);
    if (side_1)
      side_1->draw(w1, wid_1);
    else printWid(w1, wid_1);

    win.left += split.size + 1;
    win.width += split.size - 1;
    if (side_0)
      side_0->draw(win, wid_0);
    else printWid(win, wid_0);
  }
  else
  {
    Term.gotoxy(win.top+split.size, win.left);
    for(int i=0; i < win.width; i++) Term << "\u2500";
    Term << endl;

    Window w1(win.top, win.left, win.width, split.size);
    if (side_1)
      side_1->draw(w1, wid_1);
    else printWid(w1, wid_1);

    win.height += - split.size - 1;
    win.top += split.size+1;
    if (side_0)
      side_0->draw(win, wid_0);
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
  Wid win_bit = wid-(wid & (wid-1)); // (last bit of cur_wid)
  wid |= win_bit>>1;
  Wid wid_0 = wid & ~win_bit;
  Wid wid_1 = wid;
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

void Splitter::dump(Window from, std::string indent, Wid cur_wid)
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

void WindowBuffer::draw(const Window& win, TinyTerm& term, Buffer& buff)
{
}

void WindowBuffer::focus(TinyTerm& term)
{
  term.gotoxy(cursor.row, cursor.col);
}

}