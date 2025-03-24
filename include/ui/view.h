#pragma once
#include <ncurses.h>

class View {
protected:
    WINDOW* parent_win;
    int row;
    int col;
    
public:
    View() : parent_win(nullptr), row(0), col(0) {}
    
    void setWindow(WINDOW* win) {
        parent_win = win;
    }
    
    virtual void init() = 0;
    virtual void update() = 0;
    virtual void resize() = 0;
    
    void setPosition(int r, int c) {
        row = r;
        col = c;
    }
    
    virtual int getHeight() const { return 1; }
    virtual ~View() = default;
}; 