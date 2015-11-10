#include <string>
#include <iostream>
#include <iterator>
#include <sstream>
#include <fstream>
#include <map>
#include <vector>

#include "output.h"
#include "rng.h"
#include "map.h"
#include "input.h"
#include "catacharset.h"
#include "iuse_software_minesweeper.h"
#include "translations.h"

minesweeper_game::minesweeper_game()
{
    iMinY = 10;
    iMinX = 10;

    iMaxY = FULL_SCREEN_HEIGHT - 2;
    iMaxX = FULL_SCREEN_WIDTH - 2;

    iOffsetX = 0;
    iOffsetY = 0;
}

void minesweeper_game::new_level(WINDOW *w_minesweeper)
{
    for (int iY = 0; iY < iMaxY; iY++) {
        mvwputch(w_minesweeper, 1 + iY, 1, c_black, std::string(iMaxX, ' '));
    }

    mLevel.clear();
    mLevelReveal.clear();

    auto set_num = [&](const std::string sType, int &iVal, const int iMin, const int iMax) {
        std::stringstream ssTemp;
        ssTemp << _("Min:") << iMin << " " << _("Max:") << " " << iMax;

        do {
            if ( iVal < iMin || iVal > iMax ) {
                iVal = iMin;
            }

            iVal = std::atoi(string_input_popup(sType.c_str(), 5, std::to_string(iVal), ssTemp.str().c_str(), "", -1, true).c_str());
        } while( iVal < iMin || iVal > iMax);
    };

    iLevelY = iMinY;
    iLevelX = iMinX;

    set_num(_("Level width:"), iLevelX, iMinX, iMaxX);
    set_num(_("Level height:"), iLevelY, iMinY, iMaxY);

    iBombs = iLevelX * iLevelY * 0.2;

    iMinBombs = iBombs;
    iMaxBombs = iLevelX * iLevelY * 0.8;

    set_num(_("Number of bombs:"), iBombs, iMinBombs, iMaxBombs);

    iOffsetX = ((iMaxX - iLevelX) / 2) + 1;
    iOffsetY = ((iMaxY - iLevelY) / 2) + 1;

    int iRandX;
    int iRandY;
    for ( int i = 0; i < iBombs; i++ ) {
        do {
            iRandX = rng(0, iLevelX - 1);
            iRandY = rng(0, iLevelY - 1);
        } while ( mLevel[iRandY][iRandX] == (int)bomb );

        mLevel[iRandY][iRandX] = (int)bomb;
    }

    for ( int y = 0; y < iLevelY; y++ ) {
        for ( int x = 0; x < iLevelX; x++ ) {
            if (mLevel[y][x] == (int)bomb) {
                const auto circle = closest_tripoints_first( 1, {x, y, 0} );

                for( const auto &p : circle ) {
                    if ( p.x >= 0 && p.x < iLevelX && p.y >= 0 && p.y < iLevelY ) {
                        if ( mLevel[p.y][p.x] != (int)bomb ) {
                            mLevel[p.y][p.x]++;
                        }
                    }
                }
            }
        }
    }

    for (int y = 0; y < iLevelY; y++) {
        mvwputch(w_minesweeper, iOffsetY + y, iOffsetX, c_white, std::string(iLevelX, '#'));
    }

    mvwputch(w_minesweeper, iOffsetY, iOffsetX, hilite(c_white), "#");
}

bool minesweeper_game::check_win()
{
    for ( int y = 0; y < iLevelY; y++ ) {
        for ( int x = 0; x < iLevelX; x++ ) {
            if ((mLevelReveal[y][x] == flag || mLevelReveal[y][x] == unknown) && mLevel[y][x] != (int)bomb) {
                return false;
            }
        }
    }

    return true;
}

int minesweeper_game::start_game()
{
    const int iCenterX = (TERMX > FULL_SCREEN_WIDTH) ? (TERMX - FULL_SCREEN_WIDTH) / 2 : 0;
    const int iCenterY = (TERMY > FULL_SCREEN_HEIGHT) ? (TERMY - FULL_SCREEN_HEIGHT) / 2 : 0;

    WINDOW *w_minesweeper = newwin(FULL_SCREEN_HEIGHT, FULL_SCREEN_WIDTH, iCenterY, iCenterX);
    WINDOW_PTR w_minesweeperptr( w_minesweeper );

    draw_border(w_minesweeper);

    std::vector<std::string> shortcuts;
    shortcuts.push_back(_("<n>ew level"));
    shortcuts.push_back(_("<f>lag"));
    shortcuts.push_back(_("<q>uit"));

    int iWidth = 0;
    for( auto &shortcut : shortcuts ) {
        if ( iWidth > 0 ) {
            iWidth += 1;
        }
        iWidth += utf8_width(shortcut);
    }

    int iPos = FULL_SCREEN_WIDTH - iWidth - 1;
    for( auto &shortcut : shortcuts ) {
        shortcut_print(w_minesweeper, 0, iPos, c_white, c_ltgreen, shortcut);
        iPos += utf8_width(shortcut) + 1;
    }

    mvwputch(w_minesweeper, 0, 2, hilite(c_white), _("Minesweeper"));

    input_context ctxt("MINESWEEPER");
    ctxt.register_cardinal();
    ctxt.register_action("NEW");
    ctxt.register_action("FLAG");
    ctxt.register_action("CONFIRM");
    ctxt.register_action("QUIT");
    ctxt.register_action("HELP_KEYBINDINGS");

    new_level(w_minesweeper);

    static const std::array<int, 9> aColors = {
        c_white,
        c_ltgray,
        c_cyan,
        c_blue,
        c_ltblue,
        c_green,
        c_magenta,
        c_red,
        c_yellow
    };

    int iScore = 5;

    int iPlayerY = 0;
    int iPlayerX = 0;

    std::function<void (int, int)> rec_reveal = [&](const int y, const int x) {
        if ( mLevelReveal[y][x] == unknown || mLevelReveal[y][x] == flag ) {
            mLevelReveal[y][x] = seen;

            if ( mLevel[y][x] == 0 ) {
                const auto circle = closest_tripoints_first( 1, {x, y, 0} );

                for( const auto &p : circle ) {
                    if ( p.x >= 0 && p.x < iLevelX && p.y >= 0 && p.y < iLevelY ) {
                        if ( mLevelReveal[p.y][p.x] != seen ) {
                            rec_reveal(p.y, p.x);
                        }
                    }
                }

                mvwputch(w_minesweeper, iOffsetY + y, iOffsetX + x, c_black, " ");

            } else {
                mvwputch(w_minesweeper, iOffsetY + y, iOffsetX + x,
                         (x == iPlayerX && y == iPlayerY) ? hilite(aColors[mLevel[y][x]]) : aColors[mLevel[y][x]],
                         std::to_string(mLevel[y][x]));
            }
        }
    };

    int iDirY, iDirX;

    std::string action;

    do {
        wrefresh(w_minesweeper);

        if (check_win()) {
            popup_top(_("Congratulations, you won!"));

            iScore = 30;

            action = "QUIT";
        } else {
            action = ctxt.handle_input();
        }

        if (ctxt.get_direction(iDirX, iDirY, action)) {
            if ( iPlayerX + iDirX >= 0 && iPlayerX + iDirX < iLevelX && iPlayerY + iDirY >= 0 && iPlayerY + iDirY < iLevelY ) {

                std::string sGlyph;
                nc_color cColor;

                for ( int i=0; i < 2; i++ ) {
                    cColor = c_white;
                    if ( mLevelReveal[iPlayerY][iPlayerX] == flag ) {
                        sGlyph = "!";
                        cColor = c_yellow;
                    } else if ( mLevelReveal[iPlayerY][iPlayerX] == seen ) {
                        if ( mLevel[iPlayerY][iPlayerX] == 0 ) {
                            sGlyph = " ";
                            cColor = c_black;
                        } else {
                            sGlyph = std::to_string(mLevel[iPlayerY][iPlayerX]);
                            cColor = aColors[mLevel[iPlayerY][iPlayerX]];
                        }
                    } else {
                        sGlyph = '#';
                    }

                    mvwputch(w_minesweeper, iOffsetY + iPlayerY, iOffsetX + iPlayerX, (i == 0) ? cColor : hilite(cColor), sGlyph.c_str());

                    if ( i == 0 ) {
                        iPlayerX += iDirX;
                        iPlayerY += iDirY;
                    }
                }
            }
        } else if (action == "FLAG") {
            if ( mLevelReveal[iPlayerY][iPlayerX] == unknown ) {
                mLevelReveal[iPlayerY][iPlayerX] = flag;
                mvwputch(w_minesweeper, iOffsetY + iPlayerY, iOffsetX + iPlayerX, hilite(c_white), "!");

            } else if ( mLevelReveal[iPlayerY][iPlayerX] == flag ) {
                mLevelReveal[iPlayerY][iPlayerX] = unknown;
                mvwputch(w_minesweeper, iOffsetY + iPlayerY, iOffsetX + iPlayerX, hilite(c_white), "#");
            }
        } else if (action == "CONFIRM") {
            if ( mLevelReveal[iPlayerY][iPlayerX] != seen ) {
                if ( mLevel[iPlayerY][iPlayerX] == (int)bomb ) {
                    for ( int y = 0; y < iLevelY; y++ ) {
                        for ( int x = 0; x < iLevelX; x++ ) {
                            if (mLevel[y][x] == (int)bomb) {
                                mvwputch(w_minesweeper, iOffsetY + y, iOffsetX + x, hilite(c_red), (mLevelReveal[y][x] == flag) ? "!" : "*" );
                            }
                        }
                    }

                    wrefresh(w_minesweeper);

                    popup_top(_("Boom, you're dead! Better luck next time."));
                    action = "QUIT";

                } else {
                    rec_reveal(iPlayerY, iPlayerX);
                }
            }

        } else if (action == "NEW") {
            new_level(w_minesweeper);

            iPlayerY = 0;
            iPlayerX = 0;
        }

    } while (action != "QUIT");

    return iScore;
}
