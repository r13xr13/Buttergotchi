#include "ButtergotchiMenu.h"
#include "modules/buttergotchi/buttergottchi.h"

void ButtergotchiMenu::optionsMenu() {
    buttergotchi_start();
}

void ButtergotchiMenu::drawIcon(float scale) {
    clearIconArea();

    // Draw a bold "B" as the Buttergotchi icon
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(scale * 5);
    tft.setTextColor(buttergotchiConfig.priColor, buttergotchiConfig.bgColor);
    tft.drawString("B", iconCenterX, iconCenterY + scale * 2);

    // Draw a subtle golden circle around it
    int radius = scale * 18;
    tft.drawCircle(iconCenterX, iconCenterY + scale * 2, radius, buttergotchiConfig.priColor);

    // Small decorative dots at 4 corners
    int dotR = scale * 2;
    int offset = scale * 22;
    tft.fillCircle(iconCenterX - offset, iconCenterY + 2 - offset, dotR, buttergotchiConfig.priColor);
    tft.fillCircle(iconCenterX + offset, iconCenterY + 2 - offset, dotR, buttergotchiConfig.priColor);
    tft.fillCircle(iconCenterX - offset, iconCenterY + 2 + offset, dotR, buttergotchiConfig.priColor);
    tft.fillCircle(iconCenterX + offset, iconCenterY + 2 + offset, dotR, buttergotchiConfig.priColor);
}
