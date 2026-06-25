#ifndef __MAIN_MENU_H__
#define __MAIN_MENU_H__

#include <MenuItemInterface.h>

#include "menu_items/ButtergotchiMenu.h"
#include "menu_items/ConfigMenu.h"
#include "menu_items/ConnectMenu.h"
#include "menu_items/FileMenu.h"
#include "menu_items/WifiMenu.h"
class MainMenu {
public:
    ButtergotchiMenu buttergotchiMenu;
    WifiMenu wifiMenu;
    ConnectMenu connectMenu;
    ConfigMenu configMenu;
    FileMenu fileMenu;

    MainMenu();
    ~MainMenu();

    void begin(void);
    std::vector<MenuItemInterface *> getItems(void) { return _menuItems; }
    void hideAppsMenu();

private:
    int _currentIndex = 0;
    int _totalItems = 0;
    std::vector<MenuItemInterface *> _menuItems;
};
extern MainMenu mainMenu;

#endif
