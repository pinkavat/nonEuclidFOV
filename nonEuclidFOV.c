#include <stdio.h>
#include <stdlib.h>

#include <ncurses.h>    // compile with -lncurses

/* nonEuclidFOV.c
 *
 * An extension of our O(n) FOV algorithm (described in sightLineTest.c) that allows for arbitrary rotations and connections between tiles ("portals")
 *
 * written August 2020 by Thomas Pinkava
*/

// TODO: view distance
// TODO: factor into functions, improve portal storage strategy to save memory perhaps?
// TODO: fix corner kludge for on-axis tiles

#define SCREEN_WIDTH 31
#define SCREEN_HEIGHT 31

#define WORLD_HEIGHT 7
#define WORLD_WIDTH 7

#define OUT_OF_BOUNDS(x, y) ((x) < 0 || (x) >= WORLD_WIDTH || (y) < 0 || (y) >= WORLD_HEIGHT)

enum cardinal_directions {NORTH, EAST, SOUTH, WEST};

struct portal {
    int x, y;               // The exit coordinate of the portal
    unsigned int rotation;  // The screen-space upward direction of the world rotates clockwise this many 90 deg turns upon passing through this portal
};


//======= TEMPORARY HELPERS

#define CAMERA_X -15
#define CAMERA_Y -15

void ADD_PORTAL(struct portal portals[WORLD_HEIGHT][WORLD_WIDTH],int x, int y, int dx, int dy, unsigned int rotation){
    portals[y][x].x=dx;
    portals[y][x].y=dy;
    portals[y][x].rotation=rotation;
}

//======= END TEMPORARY HELPERS


int main(void){

    int avatarX = 2;    // The player's coordinates in worldspace
    int avatarY = 1;

    enum cardinal_directions avatarUpDir = NORTH;   // The player's rotation

    int screenX = -2;    // The coordinates of the screen's top-left corner in worldspace
    int screenY = -2;

    // The set of all portals in the world
    struct portal portals[WORLD_HEIGHT][WORLD_WIDTH];
    for(int i = 0; i < WORLD_HEIGHT; i++){      // Initialize every portal tile to "no portal" TODO think of better way for this perhaps
        for(int j = 0; j < WORLD_WIDTH; j++){
            portals[i][j].x = -1;
            // portals[i][j].y = -1;
            // No need to initialize y or rotation; the check is at x value
        }
    }

    /*
    // A map of the tiles that block sight in the world
    unsigned char blockages[WORLD_HEIGHT][WORLD_WIDTH] =   {    // 19x7
                                    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
                                    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
                                    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
                                    {1,0,0,1,1,1,1,1,0,0,0,1,1,1,1,1,0,0,1},
                                    {1,0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,1},
                                    {1,0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,1},
                                    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}
                                    };
    


    // Manually set some test portals
    ADD_PORTAL(portals,4,4,15,4, 0);    // Lower corridor shortener
    ADD_PORTAL(portals,4,5,15,5, 0);
    ADD_PORTAL(portals,14,4,3,4, 0);
    ADD_PORTAL(portals,14,5,3,5, 0);

    ADD_PORTAL(portals,6,4,11,4, 0);    // Hall of mirrors
    ADD_PORTAL(portals,6,5,11,5, 0);
    ADD_PORTAL(portals,12,4,7,4, 0);
    ADD_PORTAL(portals,12,5,7,5, 0);
    */


    unsigned char blockages[WORLD_HEIGHT][WORLD_WIDTH] = {  // 7x7
                                            {1, 1, 1, 1, 1, 1, 1},
                                            {1, 1, 0, 0, 0, 0, 1},
                                            {1, 0, 0, 0, 0, 0, 1},
                                            {1, 0, 0, 0, 0, 0, 1},
                                            {1, 0, 0, 0, 0, 0, 1},
                                            {1, 1, 0, 0, 0, 1, 1},
                                            {1, 1, 1, 1, 1, 1, 1}
                                            };

    const char* const decor[WORLD_HEIGHT][WORLD_WIDTH] = {
                                            {"  ", "  ", "  ", "  ", "  ", "  ", "  "},
                                            {"  ", "  ", "  ", "  ", "  ", "  ", "  "},
                                            {"  ", "  ", "  ", "[]", "  ", "  ", "  "},
                                            {"  ", "  ", "  ", "::", "  ", "  ", "  "},
                                            {"  ", "  ", "  ", "::", "  ", "  ", "  "},
                                            {"  ", "  ", "  ", "  ", "  ", "  ", "  "},
                                            {"  ", "  ", "  ", "  ", "  ", "  ", "  "}
                                            };

    ADD_PORTAL(portals, 0, 2, 4, 5, 1);
    ADD_PORTAL(portals, 0, 3, 3, 5, 1);
    ADD_PORTAL(portals, 0, 4, 2, 5, 1);
    
    ADD_PORTAL(portals, 4, 6, 1, 2, 3);
    ADD_PORTAL(portals, 3, 6, 1, 3, 3);
    ADD_PORTAL(portals, 2, 6, 1, 4, 3);




    // A buffer to store the computed shadows in
    unsigned int shadows[SCREEN_HEIGHT][SCREEN_WIDTH];

    // A buffer to store the world-coordinate that each screen tile displays
    int tileWorldXs[SCREEN_HEIGHT][SCREEN_WIDTH];
    int tileWorldYs[SCREEN_HEIGHT][SCREEN_WIDTH];

    // A buffer to store the orientation of each tile
    enum cardinal_directions upDirs[SCREEN_HEIGHT][SCREEN_WIDTH];

    // Initialize ncurses
    // More here: http://tldp.org/HOWTO/NCURSES-Programming-HOWTO/init.html
    initscr();           // Initialize the window
    noecho();            // Echo no keypresses
    curs_set(FALSE);     // Display no cursor
    keypad(stdscr,true); // Allow for arrow and function keys

    // Check for terminal colour
    if(has_colors() == FALSE || can_change_color() == FALSE){   
        endwin();
        fprintf(stderr,"Your terminal does not support the necessary colour functionality\n");
        exit(1);
    }
    start_color();
    // Initialize colours
    init_pair(1, COLOR_BLACK, COLOR_RED);
    init_pair(2, COLOR_BLUE, COLOR_WHITE);
    init_pair(3, COLOR_GREEN, COLOR_BLACK);
    init_pair(4, COLOR_BLACK, COLOR_YELLOW);
    init_pair(5, COLOR_BLACK, COLOR_WHITE);
    init_pair(6, COLOR_BLACK, COLOR_BLUE);
    init_pair(7, COLOR_BLUE, COLOR_BLACK);


    // "game-style" demo loop
    unsigned char cameraMode = 1;
    while(1){
        unsigned char bobFlag = 0;
        // Get user input and move viewpoint accordingly
        int targetX=avatarX, targetY=avatarY;
        enum cardinal_directions targetDir;
        unsigned char zorp = 0;
        switch(getch()){
            case KEY_UP: case 'w': targetDir = NORTH; zorp = 1; break;
            case KEY_LEFT: case 'a': targetDir = WEST; zorp = 1; break;
            case KEY_DOWN: case 's': targetDir = SOUTH; zorp = 1; break;
            case KEY_RIGHT: case 'd': targetDir = EAST; zorp = 1; break;
            case 'i': screenY--; break;
            case 'j': screenX--; break;
            case 'k': screenY++; break;
            case 'l': screenX++; break;
            case 't': cameraMode = 1; break;
            case 'y': cameraMode = 0; break;
            default:
                bobFlag = 1;
            break;
        }
        targetDir = (avatarUpDir + targetDir) % 4;
        if(zorp){
            switch(targetDir){
                case NORTH: targetY--; break;
                case EAST: targetX++; break;
                case SOUTH: targetY++; break;
                case WEST: targetX--; break;
            }
        }
        if(!OUT_OF_BOUNDS(targetX, targetY)){
            if(portals[targetY][targetX].x != -1){
                // Teleport player rule
                avatarX = portals[targetY][targetX].x;
                avatarY = portals[targetY][targetX].y;
                avatarUpDir = (avatarUpDir + portals[targetY][targetX].rotation) % 4;
            } else if(!blockages[targetY][targetX]) {
                avatarX = targetX;
                avatarY = targetY;
            }
        }
        if(cameraMode == 1){
            screenX = avatarX + CAMERA_X;
            screenY = avatarY + CAMERA_Y;
        } else {
            if(avatarX<screenX)screenX--;
            if(avatarX>screenX+SCREEN_WIDTH-1)screenX++;
            if(avatarY<screenY)screenY--;
            if(avatarY>screenY+SCREEN_HEIGHT-1)screenY++;
        }
        if(bobFlag)continue;



        // Compute shadows

        int avatarScreenX = avatarX - screenX;  // The avatar's position in screenspace
        int avatarScreenY = avatarY - screenY;

        // Iterate over the rows, away from the axis (dy is y iteration direction)
        for(int dy = -1; dy < 2; dy += 2){

            // Reset row-wide shadow buffers TODO sight-limit (?)
            int prevO[SCREEN_WIDTH];                    // The previous row's O-values (100 - Trailing Y)
            int prevP[SCREEN_WIDTH];                    // The previous row's P-values (Leading Y)
            for(int j=0;j<SCREEN_WIDTH;j++){
                prevO[j]=-1;
                prevP[j]=-1;
            }

            int yBound = (dy==-1) ? avatarScreenY + 1 : SCREEN_HEIGHT - avatarScreenY;   // TODO sight-limit

            for(int y = 0; y < yBound; y++){    // Avatar-space coordinates  
  
                // Iterate over the columns, away from the axis (dx is x iteration direction)
                for(int dx = -1; dx < 2; dx += 2){

                    int worldX = avatarX;   // Reset the true world coordinates to the avatar coordinates
                    int worldY = avatarY;

                    enum cardinal_directions upDir = avatarUpDir; // Reset the screenspace upward direction to match the avatar's rotation

                    int prevM = -1;     // The previous cell's M-value (Trailing X)
                    int prevN = -1;     // The previous cell's N-value (100 - Leading X)

                    int xBound = (dx==-1) ? avatarScreenX + 1 : SCREEN_WIDTH - avatarScreenX;   
                    // TODO sight-limit; TODO circular view distance computed in slices at this step

                    for(int x = 0; x < xBound; x++){    // Avatar-space coordinate
                        // For every tile on the screen, iterating away from the origin in avatar-space

                        // Compute screenspace equivalent of avatar-space iteration coordinate
                        int sy = avatarScreenY+(dy*y);
                        int sx = avatarScreenX+(dx*x);
                        
                        // Inherit our portal offset, rotation, and world coordinates from the unshadowed neighbor
                        if(y > 0 && prevO[sx] != 0 && prevP[sx] != 100){
                            // Inherit world coordinate from previous row
                            upDir = upDirs[sy-dy][sx];  // Inherit upDir from previous row

                            switch(upDir){
                                case NORTH: worldX = tileWorldXs[sy-dy][sx]; worldY = tileWorldYs[sy-dy][sx] + dy; break;
                                case EAST: worldY = tileWorldYs[sy-dy][sx]; worldX = tileWorldXs[sy-dy][sx] - dy; break;
                                case SOUTH: worldX = tileWorldXs[sy-dy][sx]; worldY = tileWorldYs[sy-dy][sx] - dy; break;
                                case WEST: worldY = tileWorldYs[sy-dy][sx]; worldX = tileWorldXs[sy-dy][sx] + dy; break;
                            }

                        } else if(x > 0){
                            // Inherit world coordinate from previous column cell
                            switch(upDir){
                                case NORTH: worldX += dx; break;
                                case EAST: worldY += dx; break;
                                case SOUTH: worldX -= dx; break;
                                case WEST: worldY -= dx; break;
                            }
                        }
                        

                        // Begin shadow computation
                        unsigned int shadow = 0;
            
                        // Shadow Step 0: Resolve initial shadow conflicts
                        if(prevP[sx] != -1) prevN = -1;
                        if(prevM != -1) prevO[sx] = -1;

                        // Shadow Step 1: Use similar-triangles rule to compute the exit point for each entering shadow line:
                        // N.B. This simplified implementation assumes sightlines start from the origin point; not the center of the origin tile.
                        int a = (prevM == -1) ? -1: (prevM + 100*y) / x + prevM;
                        int b = (prevN == -1) ? -1: (prevN + 100*y) / x + prevN;
                        int c = (prevO[sx] == -1) ? -1: (prevO[sx] + 100*x) / y + prevO[sx];
                        int d = (prevP[sx] == -1) ? -1: (prevP[sx] + 100*x) / y + prevP[sx];


                        // Shadow Step 2a: If any of the lines overflow, they alternate with their partner line (minmaxing to maximally expand the shadow)
                        if(a > 100){
                            c = 100 - (100*a - 10000) / (a - prevM);
                            a = 100;
                            // Add M-C line to shadow
                            shadow += 100 - (c * (100 - prevM))/200;
                        } else if(prevM != -1){
                            // Add M-A line to shadow
                            shadow += (a + prevM)/2;
                        }
                        if(c > 100){
                            a = 100 - (100*c - 10000) / (c - prevO[sx]);
                            c = -1;
                            // Add O-A line to shadow
                            shadow += (a * (100 - prevO[sx]))/200;
                        } else if (prevO[sx] != -1){
                            // Add O-C line to shadow
                            shadow += 100 - (c + prevO[sx])/2;
                        }

                        if(b > 100){
                            int dfromb = 100 - (100*b - 10000) / (b - prevN);
                            d = (d > dfromb) ? d : dfromb;
                            b = -1;
                            // Add N-D line to shadow
                            shadow += (d * (100 - prevN))/200;
                        } else if (prevN != -1){
                            // Add N-B line to shadow
                            shadow += 100 - (b + prevN)/2;
                        }
                        if (d > 100){
                            int bfromd = 100 - (100*d - 10000) / (d - prevP[sx]);
                            b = (b < bfromd && b >= 0) ? b : bfromd;
                            d = 100;
                            // Add P-B line to shadow
                            shadow += 100 - (b * (100-prevP[sx]))/200;
                        } else if (prevP[sx] != -1){
                            // Add P-D line to shadow 
                            shadow += (d + prevP[sx])/2;
                        }

                        // Shadow Step 2b: if the edges pull in, the perpendicular side must all be in shadow
                        int tempC = c;
                        if(b != -1){
                            c = 0;
                        }
                        if (tempC != -1){
                            b = 0;
                        }

                        // Shadow Step 2c: if the trailing and leading edges of the shadow overlap, collapse the shadow.
                        if(a != -1 && b != -1 && a > b){ b = 0; a = -1;}
                        if(d != -1 && c != -1 && d > c){ c = 0; d = -1;}

                        // Shadow Step 3: Copy out values for the next cell
                        prevM = a;
                        prevN = b;
                        prevO[sx] = c;
                        prevP[sx] = d;


                        if(OUT_OF_BOUNDS(worldX, worldY)){
                            // Tile is out of world bounds, report it as fully in shadow
                            shadow = 100;
                            tileWorldXs[sy][sx] = 0;    // For safety, mark the tile as pointing to an in-bounds tile
                            tileWorldYs[sy][sx] = 0;    // Tile is fully enshadowed, thus, it will never render and its contents are immaterial
                        } else {
                            // Check for portals
                            if(portals[worldY][worldX].x != -1){
                                // If this cell is a portal, rotate and teleport iterator to exit up-direction and coordinate
                                upDir = (upDir + portals[worldY][worldX].rotation) % 4;
                                int tempWorldX = portals[worldY][worldX].x;
                                worldY = portals[worldY][worldX].y;
                                worldX = tempWorldX;
                            }
                            // Check for blockages
                            if(blockages[worldY][worldX]){
                                // If the cell is a wall, report that this cell has a 100% Leading X and a 100% Trailing Y
                                if(!(x==0&&dx==-1))prevO[sx] = 0;   // Kludge to account for the fact that each axis is passed over twice; only the second x-axis-pass counts
                                prevN = 0;

                                // Kludge to show otherwise shadowed corner walls (TODO FIX)
                                // if(y>0 && x>0 && shadows[sy-dy][sx]<100 && shadows[sy][sx-dx]<100) shadow = 50;  
                            }
                            // Store the current world position in the buffer, so the screen knows what world tile corresponds to this screen tile
                            tileWorldXs[sy][sx] = worldX;
                            tileWorldYs[sy][sx] = worldY;
                        }

                        // Store this cell's upward direction, for use in the next row's computation (and later, rendering)
                        upDirs[sy][sx] = upDir;

                        // Shadow Step 4: clamp shadow to 100% maximum and store it in the shadow array
                        if (shadow > 100) shadow = 100;
                        shadows[sy][sx] = shadow;

                    } // End loop over x
                }   // End loop over column iteration direction

            }   // End loop over Y
        } // End loop over row iteration direction




        // print screen
        //printf("\e[H");
        for(int y = 0; y < SCREEN_HEIGHT; y++){
            for(int x = 0; x < SCREEN_WIDTH; x++){
                move(y,x*2);
                int wx = tileWorldXs[y][x]; // Fetch the world tile at this screen position
                int wy = tileWorldYs[y][x];
                if(OUT_OF_BOUNDS(wx, wy)){  // TODO this should be redundant now
                    //printf("\e[38;5;52m▒▒\e[0m");
                } else if (shadows[y][x] == 100){
                    //printf("  ");
                    attron(COLOR_PAIR(3));
                    printw("  ");
                    attroff(COLOR_PAIR(3));
                } else if (wx == avatarX && wy == avatarY){
                    //printf("><");
                    attron(COLOR_PAIR(1)|A_BOLD|A_BLINK);
                    addch(ACS_DEGREE);
                    addch(ACS_DEGREE);
                    attroff(COLOR_PAIR(1)|A_BOLD|A_BLINK);
                } else if(blockages[wy][wx]){
                    //printf("██");
                    attron(COLOR_PAIR(4));
                    printw("  ");
                    attroff(COLOR_PAIR(4));
                } else {
                    //if(upDirs[y][x] == SOUTH) attron(A_REVERSE);
                    //printf("\e[38;5;%dm%s\e[0m", ((255-232) * (100-shadows[y][x]))/100+232, "░░");
                    int zob = (shadows[y][x]>80) ? 7 : (shadows[y][x]>60) ? 6 : (shadows[y][x]>40)? 5 : 2;
                    attron(COLOR_PAIR(zob));
                    if(shadows[y][x] > 20){ 
                        addch(ACS_CKBOARD);
                        addch(ACS_CKBOARD);
                    } else {
                        printw(decor[wy][wx]);
                    }
                    attroff(COLOR_PAIR(zob));
                    //if(upDirs[y][x] == SOUTH) attroff(A_REVERSE);
                }
            }
            //printf("\n");
        }
        refresh();
    }

    // Cleanup
    endwin();
}
