#pragma once

// vimcmd.h
class VimCmd {
public:
  enum class Command {
      INSERT, APPEND, REPLACE, JOIN, CHANGE, CHANGE_WORD, DELETE, DELETE_WORD, PUT_AFTER,
      UNDO, REPEAT, OPEN_LINE, MOVE_LEFT, MOVE_DOWN, MOVE_UP, MOVE_RIGHT, MOVE_WORD_BACKWARD,
      MOVE_LINE_END, MOVE_DOC_END, COPY_LINE, COPY_WORD, DELETE_LINE, QUIT, SEARCH_NEXT,
      UNKNOWN, UNTERMINATED
  };

  constexpr const char* commands = "i,a,R,J,C,cw,x,p,U,.,o,h,j,k,l,w,b,$,G,yy,yw,dd,dw,q,n";

    static Command getCommandNumber(const char* command);
};


