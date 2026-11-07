#!/bin/bash
# Scripted checks of the main screen (src/ui/console*.cpp, menu.cpp, statuswin.cpp): unit orders by
# key, mouse and menu, the go-to list and drag, save / load, the interface frame.
#
#   tests/console/run.sh [OUTDIR]        (OPENSMAC=path/to/opensmac, default build/pp-native/opensmac)
#
# Each case runs OpenSMAC with scripted keys (hidden, SDL dummy video) and checks the units the
# script lists (UNITS: "unit ID fF NAME (X,Y) order O moves M state S") and the log. Screenshots go
# to OUTDIR/<case>/. Exit status 0 when every check passes.
set -u
# Animated by default (opensmac --animate: slides, unit moves, booms, battle reports and movies play
# with their waits, as in an interactive game); TEST_FAST=1 runs unattended (skips them).
if [ "${TEST_FAST:-0}" = 1 ]; then export OPENSMAC_ANIMATE=0; else export OPENSMAC_ANIMATE=1; fi
# No audio device in the tests (sounds still run through the mixer); set SDL_AUDIODRIVER to override.
export SDL_AUDIODRIVER="${SDL_AUDIODRIVER:-dummy}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
OPENSMAC="$(realpath "${OPENSMAC:-$ROOT/build/pp-native/opensmac}")"
OUT="$(realpath -m "${1:-$ROOT/work/console_test}")"
mkdir -p "$OUT"
FAIL=0
NEW="--new size=0 --human 1 --seed 1,2"

run() {  # run CASE ARGS... : log in OUT/CASE.log
    local name="$1"; shift
    mkdir -p "$OUT/$name"
    (cd "$ROOT" && SDL_VIDEODRIVER=dummy timeout 600 "$OPENSMAC" --original-bugs --hidden --outdir "$OUT/$name" "$@" > "$OUT/$name.log" 2>&1)
    echo "$?" > "$OUT/$name.rc"
}
# check CASE DESCRIPTION REGEX: the log matches (grep -E)
check() {
    if grep -qE "$3" "$OUT/$1.log"; then echo "PASS $1: $2"; else echo "FAIL $1: $2 (/$3/)"; FAIL=1; fi
}
nocrash() {
    if [ "$(cat "$OUT/$1.rc")" = 0 ] && ! grep -q "is not reconstructed yet\|abort" "$OUT/$1.log"; then
        echo "PASS $1: ran to the end"
    else
        echo "FAIL $1: exit $(cat "$OUT/$1.rc")"; FAIL=1
    fi
}

# 1. Orders by key on the first turn (Gaians: scout 4 at 35,7 in the base, foil 5):
#    hold (h), then the foil: explore (/) ...
run keys $NEW --keys "ENTER,h,UNITS,/,UNITS,SHOT:keys.bmp"
check keys "hold: order 2" "unit 4 f1 Scout Patrol \(35,7\) order 2 "
# The same through the platform's input path (SDLKEY: as from the keyboard; the plain tokens skip
# it): a punctuation key without a virtual-key code never reached the game.
run keys_sdl $NEW --keys "ENTER,SDLKEY:/,WAIT:300,UNITS"
check keys_sdl "explore by the / key" "unit 4 f1 Scout Patrol \([0-9]+,[0-9]+\) order [0-9]+ moves [0-9]+ state 0400?4"
# The smooth map view (--smooth; ui/smoothview.h): the mouse at a screen edge (MOVE) pans it off the
# tile grid, every tile found where it is drawn, back on the grid when it stops. A two-finger scroll
# (SDLWHEEL) zooms about the pointer, a step per three units; the map point there stays put.
run smooth_wheel --new size=2 --human 1 --seed 1,2 --smooth --keys "ENTER,WAIT:50,MOVE:300x200,WAIT:50,SDLWHEEL:0/1.5,SDLWHEEL:0/1.5,WAIT:300,VIEWCHECK,SDLWHEEL:0/-3,WAIT:300,VIEWCHECK,SDLWHEEL:0/-1,WAIT:300,VIEWCHECK"
check smooth_wheel "a scroll up of three units zooms in, animated" "view zoom: [0-9]+ frames, scale 1\.[0-9]+"
check smooth_wheel "a scroll down of three units zooms out" "view zoom: [0-9]+ frames, scale 0\.[0-9]+"
if [ "$(grep -c 'view zoom' "$OUT/smooth_wheel.log")" = 2 ]; then echo "PASS smooth_wheel: one unit short of a step zooms nothing"
else echo "FAIL smooth_wheel: $(grep -c 'view zoom' "$OUT/smooth_wheel.log") zooms, 2 expected"; FAIL=1; fi
check smooth_wheel "tiles found after the wheel's zooms" "viewcheck: panning 1 offset -?[0-9]+,-?[0-9]+: [0-9]+ tiles, 0 wrong"
run smooth_edge --new size=2 --human 1 --seed 1,2 --smooth --keys "ENTER,WAIT:50,MOVE:1023x300,WAIT:400,VIEWCHECK,MOVE:500x300,WAIT:600,VIEWCHECK"
check smooth_edge "the mouse at the edge pans" "viewcheck: panning 1 offset"
check smooth_edge "and stops when it leaves" "viewcheck: panning 0 offset 0,0"
# A change of zoom keeps the map point at the screen's centre: the tile followed comes back to within
# a pixel or two after zooming in and out, and is found where it is drawn. The original's moves
# it by up to a tile at each step. (Away from the map's edge: there the view stays inside the map.)
run smooth_zoom --new size=2 --human 1 --seed 1,2 --smooth --keys "ENTER,WAIT:50,S-UP,WAIT:400,S-UP,WAIT:400,S-UP,WAIT:400,VIEWPOINT,z,WAIT:250,VIEWPOINT,z,WAIT:250,VIEWPOINT,x,WAIT:250,x,WAIT:250,VIEWPOINT,VIEWCHECK"
check smooth_zoom "the zoom is animated" "view zoom: [0-9]+ frames"
check smooth_zoom "tiles found after a zoom" "viewcheck: panning 1 offset -?[0-9]+,-?[0-9]+: [0-9]+ tiles, 0 wrong"
if grep viewpoint "$OUT/smooth_zoom.log" | awk '{split($10, p, ","); x[NR]=p[1]; y[NR]=p[2]} END {d=x[1]-x[NR]; e=y[1]-y[NR]; exit !(NR==4 && d*d<=9 && e*e<=9)}'; then
    echo "PASS smooth_zoom: the point stays through a zoom in and out"
else echo "FAIL smooth_zoom: the point moved"; FAIL=1; fi
run smooth_glide --new size=2 --human 1 --seed 1,2 --smooth --keys "ENTER,WAIT:50,S-RIGHT,WAIT:500,UNITS"
check smooth_glide "a move of the view glides" "view glide: [0-9]+ frames"
# PRACX's zoom levels (--zoom-levels 6): x steps out to the level before 0, four more reach the
# outermost (the whole map) and stay there, ctrl+Z goes to the innermost, z there changes nothing.
run zoom_levels --new size=2 --human 1 --seed 1,2 --smooth --zoom-levels 6 --keys "ENTER,WAIT:50,VIEWPOINT,x,WAIT:250,VIEWPOINT,x,WAIT:250,x,WAIT:250,x,WAIT:250,x,WAIT:250,VIEWPOINT,C-z,WAIT:250,VIEWPOINT,z,WAIT:250,VIEWPOINT"
check zoom_levels "the levels are listed, with the original's zoom 0" "zoom levels: -[0-9]+( -?[0-9]+)* 0( [0-9]+)+$"
if awk '/^zoom levels:/ {n=NF-2; for (i=3; i<=NF; i++) {l[i-2]=$i; if ($i==0) z=i-2}} /^viewpoint:/ {v[++m]=$3}
        END {exit !(m==5 && v[1]==0 && v[2]==l[z-1] && v[3]==l[1] && v[4]==l[n] && v[5]==l[n])}' "$OUT/zoom_levels.log"; then
    echo "PASS zoom_levels: the zoom keys step through the levels"
else echo "FAIL zoom_levels: the zoom keys step through the levels"; FAIL=1; fi
nocrash zoom_levels
# PRACX's details when zoomed out (--zoom-details): at zoom -12 the screen differs from the
# original's (improvements, units and bases drawn), at -14 it is the same.
ZD="--new size=2 --human 1 --seed 1,2 --keys ENTER,WAIT:50,x,x,x,x,x,x,WAIT:300,SHOT:z12.bmp,x,WAIT:300,SHOT:z14.bmp"
run zoom_details_off $ZD
run zoom_details_on $ZD --zoom-details
if ! cmp -s "$OUT/zoom_details_off/z12.bmp" "$OUT/zoom_details_on/z12.bmp" && cmp -s "$OUT/zoom_details_off/z14.bmp" "$OUT/zoom_details_on/z14.bmp"; then
    echo "PASS zoom_details: details drawn at -12, none at -14"
else echo "FAIL zoom_details: details drawn at -12, none at -14"; FAIL=1; fi
nocrash zoom_details_on
# Thinker's formers (src/thinker, --thinker manage_player_units=1): the player's automated former
# runs Thinker's former_move and picks its own terraforming; off, the original's. Needs a save
# with a player former, made once with (about four minutes):
#   build/native/opensmac --original-bugs --hidden --outdir work/thinker_test/gen --new size=1 \
#       --seed 1,2 --human 0 --turns 40 --snapshots work/thinker_test/snap
#   cp work/thinker_test/snap/turn_040.sav work/thinker_test/t40.sav
# Every unit of faction 1 is automated (AUTO:ID) so that ENTER ends the turn and the automated
# units are moved: the former is unit 11 at 40,44.
if [ -f "$ROOT/work/thinker_test/t40.sav" ]; then
    AUTOS="AUTO:11,AUTO:12,AUTO:20,AUTO:23,AUTO:31,AUTO:32,AUTO:46,AUTO:54"
    ETURN="ENTER,WAIT:400,ENTER,WAIT:200,ENTER,WAIT:200"
    run thinker_formers_off --save work/thinker_test/t40.sav --human 1 --keys "WAIT:50,$AUTOS,$ETURN,$ETURN,$ETURN,UNITS"
    run thinker_formers_on --save work/thinker_test/t40.sav --human 1 --thinker manage_player_units=1 --thinker debug=1 --keys "WAIT:50,$AUTOS,$ETURN,$ETURN,$ETURN,UNITS"
    check thinker_formers_on "Thinker's former logic ran for the player's former" "^thinker: former_(action|move|skip) 40 44"
    check thinker_formers_on "the former is terraforming" "^unit 11 f1 Formers \(40,44\) order [1-9]"
    if ! grep -q '^thinker:' "$OUT/thinker_formers_off.log" && grep -qE '^unit 11 f1 Formers \(40,44\) order [1-9]' "$OUT/thinker_formers_off.log"; then
        echo "PASS thinker_formers: off, the original's logic terraforms without Thinker"
    else echo "FAIL thinker_formers: off, the original's logic terraforms without Thinker"; FAIL=1; fi
    nocrash thinker_formers_on
    # Thinker's governor (--thinker manage_player_bases=1): the governor switched on at base 0
    # (GOV:ID) takes Thinker's choice of production (select_build) instead of the original's; off,
    # the original's base_reset runs and the base keeps building its Recreation Commons.
    run thinker_governor_off --save work/thinker_test/t40.sav --human 1 --keys "WAIT:50,$AUTOS,GOV:0,BASES,$ETURN,$ETURN,BASES"
    run thinker_governor_on --save work/thinker_test/t40.sav --human 1 --thinker manage_player_bases=1 --thinker base_hurry=2 --thinker debug=1 --keys "WAIT:50,$AUTOS,GOV:0,BASES,$ETURN,$ETURN,BASES"
    check thinker_governor_on "Thinker's governor chose the base's production" "^thinker: choice: -?[0-9]+ "
    check thinker_governor_on "the governor is on and the choice applied" "^base 0 f1 .* gov 80DFBE41 production -?[0-9]+ "
    if ! grep -q '^thinker:' "$OUT/thinker_governor_off.log" && grep -qE '^base 0 f1 .* gov 80DFBE41 production -6 Recreation Commons' "$OUT/thinker_governor_off.log"; then
        echo "PASS thinker_governor: off, the original's governor keeps the base's production"
    else echo "FAIL thinker_governor: off, the original's governor keeps the base's production"; FAIL=1; fi
    nocrash thinker_governor_on
    # Thinker's popup options: within eight turns a captured mind worm of faction 1 is promoted;
    # the original announces it with a modal popup (MORALE2), game_event_popup=0 with a note in the
    # message box (the harness logs it as "netmsg MORALE2: ...").
    EIGHT="$ETURN,$ETURN,$ETURN,$ETURN,$ETURN,$ETURN,$ETURN,$ETURN"
    run thinker_popups_off --save work/thinker_test/t40.sav --human 1 --keys "WAIT:50,$AUTOS,$EIGHT,UNITS"
    run thinker_popups_on --save work/thinker_test/t40.sav --human 1 --thinker game_event_popup=0 --thinker warn_on_former_replace=0 --thinker foreign_treaty_popup=1 --keys "WAIT:50,$AUTOS,$EIGHT,UNITS"
    check thinker_popups_off "the original announces the promotion with a popup" "^popup MORALE2 shown"
    check thinker_popups_on "game_event_popup=0: the promotion is a note in the message box" "^netmsg MORALE2: "
    if ! grep -q '^popup MORALE' "$OUT/thinker_popups_on.log"; then echo "PASS thinker_popups: and no popup"
    else echo "FAIL thinker_popups: a MORALE popup with game_event_popup=0"; FAIL=1; fi
    nocrash thinker_popups_on
fi
check keys "explore: state 0x4000" "unit 5 f1 Unity Foil \([0-9]+,[0-9]+\) order 0 moves [0-9]+ state 0401?4"
nocrash keys

# 2. Go-to list (g: the base list; Enter: the preselected base, the unit's home) on a late save
#    (Morgan's... faction 1 of turn_100: colony pod 91 selected first).
if [ -f "$ROOT/work/saves_late/turn_100.sav" ]; then
    run goto --save work/saves_late/turn_100.sav --human 1 --keys "g,PLANSTATE,SHOT:goto_list.bmp,ENTER,UNITS,PLANSTATE"
    check goto "go-to list shown" "popup GOTO shown"
    # PlanWin (planwin.cpp, Console_goto_routine): the world map pane shows the route from the pod
    # to the selected base, zoomed in to -10 around both ends; hidden when the list closes.
    check goto "PlanWin's route" "planwin shown 1 zoom -10 centre 6,42 lines 1 \\(6,42\\)-\\(3,43\\) c241"
    check goto "PlanWin closed" "planwin shown 0 "
    check goto "PlanWin set up" "planwin open 239x119"
    # Find a base (ctrl+B, turn_147, faction 3): the selected base on PlanWin; Down moves the
    # selection and the list's callback redraws it at the next base.
    run findbase --save work/saves_late/turn_147.sav --human 3 --keys "DOWN,ENTER,C-b,PLANSTATE,DOWN,PLANSTATE,SHOT:find.bmp,ESC"
    check findbase "the first base" "planwin shown 1 zoom -14 centre 24,24 lines 1 \\(37,21\\)-\\(37,21\\) c255"
    check findbase "the next one" "planwin shown 1 zoom -14 centre 24,24 lines 1 \\(34,18\\)-\\(34,18\\) c255"
    nocrash findbase
    check goto "a base chosen" "popup GOTO answer: [0-9]+"
    check goto "go-to order 24 given" "unit 91 f1 Colony Pod \([0-9]+,[0-9]+\) order 24"
    nocrash goto
fi

# 3. Mouse: drag the scout (go-to drag, held button) to a tile; click on the map (centre, select).
run mouse $NEW --keys "ENTER,DRAG:530x200:640x260,UNITS,v,CLICK:300x300,UNITS,SHOT:mouse.bmp"
check mouse "drag: go-to order 24" "unit 4 f1 Scout Patrol \(35,9\) order 24"
nocrash mouse

# 3b. pfncProcZoomKey's zoom at the mouse (index -1; unreachable in play, the test aid ZOOMAT:XxY):
#     at zoom 0 every window zooms out around its centre; at a reduced zoom the main view is
#     centred on the seen tile under the mouse (the window's size read from its GraphicWin).
run zoomat $NEW --keys "ENTER,ZOOMAT:530x200,ZOOMAT:600x150"
check zoomat "zoom 0: zoomed out around the centre" "^zoomat 530,200 tile 35,7 centre 35,7 -> zoom -2 centre 35,7$"
check zoomat "zoom -2: centred on the tile under the mouse" "^zoomat 600,150 tile 35,5 centre 35,7 -> zoom -4 centre 35,5$"
nocrash zoomat

# 3c. The window's close button (CLOSE): the quit question of ctrl+Q (REALLYQUIT); a second close
#     while it is up is ignored (a modal window), "Oops, no" plays on (the hold order is given
#     afterwards); then close and "Yes" quit (the last UNITS never runs). OpenSMAC's own: the
#     original's window has no close button (win.cpp close_requested).
run close $NEW --keys "ENTER,CLOSE,CLOSE,ENTER,h,UNITS,CLOSE,DOWN,ENTER,UNITS"
check close "close: the quit question" "^window close: the quit question$"
check close "a second close while it is up: ignored" "^window close: ignored \(a modal window is up\)$"
check close "No: the answer" "popup REALLYQUIT answer: 0 "
check close "No: playing on (hold given afterwards)" "^unit 4 f1 Scout Patrol \(35,7\) order 2 "
check close "Yes: quit" "popup REALLYQUIT answer: 1 "
if [ "$(grep -c '^unit 4 ' "$OUT/close.log")" = 1 ] && [ "$(grep -c '^window close: the quit question' "$OUT/close.log")" = 2 ]; then
    echo "PASS close: quit after Yes (no more input taken)"
else echo "FAIL close: quit after Yes"; FAIL=1; fi
nocrash close
#     Before a game runs (the main menu, the setup screens) the close quits at once (the
#     MENUSTATE after it never runs); in observer mode too.
run closemenu --keys "CLOSE,WAIT:200,MENUSTATE"
check closemenu "main menu: quits" "^window close: no game running: quitting$"
if grep -q "^menustate" "$OUT/closemenu.log"; then echo "FAIL closemenu: input after the close"; FAIL=1
else echo "PASS closemenu: no input after the close"; fi
nocrash closemenu
run closesetup --keys "ENTER,WAIT:200,MENUSTATE,CLOSE,WAIT:200,MENUSTATE"
check closesetup "setup screen shown" "^menustate caption=\"\" selected=0 \[MAKE RANDOM MAP\]"
check closesetup "setup screen: quits" "^window close: no game running: quitting$"
if [ "$(grep -c '^menustate' "$OUT/closesetup.log")" = 1 ]; then echo "PASS closesetup: no input after the close"
else echo "FAIL closesetup: input after the close"; FAIL=1; fi
nocrash closesetup
run closeobs --new size=0 --human 0 --seed 1,2 --keys "CLOSE"
check closeobs "observer: quits" "^quit requested: quitting$"
nocrash closeobs

# 4. Unit orders through the menus (Alt+A: Action; the items run Console_menu_proc).
run menu $NEW --keys "ENTER,A-a,SHOT:menu_action.bmp,ESC,ALT,RIGHT,SHOT:menu_game.bmp,ESC,A-a,DOWN,DOWN,DOWN,DOWN,DOWN,DOWN,DOWN,DOWN,DOWN,DOWN,ENTER,UNITS"
check menu "a menu command chosen" "menu item [0-9A-F]+ "
check menu "Console_menu_proc ran it" "^menu command 3[0-9A-F]{2}$"
nocrash menu

# 5. Save (ctrl+S, the name typed) and load (ctrl+L, the newest save): the hold order of the
#    scout given after the save is gone after the load.
rm -f "$OUT/saveload/saves/cstest.SAV"
run saveload $NEW --keys "ENTER,C-s,c,s,t,e,s,t,ENTER,h,UNITS,C-l,ENTER,UNITS,SHOT:loaded.bmp"
if [ -f "$OUT/saveload/saves/cstest.SAV" ]; then echo "PASS saveload: saves/cstest.SAV written (below the outdir)"; else echo "FAIL saveload: no save"; FAIL=1; fi
check saveload "loaded: the scout has no orders again" "popup LOADGAME shown(.|\n)*" 
if [ "$(grep -c 'unit 4 f1 Scout Patrol (35,7) order 0 ' "$OUT/saveload.log")" -ge 1 ] && \
   grep -q 'unit 4 f1 Scout Patrol (35,7) order 2 ' "$OUT/saveload.log"; then echo "PASS saveload: order 2 before, 0 after"
else echo "FAIL saveload: orders"; FAIL=1; fi
rm -f "$OUT/saveload/saves/cstest.SAV"
nocrash saveload

# 6. A late save as a human faction: go-to list, automation (shift+A), turns.
if [ -f "$ROOT/work/saves_late/turn_100.sav" ]; then
    TURN="C-ENTER,DOWN,ENTER,ENTER,ENTER,ENTER"
    run late100 --save work/saves_late/turn_100.sav --human 1 --keys "UNITS,g,ESC,A,UNITS,SHOT:late.bmp,$TURN,$TURN,SHOT:late2.bmp"
    nocrash late100
fi

exit $FAIL
