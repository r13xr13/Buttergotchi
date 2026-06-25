#ifndef __CONNECT_MENU_H__
#define __CONNECT_MENU_H__

#include <MenuItemInterface.h>

class ConnectMenu : public MenuItemInterface {
public:
    ConnectMenu() : MenuItemInterface("Connect") {}

    void optionsMenu(void);
    void drawIcon(float scale);
    bool hasTheme() { return buttergotchiConfig.theme.connect; }
    String themePath() { return buttergotchiConfig.theme.paths.connect; }
};

#endif
