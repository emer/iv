/*
Copyright (C) 1993 Tim Prinzing
Copyright (C) 2002 Tim Prinzing, Michael Hines
This file contains programs and data originally developed by Tim Prinzing
with minor changes and improvements by Michael Hines.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with this library; if not, write to the Free
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

// =======================================================================
//
//                     <IV-Win/color.cpp>
//
//  MS-Windows implementation of the InterViews Color classes.
//
//
// ========================================================================

#include <IV-Win/MWlib.h>
#include <OS/string.h>
#include <OS/math.h>
#include <OS/table.h>
#include <InterViews/color.h>
#include <InterViews/session.h>
#include <InterViews/bitmap.h>
#include <IV-Win/color.h>
#include <IV-Win/window.h>

#include <stdio.h>

// #######################################################################
// ################# class MWpalette
// #######################################################################

static const int GROW_SIZE = 16;

declareTable(MWcolorTable, COLORREF, COLORREF)
implementTable(MWcolorTable, COLORREF, COLORREF)
unsigned long key_to_hash(COLORREF r)
	{ return (unsigned long) r; }

// -----------------------------------------------------------------------
//  Constructors/destructor
// -----------------------------------------------------------------------
MWpalette::MWpalette()
{
	// ---- lookup table for unique colors ----
	table = new MWcolorTable(502);

	// ---- create a temporary logical palette ----
	int allocSize = sizeof(LOGPALETTE) + (GROW_SIZE * sizeof(PALETTEENTRY));
	LOGPALETTE* lpal = (LOGPALETTE*) new char[allocSize];

    // ---- initialize it ----
	lpal->palNumEntries = GROW_SIZE;
	lpal->palVersion = 0x300;
	for (int i = 0; i < GROW_SIZE; i++)
    {
		lpal->palPalEntry[i].peRed = 0;
		lpal->palPalEntry[i].peGreen = 0;
		lpal->palPalEntry[i].peBlue = 0;
		lpal->palPalEntry[i].peFlags = 0;
	}

    // ---- create it ----
	palette = CreatePalette(lpal);
#if !defined(__MWERKS__)
//bug in pro 6. can't delete before calling main?
	delete []lpal;
#endif
	numEntries = 0;
	palSize = GROW_SIZE;
}

MWpalette::MWpalette(LOGPALETTE* lpal)
{
	palette = CreatePalette(lpal);
	palSize = lpal->palNumEntries;
	numEntries = palSize;
}

MWpalette::~MWpalette()
{
	DeleteObject(palette);
}

// -----------------------------------------------------------------------
//  Add an entry to the palette.
// -----------------------------------------------------------------------
COLORREF MWpalette::addEntry(
	int r,
	int g,
	int b)
{
	COLORREF color = PALETTERGB( LOBYTE(r), LOBYTE(g), LOBYTE(b));
	COLORREF bogus;

	// ---- see if it's already there ----
	if (table->find(bogus, color))
		return color;

	// ---- extend the palette if necessary ----
	if (numEntries >= palSize)
	{
    	palSize += GROW_SIZE;
		ResizePalette(palette, palSize);
	}

	// ---- add the entry to the palette ----
	PALETTEENTRY newEntry;
	newEntry.peRed = LOBYTE(r);
	newEntry.peGreen = LOBYTE(g);
	newEntry.peBlue = LOBYTE(b);
	newEntry.peFlags = 0;
    SetPaletteEntries(palette, numEntries++, 1, &newEntry);

	// ---- add to the hash table ----
	table->insert(color, color);
	return color;
}

boolean MWpalette::findEntry(int r, int g, int b, COLORREF& value)
{
	COLORREF color = PALETTERGB( LOBYTE(r), LOBYTE(g), LOBYTE(b));
	if (table->find(value, color))
		return true;
	return false;
}

// -----------------------------------------------------------------------
//  Realize the palette into the given device context.  This merges the
//  palette entries into the system palette.  The number of entries that
//  changed in the system palette is returned.
// -----------------------------------------------------------------------
int MWpalette::realizeInto(
	HDC hdc,							// device context to realize into
	BOOL background)					// always background?
{
	SelectPalette(hdc, palette, background);
	return RealizePalette(hdc);
}

// #######################################################################
// ################# class ColorRep
// #######################################################################

// --- name of the colormap file for name lookup ----
static char* COLORMAP_FILE = NULL;

// initially, we have a shared palette for all windows of the application.
static MWpalette globalPalette;

MWpalette* ColorRep::defaultPalette()
{
	return &globalPalette;
}

// -----------------------------------------------------------------------
// constructors/destructors
// -----------------------------------------------------------------------
ColorRep::ColorRep(
	int r,					// red component of color
	int g,					// green component of color
	int b)					// blue component of color
{
	// ---- add to the palette ----
	color = globalPalette.addEntry(r, g, b);
}

ColorRep::~ColorRep()
{
}


// ------------------------------------------------------------------
// translates a color name to the X11 string format of an rgb
// specification (ie #??????).  The colormap name is basically a
// section in the colormap.ini file.  
// ------------------------------------------------------------------
// oreilly: just put the entire thing in here so installation is easy -- no
// dependency on app defaults or other install location files at all
static struct { char* name; char* value; } cc[] = {
  {"snow", "#fffafa"},  {"ghost white", "#f8f8ff"},  {"GhostWhite", "#f8f8ff"},
  {"white smoke", "#f5f5f5"},  {"WhiteSmoke", "#f5f5f5"},  {"gainsboro", "#dcdcdc"},
  {"floral white", "#fffaf0"},  {"FloralWhite", "#fffaf0"},  {"old lace", "#fdf5e6"},
  {"OldLace", "#fdf5e6"},  {"linen", "#faf0e6"},  {"antique white", "#faebd7"},
  {"AntiqueWhite", "#faebd7"},  {"papaya whip", "#ffefd5"},  {"PapayaWhip", "#ffefd5"},
  {"blanched almond", "#ffebcd"},  {"BlanchedAlmond", "#ffebcd"},  {"bisque", "#ffe4c4"},
  {"peach puff", "#ffdab9"},  {"PeachPuff", "#ffdab9"},  {"navajo white", "#ffdead"},
  {"NavajoWhite", "#ffdead"},  {"moccasin", "#ffe4b5"},  {"cornsilk", "#fff8dc"},
  {"ivory", "#fffff0"},  {"lemon chiffon", "#fffacd"},  {"LemonChiffon", "#fffacd"},
  {"seashell", "#fff5ee"},  {"honeydew", "#f0fff0"},  {"mint cream", "#f5fffa"},
  {"MintCream", "#f5fffa"},  {"azure", "#f0ffff"},  {"alice blue", "#f0f8ff"},
  {"AliceBlue", "#f0f8ff"},  {"lavender", "#e6e6fa"},  {"lavender blush", "#fff0f5"},
  {"LavenderBlush", "#fff0f5"},  {"misty rose", "#ffe4e1"},  {"MistyRose", "#ffe4e1"},
  {"white", "#ffffff"},  {"black", "#000000"},  {"dark slate gray", "#2f4f4f"},
  {"DarkSlateGray", "#2f4f4f"},  {"dark slate grey", "#2f4f4f"},  {"DarkSlateGrey", "#2f4f4f"},
  {"dim gray", "#696969"},  {"DimGray", "#696969"},  {"dim grey", "#696969"},
  {"DimGrey", "#696969"},  {"slate gray", "#708090"},  {"SlateGray", "#708090"},
  {"slate grey", "#708090"},  {"SlateGrey", "#708090"},  {"light slate gray", "#778899"},
  {"LightSlateGray", "#778899"},  {"light slate grey", "#778899"},  {"LightSlateGrey", "#778899"},
  {"gray", "#bebebe"},  {"grey", "#bebebe"},  {"light grey", "#d3d3d3"},
  {"LightGrey", "#d3d3d3"},  {"light gray", "#d3d3d3"},  {"LightGray", "#d3d3d3"},
  {"midnight blue", "#191970"},  {"MidnightBlue", "#191970"},  {"navy", "#000080"},
  {"navy blue", "#000080"},  {"NavyBlue", "#000080"},  {"cornflower blue", "#6495ed"},
  {"CornflowerBlue", "#6495ed"},  {"dark slate blue", "#483d8b"},  {"DarkSlateBlue", "#483d8b"},
  {"slate blue", "#6a5acd"},  {"SlateBlue", "#6a5acd"},  {"medium slate blue", "#7b68ee"},
  {"MediumSlateBlue", "#7b68ee"},  {"light slate blue", "#8470ff"},  {"LightSlateBlue", "#8470ff"},
  {"medium blue", "#0000cd"},  {"MediumBlue", "#0000cd"},  {"royal blue", "#4169e1"},
  {"RoyalBlue", "#4169e1"},  {"blue", "#0000ff"},  {"dodger blue", "#1e90ff"},
  {"DodgerBlue", "#1e90ff"},  {"deep sky blue", "#00bfff"},  {"DeepSkyBlue", "#00bfff"},
  {"sky blue", "#87ceeb"},  {"SkyBlue", "#87ceeb"},  {"light sky blue", "#87cefa"},
  {"LightSkyBlue", "#87cefa"},  {"steel blue", "#4682b4"},  {"SteelBlue", "#4682b4"},
  {"light steel blue", "#b0c4de"},  {"LightSteelBlue", "#b0c4de"},  {"light blue", "#add8e6"},
  {"LightBlue", "#add8e6"},  {"powder blue", "#b0e0e6"},  {"PowderBlue", "#b0e0e6"},
  {"pale turquoise", "#afeeee"},  {"PaleTurquoise", "#afeeee"},  {"dark turquoise", "#00ced1"},
  {"DarkTurquoise", "#00ced1"},  {"medium turquoise", "#48d1cc"},  {"MediumTurquoise", "#48d1cc"},
  {"turquoise", "#40e0d0"},  {"cyan", "#00ffff"},  {"light cyan", "#e0ffff"},
  {"LightCyan", "#e0ffff"},  {"cadet blue", "#5f9ea0"},  {"CadetBlue", "#5f9ea0"},
  {"medium aquamarine", "#66cdaa"},  {"MediumAquamarine", "#66cdaa"},  {"aquamarine", "#7fffd4"},
  {"dark green", "#006400"},  {"DarkGreen", "#006400"},  {"dark olive green", "#556b2f"},
  {"DarkOliveGreen", "#556b2f"},  {"dark sea green", "#8fbc8f"},  {"DarkSeaGreen", "#8fbc8f"},
  {"sea green", "#2e8b57"},  {"SeaGreen", "#2e8b57"},  {"medium sea green", "#3cb371"},
  {"MediumSeaGreen", "#3cb371"},  {"light sea green", "#20b2aa"},  {"LightSeaGreen", "#20b2aa"},
  {"pale green", "#98fb98"},  {"PaleGreen", "#98fb98"},  {"spring green", "#00ff7f"},
  {"SpringGreen", "#00ff7f"},  {"lawn green", "#7cfc00"},  {"LawnGreen", "#7cfc00"},
  {"green", "#00ff00"},  {"chartreuse", "#7fff00"},  {"medium spring green", "#00fa9a"},
  {"MediumSpringGreen", "#00fa9a"},  {"green yellow", "#adff2f"},  {"GreenYellow", "#adff2f"},
  {"lime green", "#32cd32"},  {"LimeGreen", "#32cd32"},  {"yellow green", "#9acd32"},
  {"YellowGreen", "#9acd32"},  {"forest green", "#228b22"},  {"ForestGreen", "#228b22"},
  {"olive drab", "#6b8e23"},  {"OliveDrab", "#6b8e23"},  {"dark khaki", "#bdb76b"},
  {"DarkKhaki", "#bdb76b"},  {"khaki", "#f0e68c"},  {"pale goldenrod", "#eee8aa"},
  {"PaleGoldenrod", "#eee8aa"},  {"light goldenrod yellow", "#fafad2"},  {"LightGoldenrodYellow", "#fafad2"},
  {"light yellow", "#ffffe0"},  {"LightYellow", "#ffffe0"},  {"yellow", "#ffff00"},
  {"gold", "#ffd700"},  {"light goldenrod", "#eedd82"},  {"LightGoldenrod", "#eedd82"},
  {"goldenrod", "#daa520"},  {"dark goldenrod", "#b8860b"},  {"DarkGoldenrod", "#b8860b"},
  {"rosy brown", "#bc8f8f"},  {"RosyBrown", "#bc8f8f"},  {"indian red", "#cd5c5c"},
  {"IndianRed", "#cd5c5c"},  {"saddle brown", "#8b4513"},  {"SaddleBrown", "#8b4513"},
  {"sienna", "#a0522d"},  {"peru", "#cd853f"},  {"burlywood", "#deb887"},
  {"beige", "#f5f5dc"},  {"wheat", "#f5deb3"},  {"sandy brown", "#f4a460"},
  {"SandyBrown", "#f4a460"},  {"tan", "#d2b48c"},  {"chocolate", "#d2691e"},
  {"firebrick", "#b22222"},  {"brown", "#a52a2a"},  {"dark salmon", "#e9967a"},
  {"DarkSalmon", "#e9967a"},  {"salmon", "#fa8072"},  {"light salmon", "#ffa07a"},
  {"LightSalmon", "#ffa07a"},  {"orange", "#ffa500"},  {"dark orange", "#ff8c00"},
  {"DarkOrange", "#ff8c00"},  {"coral", "#ff7f50"},  {"light coral", "#f08080"},
  {"LightCoral", "#f08080"},  {"tomato", "#ff6347"},  {"orange red", "#ff4500"},
  {"OrangeRed", "#ff4500"},  {"red", "#ff0000"},  {"hot pink", "#ff69b4"},
  {"HotPink", "#ff69b4"},  {"deep pink", "#ff1493"},  {"DeepPink", "#ff1493"},
  {"pink", "#ffc0cb"},  {"light pink", "#ffb6c1"},  {"LightPink", "#ffb6c1"},
  {"pale violet red", "#db7093"},  {"PaleVioletRed", "#db7093"},  {"maroon", "#b03060"},
  {"medium violet red", "#c71585"},  {"MediumVioletRed", "#c71585"},  {"violet red", "#d02090"},
  {"VioletRed", "#d02090"},  {"magenta", "#ff00ff"},  {"violet", "#ee82ee"},
  {"plum", "#dda0dd"},  {"orchid", "#da70d6"},  {"medium orchid", "#ba55d3"},
  {"MediumOrchid", "#ba55d3"},  {"dark orchid", "#9932cc"},  {"DarkOrchid", "#9932cc"},
  {"dark violet", "#9400d3"},  {"DarkViolet", "#9400d3"},  {"blue violet", "#8a2be2"},
  {"BlueViolet", "#8a2be2"},  {"purple", "#a020f0"},  {"medium purple", "#9370db"},
  {"MediumPurple", "#9370db"},  {"thistle", "#d8bfd8"},  {"snow1", "#fffafa"},
  {"snow2", "#eee9e9"},  {"snow3", "#cdc9c9"},  {"snow4", "#8b8989"},
  {"seashell1", "#fff5ee"},  {"seashell2", "#eee5de"},  {"seashell3", "#cdc5bf"},
  {"seashell4", "#8b8682"},  {"AntiqueWhite1", "#ffefdb"},  {"AntiqueWhite2", "#eedfcc"},
  {"AntiqueWhite3", "#cdc0b0"},  {"AntiqueWhite4", "#8b8378"},  {"bisque1", "#ffe4c4"},
  {"bisque2", "#eed5b7"},  {"bisque3", "#cdb79e"},  {"bisque4", "#8b7d6b"},
  {"PeachPuff1", "#ffdab9"},  {"PeachPuff2", "#eecbad"},  {"PeachPuff3", "#cdaf95"},
  {"PeachPuff4", "#8b7765"},  {"NavajoWhite1", "#ffdead"},  {"NavajoWhite2", "#eecfa1"},
  {"NavajoWhite3", "#cdb38b"},  {"NavajoWhite4", "#8b795e"},  {"LemonChiffon1", "#fffacd"},
  {"LemonChiffon2", "#eee9bf"},  {"LemonChiffon3", "#cdc9a5"},  {"LemonChiffon4", "#8b8970"},
  {"cornsilk1", "#fff8dc"},  {"cornsilk2", "#eee8cd"},  {"cornsilk3", "#cdc8b1"},
  {"cornsilk4", "#8b8878"},  {"ivory1", "#fffff0"},  {"ivory2", "#eeeee0"},
  {"ivory3", "#cdcdc1"},  {"ivory4", "#8b8b83"},  {"honeydew1", "#f0fff0"},
  {"honeydew2", "#e0eee0"},  {"honeydew3", "#c1cdc1"},  {"honeydew4", "#838b83"},
  {"LavenderBlush1", "#fff0f5"},  {"LavenderBlush2", "#eee0e5"},  {"LavenderBlush3", "#cdc1c5"},
  {"LavenderBlush4", "#8b8386"},  {"MistyRose1", "#ffe4e1"},  {"MistyRose2", "#eed5d2"},
  {"MistyRose3", "#cdb7b5"},  {"MistyRose4", "#8b7d7b"},  {"azure1", "#f0ffff"},
  {"azure2", "#e0eeee"},  {"azure3", "#c1cdcd"},  {"azure4", "#838b8b"},
  {"SlateBlue1", "#836fff"},  {"SlateBlue2", "#7a67ee"},  {"SlateBlue3", "#6959cd"},
  {"SlateBlue4", "#473c8b"},  {"RoyalBlue1", "#4876ff"},  {"RoyalBlue2", "#436eee"},
  {"RoyalBlue3", "#3a5fcd"},  {"RoyalBlue4", "#27408b"},  {"blue1", "#0000ff"},
  {"blue2", "#0000ee"},  {"blue3", "#0000cd"},  {"blue4", "#00008b"},
  {"DodgerBlue1", "#1e90ff"},  {"DodgerBlue2", "#1c86ee"},  {"DodgerBlue3", "#1874cd"},
  {"DodgerBlue4", "#104e8b"},  {"SteelBlue1", "#63b8ff"},  {"SteelBlue2", "#5cacee"},
  {"SteelBlue3", "#4f94cd"},  {"SteelBlue4", "#36648b"},  {"DeepSkyBlue1", "#00bfff"},
  {"DeepSkyBlue2", "#00b2ee"},  {"DeepSkyBlue3", "#009acd"},  {"DeepSkyBlue4", "#00688b"},
  {"SkyBlue1", "#87ceff"},  {"SkyBlue2", "#7ec0ee"},  {"SkyBlue3", "#6ca6cd"},
  {"SkyBlue4", "#4a708b"},  {"LightSkyBlue1", "#b0e2ff"},  {"LightSkyBlue2", "#a4d3ee"},
  {"LightSkyBlue3", "#8db6cd"},  {"LightSkyBlue4", "#607b8b"},  {"SlateGray1", "#c6e2ff"},
  {"SlateGray2", "#b9d3ee"},  {"SlateGray3", "#9fb6cd"},  {"SlateGray4", "#6c7b8b"},
  {"LightSteelBlue1", "#cae1ff"},  {"LightSteelBlue2", "#bcd2ee"},  {"LightSteelBlue3", "#a2b5cd"},
  {"LightSteelBlue4", "#6e7b8b"},  {"LightBlue1", "#bfefff"},  {"LightBlue2", "#b2dfee"},
  {"LightBlue3", "#9ac0cd"},  {"LightBlue4", "#68838b"},  {"LightCyan1", "#e0ffff"},
  {"LightCyan2", "#d1eeee"},  {"LightCyan3", "#b4cdcd"},  {"LightCyan4", "#7a8b8b"},
  {"PaleTurquoise1", "#bbffff"},  {"PaleTurquoise2", "#aeeeee"},  {"PaleTurquoise3", "#96cdcd"},
  {"PaleTurquoise4", "#668b8b"},  {"CadetBlue1", "#98f5ff"},  {"CadetBlue2", "#8ee5ee"},
  {"CadetBlue3", "#7ac5cd"},  {"CadetBlue4", "#53868b"},  {"turquoise1", "#00f5ff"},
  {"turquoise2", "#00e5ee"},  {"turquoise3", "#00c5cd"},  {"turquoise4", "#00868b"},
  {"cyan1", "#00ffff"},  {"cyan2", "#00eeee"},  {"cyan3", "#00cdcd"},
  {"cyan4", "#008b8b"},  {"DarkSlateGray1", "#97ffff"},  {"DarkSlateGray2", "#8deeee"},
  {"DarkSlateGray3", "#79cdcd"},  {"DarkSlateGray4", "#528b8b"},  {"aquamarine1", "#7fffd4"},
  {"aquamarine2", "#76eec6"},  {"aquamarine3", "#66cdaa"},  {"aquamarine4", "#458b74"},
  {"DarkSeaGreen1", "#c1ffc1"},  {"DarkSeaGreen2", "#b4eeb4"},  {"DarkSeaGreen3", "#9bcd9b"},
  {"DarkSeaGreen4", "#698b69"},  {"SeaGreen1", "#54ff9f"},  {"SeaGreen2", "#4eee94"},
  {"SeaGreen3", "#43cd80"},  {"SeaGreen4", "#2e8b57"},  {"PaleGreen1", "#9aff9a"},
  {"PaleGreen2", "#90ee90"},  {"PaleGreen3", "#7ccd7c"},  {"PaleGreen4", "#548b54"},
  {"SpringGreen1", "#00ff7f"},  {"SpringGreen2", "#00ee76"},  {"SpringGreen3", "#00cd66"},
  {"SpringGreen4", "#008b45"},  {"green1", "#00ff00"},  {"green2", "#00ee00"},
  {"green3", "#00cd00"},  {"green4", "#008b00"},  {"chartreuse1", "#7fff00"},
  {"chartreuse2", "#76ee00"},  {"chartreuse3", "#66cd00"},  {"chartreuse4", "#458b00"},
  {"OliveDrab1", "#c0ff3e"},  {"OliveDrab2", "#b3ee3a"},  {"OliveDrab3", "#9acd32"},
  {"OliveDrab4", "#698b22"},  {"DarkOliveGreen1", "#caff70"},  {"DarkOliveGreen2", "#bcee68"},
  {"DarkOliveGreen3", "#a2cd5a"},  {"DarkOliveGreen4", "#6e8b3d"},  {"khaki1", "#fff68f"},
  {"khaki2", "#eee685"},  {"khaki3", "#cdc673"},  {"khaki4", "#8b864e"},
  {"LightGoldenrod1", "#ffec8b"},  {"LightGoldenrod2", "#eedc82"},  {"LightGoldenrod3", "#cdbe70"},
  {"LightGoldenrod4", "#8b814c"},  {"LightYellow1", "#ffffe0"},  {"LightYellow2", "#eeeed1"},
  {"LightYellow3", "#cdcdb4"},  {"LightYellow4", "#8b8b7a"},  {"yellow1", "#ffff00"},
  {"yellow2", "#eeee00"},  {"yellow3", "#cdcd00"},  {"yellow4", "#8b8b00"},
  {"gold1", "#ffd700"},  {"gold2", "#eec900"},  {"gold3", "#cdad00"},
  {"gold4", "#8b7500"},  {"goldenrod1", "#ffc125"},  {"goldenrod2", "#eeb422"},
  {"goldenrod3", "#cd9b1d"},  {"goldenrod4", "#8b6914"},  {"DarkGoldenrod1", "#ffb90f"},
  {"DarkGoldenrod2", "#eead0e"},  {"DarkGoldenrod3", "#cd950c"},  {"DarkGoldenrod4", "#8b6508"},
  {"RosyBrown1", "#ffc1c1"},  {"RosyBrown2", "#eeb4b4"},  {"RosyBrown3", "#cd9b9b"},
  {"RosyBrown4", "#8b6969"},  {"IndianRed1", "#ff6a6a"},  {"IndianRed2", "#ee6363"},
  {"IndianRed3", "#cd5555"},  {"IndianRed4", "#8b3a3a"},  {"sienna1", "#ff8247"},
  {"sienna2", "#ee7942"},  {"sienna3", "#cd6839"},  {"sienna4", "#8b4726"},
  {"burlywood1", "#ffd39b"},  {"burlywood2", "#eec591"},  {"burlywood3", "#cdaa7d"},
  {"burlywood4", "#8b7355"},  {"wheat1", "#ffe7ba"},  {"wheat2", "#eed8ae"},
  {"wheat3", "#cdba96"},  {"wheat4", "#8b7e66"},  {"tan1", "#ffa54f"},
  {"tan2", "#ee9a49"},  {"tan3", "#cd853f"},  {"tan4", "#8b5a2b"},
  {"chocolate1", "#ff7f24"},  {"chocolate2", "#ee7621"},  {"chocolate3", "#cd661d"},
  {"chocolate4", "#8b4513"},  {"firebrick1", "#ff3030"},  {"firebrick2", "#ee2c2c"},
  {"firebrick3", "#cd2626"},  {"firebrick4", "#8b1a1a"},  {"brown1", "#ff4040"},
  {"brown2", "#ee3b3b"},  {"brown3", "#cd3333"},  {"brown4", "#8b2323"},
  {"salmon1", "#ff8c69"},  {"salmon2", "#ee8262"},  {"salmon3", "#cd7054"},
  {"salmon4", "#8b4c39"},  {"LightSalmon1", "#ffa07a"},  {"LightSalmon2", "#ee9572"},
  {"LightSalmon3", "#cd8162"},  {"LightSalmon4", "#8b5742"},  {"orange1", "#ffa500"},
  {"orange2", "#ee9a00"},  {"orange3", "#cd8500"},  {"orange4", "#8b5a00"},
  {"DarkOrange1", "#ff7f00"},  {"DarkOrange2", "#ee7600"},  {"DarkOrange3", "#cd6600"},
  {"DarkOrange4", "#8b4500"},  {"coral1", "#ff7256"},  {"coral2", "#ee6a50"},
  {"coral3", "#cd5b45"},  {"coral4", "#8b3e2f"},  {"tomato1", "#ff6347"},
  {"tomato2", "#ee5c42"},  {"tomato3", "#cd4f39"},  {"tomato4", "#8b3626"},
  {"OrangeRed1", "#ff4500"},  {"OrangeRed2", "#ee4000"},  {"OrangeRed3", "#cd3700"},
  {"OrangeRed4", "#8b2500"},  {"red1", "#ff0000"},  {"red2", "#ee0000"},
  {"red3", "#cd0000"},  {"red4", "#8b0000"},  {"DeepPink1", "#ff1493"},
  {"DeepPink2", "#ee1289"},  {"DeepPink3", "#cd1076"},  {"DeepPink4", "#8b0a50"},
  {"HotPink1", "#ff6eb4"},  {"HotPink2", "#ee6aa7"},  {"HotPink3", "#cd6090"},
  {"HotPink4", "#8b3a62"},  {"pink1", "#ffb5c5"},  {"pink2", "#eea9b8"},
  {"pink3", "#cd919e"},  {"pink4", "#8b636c"},  {"LightPink1", "#ffaeb9"},
  {"LightPink2", "#eea2ad"},  {"LightPink3", "#cd8c95"},  {"LightPink4", "#8b5f65"},
  {"PaleVioletRed1", "#ff82ab"},  {"PaleVioletRed2", "#ee799f"},  {"PaleVioletRed3", "#cd6889"},
  {"PaleVioletRed4", "#8b475d"},  {"maroon1", "#ff34b3"},  {"maroon2", "#ee30a7"},
  {"maroon3", "#cd2990"},  {"maroon4", "#8b1c62"},  {"VioletRed1", "#ff3e96"},
  {"VioletRed2", "#ee3a8c"},  {"VioletRed3", "#cd3278"},  {"VioletRed4", "#8b2252"},
  {"magenta1", "#ff00ff"},  {"magenta2", "#ee00ee"},  {"magenta3", "#cd00cd"},
  {"magenta4", "#8b008b"},  {"orchid1", "#ff83fa"},  {"orchid2", "#ee7ae9"},
  {"orchid3", "#cd69c9"},  {"orchid4", "#8b4789"},  {"plum1", "#ffbbff"},
  {"plum2", "#eeaeee"},  {"plum3", "#cd96cd"},  {"plum4", "#8b668b"},
  {"MediumOrchid1", "#e066ff"},  {"MediumOrchid2", "#d15fee"},  {"MediumOrchid3", "#b452cd"},
  {"MediumOrchid4", "#7a378b"},  {"DarkOrchid1", "#bf3eff"},  {"DarkOrchid2", "#b23aee"},
  {"DarkOrchid3", "#9a32cd"},  {"DarkOrchid4", "#68228b"},  {"purple1", "#9b30ff"},
  {"purple2", "#912cee"},  {"purple3", "#7d26cd"},  {"purple4", "#551a8b"},
  {"MediumPurple1", "#ab82ff"},  {"MediumPurple2", "#9f79ee"},  {"MediumPurple3", "#8968cd"},
  {"MediumPurple4", "#5d478b"},  {"thistle1", "#ffe1ff"},  {"thistle2", "#eed2ee"},
  {"thistle3", "#cdb5cd"},  {"thistle4", "#8b7b8b"},  {"gray0", "#000000"},
  {"grey0", "#000000"},  {"gray1", "#030303"},  {"grey1", "#030303"},
  {"gray2", "#050505"},  {"grey2", "#050505"},  {"gray3", "#080808"},
  {"grey3", "#080808"},  {"gray4", "#0a0a0a"},  {"grey4", "#0a0a0a"},
  {"gray5", "#0d0d0d"},  {"grey5", "#0d0d0d"},  {"gray6", "#0f0f0f"},
  {"grey6", "#0f0f0f"},  {"gray7", "#121212"},  {"grey7", "#121212"},
  {"gray8", "#141414"},  {"grey8", "#141414"},  {"gray9", "#171717"},
  {"grey9", "#171717"},  {"gray10", "#1a1a1a"},  {"grey10", "#1a1a1a"},
  {"gray11", "#1c1c1c"},  {"grey11", "#1c1c1c"},  {"gray12", "#1f1f1f"},
  {"grey12", "#1f1f1f"},  {"gray13", "#212121"},  {"grey13", "#212121"},
  {"gray14", "#242424"},  {"grey14", "#242424"},  {"gray15", "#262626"},
  {"grey15", "#262626"},  {"gray16", "#292929"},  {"grey16", "#292929"},
  {"gray17", "#2b2b2b"},  {"grey17", "#2b2b2b"},  {"gray18", "#2e2e2e"},
  {"grey18", "#2e2e2e"},  {"gray19", "#303030"},  {"grey19", "#303030"},
  {"gray20", "#333333"},  {"grey20", "#333333"},  {"gray21", "#363636"},
  {"grey21", "#363636"},  {"gray22", "#383838"},  {"grey22", "#383838"},
  {"gray23", "#3b3b3b"},  {"grey23", "#3b3b3b"},  {"gray24", "#3d3d3d"},
  {"grey24", "#3d3d3d"},  {"gray25", "#404040"},  {"grey25", "#404040"},
  {"gray26", "#424242"},  {"grey26", "#424242"},  {"gray27", "#454545"},
  {"grey27", "#454545"},  {"gray28", "#474747"},  {"grey28", "#474747"},
  {"gray29", "#4a4a4a"},  {"grey29", "#4a4a4a"},  {"gray30", "#4d4d4d"},
  {"grey30", "#4d4d4d"},  {"gray31", "#4f4f4f"},  {"grey31", "#4f4f4f"},
  {"gray32", "#525252"},  {"grey32", "#525252"},  {"gray33", "#545454"},
  {"grey33", "#545454"},  {"gray34", "#575757"},  {"grey34", "#575757"},
  {"gray35", "#595959"},  {"grey35", "#595959"},  {"gray36", "#5c5c5c"},
  {"grey36", "#5c5c5c"},  {"gray37", "#5e5e5e"},  {"grey37", "#5e5e5e"},
  {"gray38", "#616161"},  {"grey38", "#616161"},  {"gray39", "#636363"},
  {"grey39", "#636363"},  {"gray40", "#666666"},  {"grey40", "#666666"},
  {"gray41", "#696969"},  {"grey41", "#696969"},  {"gray42", "#6b6b6b"},
  {"grey42", "#6b6b6b"},  {"gray43", "#6e6e6e"},  {"grey43", "#6e6e6e"},
  {"gray44", "#707070"},  {"grey44", "#707070"},  {"gray45", "#737373"},
  {"grey45", "#737373"},  {"gray46", "#757575"},  {"grey46", "#757575"},
  {"gray47", "#787878"},  {"grey47", "#787878"},  {"gray48", "#7a7a7a"},
  {"grey48", "#7a7a7a"},  {"gray49", "#7d7d7d"},  {"grey49", "#7d7d7d"},
  {"gray50", "#7f7f7f"},  {"grey50", "#7f7f7f"},  {"gray51", "#828282"},
  {"grey51", "#828282"},  {"gray52", "#858585"},  {"grey52", "#858585"},
  {"gray53", "#878787"},  {"grey53", "#878787"},  {"gray54", "#8a8a8a"},
  {"grey54", "#8a8a8a"},  {"gray55", "#8c8c8c"},  {"grey55", "#8c8c8c"},
  {"gray56", "#8f8f8f"},  {"grey56", "#8f8f8f"},  {"gray57", "#919191"},
  {"grey57", "#919191"},  {"gray58", "#949494"},  {"grey58", "#949494"},
  {"gray59", "#969696"},  {"grey59", "#969696"},  {"gray60", "#999999"},
  {"grey60", "#999999"},  {"gray61", "#9c9c9c"},  {"grey61", "#9c9c9c"},
  {"gray62", "#9e9e9e"},  {"grey62", "#9e9e9e"},  {"gray63", "#a1a1a1"},
  {"grey63", "#a1a1a1"},  {"gray64", "#a3a3a3"},  {"grey64", "#a3a3a3"},
  {"gray65", "#a6a6a6"},  {"grey65", "#a6a6a6"},  {"gray66", "#a8a8a8"},
  {"grey66", "#a8a8a8"},  {"gray67", "#ababab"},  {"grey67", "#ababab"},
  {"gray68", "#adadad"},  {"grey68", "#adadad"},  {"gray69", "#b0b0b0"},
  {"grey69", "#b0b0b0"},  {"gray70", "#b3b3b3"},  {"grey70", "#b3b3b3"},
  {"gray71", "#b5b5b5"},  {"grey71", "#b5b5b5"},  {"gray72", "#b8b8b8"},
  {"grey72", "#b8b8b8"},  {"gray73", "#bababa"},  {"grey73", "#bababa"},
  {"gray74", "#bdbdbd"},  {"grey74", "#bdbdbd"},  {"gray75", "#bfbfbf"},
  {"grey75", "#bfbfbf"},  {"gray76", "#c2c2c2"},  {"grey76", "#c2c2c2"},
  {"gray77", "#c4c4c4"},  {"grey77", "#c4c4c4"},  {"gray78", "#c7c7c7"},
  {"grey78", "#c7c7c7"},  {"gray79", "#c9c9c9"},  {"grey79", "#c9c9c9"},
  {"gray80", "#cccccc"},  {"grey80", "#cccccc"},  {"gray81", "#cfcfcf"},
  {"grey81", "#cfcfcf"},  {"gray82", "#d1d1d1"},  {"grey82", "#d1d1d1"},
  {"gray83", "#d4d4d4"},  {"grey83", "#d4d4d4"},  {"gray84", "#d6d6d6"},
  {"grey84", "#d6d6d6"},  {"gray85", "#d9d9d9"},  {"grey85", "#d9d9d9"},
  {"gray86", "#dbdbdb"},  {"grey86", "#dbdbdb"},  {"gray87", "#dedede"},
  {"grey87", "#dedede"},  {"gray88", "#e0e0e0"},  {"grey88", "#e0e0e0"},
  {"gray89", "#e3e3e3"},  {"grey89", "#e3e3e3"},  {"gray90", "#e5e5e5"},
  {"grey90", "#e5e5e5"},  {"gray91", "#e8e8e8"},  {"grey91", "#e8e8e8"},
  {"gray92", "#ebebeb"},  {"grey92", "#ebebeb"},  {"gray93", "#ededed"},
  {"grey93", "#ededed"},  {"gray94", "#f0f0f0"},  {"grey94", "#f0f0f0"},
  {"gray95", "#f2f2f2"},  {"grey95", "#f2f2f2"},  {"gray96", "#f5f5f5"},
  {"grey96", "#f5f5f5"},  {"gray97", "#f7f7f7"},  {"grey97", "#f7f7f7"},
  {"gray98", "#fafafa"},  {"grey98", "#fafafa"},  {"gray99", "#fcfcfc"},
  {"grey99", "#fcfcfc"},  {"gray100", "#ffffff"},  {"grey100", "#ffffff"},
{0}
};

const char* ColorRep::nameToRGB(const char* colormap, const char* name)
{
	MWassert(colormap);
	MWassert(name);

#if OCSMALL
	int i;
	for (i = 0; cc[i].name; ++i) {
		if (strcmp(cc[i].name, name) == 0) {
			return cc[i].value;
		}
	}
#else
 if (!Session::installLocation()) {
	int i;
	for (i = 0; cc[i].name; ++i) {
		if (strcmp(cc[i].name, name) == 0) {
			return cc[i].value;
		}
	}
 }else{
	if (COLORMAP_FILE == NULL)
	{
		const char* loc = Session::installLocation();
		const char* leafname = "/colormap.ini";
		COLORMAP_FILE = new char[ strlen(loc) + strlen(leafname) + 1];
		strcpy(COLORMAP_FILE, loc);
		strcat(COLORMAP_FILE, leafname);
	}

	static char rgbName[10];
	if (GetPrivateProfileString(colormap, name, "", rgbName, 10,
		COLORMAP_FILE))
	{
		return rgbName;
	}
	else {
	int i;
	for (i = 0; cc[i].name; ++i) {
		if (strcmp(cc[i].name, name) == 0) {
			return cc[i].value;
		}
	}
	}
 }
#endif
	return NULL;
}

const Color* ColorRep::rgbToColor(const char* name)
{
	if (name[0] == '#')
	{
		int r;
		int g;
		int b;
		sscanf(&(name[1]), "%2x", &r);
		sscanf(&(name[3]), "%2x", &g);
		sscanf(&(name[5]), "%2x", &b);
		const Color* c = new Color( 
			(ColorIntensity) (r/255.0),
			(ColorIntensity) (g/255.0),
			(ColorIntensity) (b/255.0),
			1.0);
		return c;
	}
	return nil;
}

// #######################################################################
// ################# class Color
// #######################################################################

// The following data is used to create 16 stipple patterns used when the
// alpha value of the color is set to something other than 1.0.
static unsigned char stippleData[16][8] = 
{ 
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x11, 0x00, 0x00, 0x00, 0x11, 0x00, 0x00, 0x00 },
	{ 0x11, 0x00, 0x44, 0x00, 0x11, 0x00, 0x44, 0x00 }, 
	{ 0x55, 0x00, 0x44, 0x00, 0x55, 0x00, 0x44, 0x00 },
	{ 0x55, 0x00, 0x55, 0x00, 0x55, 0x00, 0x55, 0x00 },
	{ 0x55, 0x22, 0x55, 0x00, 0x55, 0x22, 0x55, 0x00 },
	{ 0x55, 0x22, 0x55, 0x88, 0x55, 0x22, 0x55, 0x88 },
	{ 0x55, 0xAA, 0x55, 0x88, 0x55, 0xAA, 0x55, 0x88 },
	{ 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA },
	{ 0x77, 0xAA, 0x55, 0xAA, 0x77, 0xAA, 0x55, 0xAA },
	{ 0x77, 0xAA, 0xDD, 0xAA, 0x77, 0xAA, 0xDD, 0xAA },
	{ 0xFF, 0xAA, 0xDD, 0xAA, 0xFF, 0xAA, 0xDD, 0xAA },
	{ 0xFF, 0xAA, 0xFF, 0xAA, 0xFF, 0xAA, 0xFF, 0xAA },
	{ 0xFF, 0xBB, 0xFF, 0xAA, 0xFF, 0xBB, 0xFF, 0xAA },
	{ 0xFF, 0xBB, 0xFF, 0xEE, 0xFF, 0xBB, 0xFF, 0xEE },
	{ 0xFF, 0xFF, 0xFF, 0xEE, 0xFF, 0xFF, 0xFF, 0xEE },
};

Color::Color(
	ColorIntensity r,					// red component of color
	ColorIntensity g,					// green component of color
	ColorIntensity b,                   // blue component of color
	float alpha,
	ColorOp op)
{
	int red = (int) (r * 255);
	int green = (int) (g * 255);
	int blue = (int) (b * 255);

	impl_ = new ColorRep(red, green, blue);
    impl_->op = op;

	// ---- set stipple pattern if dither desired ----
	impl_->alpha = alpha;
	if ((alpha > 0.9999) && (alpha < 1.0001))
	{
		impl_->stipple = NULL;
	}
	else
	{
		int index = int(alpha * 16);
		index = (index > 15) ? 15 : index;
		index = (index < 0) ? 0 : index;
		impl_->stipple = new Bitmap(stippleData[index], 8, 8);
	}
}

Color::Color(
	const Color& color,
	float alpha,
	ColorOp op)
{
	COLORREF cref = color.impl_->msColor();
	int red = GetRValue(cref);
	int green = GetGValue(cref);
	int blue = GetBValue(cref);

	impl_ = new ColorRep(red, blue, green);
    impl_->op = op;

	// ---- set stipple pattern if dither desired ----
	impl_->alpha = alpha;
	if ((alpha > 0.9999) && (alpha < 1.0001))
	{
		impl_->stipple = NULL;
	}
	else
	{
		int index = int(alpha * 16);
		index = (index > 15) ? 15 : index;
		index = (index < 0) ? 0 : index;
		impl_->stipple = new Bitmap(stippleData[index], 8, 8);
	}
}

Color::~Color()
{
	delete impl_->stipple;
	delete impl_;
}

void Color::intensities(
	Display*,
	ColorIntensity& r,
	ColorIntensity& g,
	ColorIntensity& b) const
{
	intensities(r, g, b);
}

void Color::intensities(
	ColorIntensity& r,
	ColorIntensity& g,
	ColorIntensity& b) const
{
	COLORREF cref = impl_->msColor();

	r = (ColorIntensity) (((float) GetRValue(cref)) / 255.0);
	g = (ColorIntensity) (((float) GetGValue(cref)) / 255.0);
	b = (ColorIntensity) (((float) GetBValue(cref)) / 255.0);
}

float Color::alpha() const
{
    return impl_->alpha;
}

const Color* Color::brightness(float adjust) const
{
    ColorIntensity r, g, b;
    intensities(r, g, b);
	if (adjust >= 0)
	{
		r += (1 - r) * adjust;
		g += (1 - g) * adjust;
		b += (1 - b) * adjust;
	}
	else
	{
		float f = (float) (adjust + 1.0);
		r *= f;
		g *= f;
		b *= f;
    }
    return new Color(r, g, b);
}

boolean Color::distinguished(const Color* c) const 
{
    return distinguished(Session::instance()->default_display(), c);
}

boolean Color::distinguished(Display*, const Color* color) const
{
	COLORREF cref = color->impl_->msColor();
	int red = GetRValue(cref);
	int green = GetGValue(cref);
	int blue = GetBValue(cref);

	COLORREF bogus;
	return ( globalPalette.findEntry(red,green,blue,bogus) ) ? false : true;
}

// ---------------------------------------------------------------------------
// Lookup color by name.  This is intended to look things up by the X11
// style name, which is partially supported under the Windows implementation.
// If the name starts with a '#' then we translate the hex numbers to rgb 
// values directly.  Otherwise, we attempt to look up the color name in
// the default colormap.
// ---------------------------------------------------------------------------
const Color* Color::lookup(Display*, const char* name)
{
	if (name)
	{
		// ---- check for rgb specification ----
		if (name[0] == '#')
			return ColorRep::rgbToColor(name);

		// ---- must be a color name ----
		const char* rgb;
		if (rgb = ColorRep::nameToRGB("default", name))
		{
			return ColorRep::rgbToColor(rgb);
		}
	}
	
	return nil;
}

const Color* Color::lookup(Display* d, const String& s) 
{
	// Since the value is not expected to be null terminiated, we simply
	// pass it through to the function that expects a (const char*) arg.
	return lookup(d, s.string());
}

boolean Color::find(
    const Display* display, const String& name,
    ColorIntensity& r, ColorIntensity& g, ColorIntensity& b) 
{
	NullTerminatedString nm(name);
    return find(display, nm.string(), r, g, b);
}

boolean Color::find(
    const Display*, 
	const char* name,
    ColorIntensity& r, 
	ColorIntensity& g, 
	ColorIntensity& b) 
{
	const char* rgb;
	if (rgb = ColorRep::nameToRGB("default", name))
	{
		if (rgb[0] == '#')
		{
			int ir;
			int ig;
			int ib;
			sscanf(&(rgb[1]), "%2x", &ir);
			sscanf(&(rgb[3]), "%2x", &ig);
			sscanf(&(rgb[5]), "%2x", &ib);

			r = (ColorIntensity) (ir/255.0);
			g = (ColorIntensity) (ig/255.0);
			b = (ColorIntensity) (ib/255.0);
			return true;
		}
	}
	return false;
}

