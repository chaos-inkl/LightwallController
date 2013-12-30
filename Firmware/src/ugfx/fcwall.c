/** @file fcwall.c
 * @brief Dynamic fullcircle simulation of the wall
 * @author Ollo
 *
 * @date 30.12.2013
 * @defgroup LightwallController
 *
 */

#include "fcwall.h"
#include "gfx.h"

/******************************************************************************
 * GLOBAL VARIABLES of this module
 ******************************************************************************/


/******************************************************************************
 * LOCAL VARIABLES for this module
 ******************************************************************************/
static int boxWidth;
static int boxHeight;

/******************************************************************************
 * LOCAL FUNCTIONS
 ******************************************************************************/

/******************************************************************************
 * EXTERN FUNCTIONS
 ******************************************************************************/

void setBox(int x, int y, uint8_t red, uint8_t green, uint8_t blue) {

		int hexCol = red << 16 | green << 8 | blue;

		color_t col = HTML2COLOR(hexCol);

        gdispFillArea(x*(boxWidth+1), y*(boxHeight+1), boxWidth, boxHeight, col);
        gdispDrawBox (x*(boxWidth+1), y*(boxHeight+1), boxWidth, boxHeight, Yellow);
}

void fcwall_init(int w, int h)
{
	coord_t width, height;
	int x = 0;
	int y = 0;

	width = gdispGetWidth();
	height = gdispGetHeight();

	boxWidth = ((int) (width / w))-1;
	boxHeight = ((int) ((height-25) / h))-1;

	for(x=0; x < w; x++)
	{
		for(y=0; y < h; y++)
		{
			setBox(x,y, 0,0,0);
		}
	}
}


