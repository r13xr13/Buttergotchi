#ifndef __BUTTERGOTCHI_MENU_H__
#define __BUTTERGOTCHI_MENU_H__

#include <MenuItemInterface.h>

class ButtergotchiMenu : public MenuItemInterface {
public:
    ButtergotchiMenu() : MenuItemInterface("Buttergotchi") {}

    void optionsMenu(void);
    void drawIcon(float scale);
    bool hasTheme() { return false; }
    String themePath() { return ""; }
};

#endif
