/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2025 APAmk2, Vladislav4KZ, Velaron

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

/*
Crosshair options. Every change goes to the cvars at once, so the crosshair
in game follows the page; Cancel puts back the values the page opened with.

With the enhanced crosshair (xhair_*), the preview draws the crosshair with
the client's own geometry (cl_dll/crosshair_geom.cpp, built into this
library when the parent project defines MAINUI_CROSSHAIR_GEOM): at 1:1 it
is the in-game crosshair pixel for pixel, and the zoomed view shows each of
these pixels four times larger.
*/

#include <stdlib.h>
#include "Framework.h"
#include "PicButton.h"
#include "Slider.h"
#include "CheckBox.h"
#include "SpinControl.h"
#include "Field.h"
#include "Action.h"
#include "StringArrayModel.h"
#include "Scissor.h"
#ifdef MAINUI_CROSSHAIR_GEOM
#include "crosshair_geom.h"
#endif

#define ART_BANNER "gfx/shell/head_xhair"

#define XHAIR_ZOOM 4
#define XHAIR_ROW_STEP 36

static const char *g_szCrosshairSizes[] = { "auto", "small", "medium", "large" };

static struct
{
	const char *name;
	int r, g, b;
} g_CrosshairColors[] = {
	{ "#Valve_Green", 50, 250, 50 },
	{ "#Valve_Red", 250, 50, 50 },
	{ "#Valve_Blue", 50, 50, 250 },
	{ "#Valve_Yellow", 250, 250, 50 },
	{ "#Valve_Ltblue", 50, 250, 250 }
};

// every cvar of the page with its default, as the client creates them
static const struct
{
	const char *name;
	const char *def;
} g_CrosshairCvars[] = {
	{ "xhair_enable", "0" },
	{ "xhair_style", "0" },
	{ "xhair_t", "0" },
	{ "xhair_gap", "0" },
	{ "xhair_size", "4" },
	{ "xhair_thick", "0" },
	{ "xhair_dot", "0" },
	{ "xhair_dot_size", "0" },
	{ "xhair_pad", "0" },
	{ "xhair_color", "0 255 0 255" },
	{ "xhair_pad_color", "0 0 0" },
	{ "xhair_additive", "0" },
	{ "xhair_aa", "1" },
	{ "xhair_scale", "1" },
	{ "xhair_dynamic_move", "1" },
	{ "xhair_dynamic_scale", "0" },
	{ "xhair_gap_useweaponvalue", "0" },
	{ "cl_crosshair_size", "auto" },
	{ "cl_crosshair_color", "50 250 50" },
	{ "cl_crosshair_translucent", "1" },
};

class CMenuCrosshair;

// a labelled value: a slider for quick changes and a field for exact ones
class CMenuCrosshairValue
{
public:
	void Setup( CMenuCrosshair *page, const char *name, float min, float max, float step, int precision );
	void SetCoord( int x, int y );
	void SetVisible( bool visible );
	void SetValue( float v );
	float Value() const { return value; }

	CMenuAction label;
	CMenuSlider slider;
	CMenuField field;

private:
	static void SliderChanged( CMenuBaseItem *pSelf, void *pExtra );
	static void FieldChanged( CMenuBaseItem *pSelf, void *pExtra );
	void UpdateField();

	CMenuCrosshair *page;
	float value;
	int precision;
};

class CMenuCrosshairPreview : public CMenuBaseItem
{
public:
	CMenuCrosshairPreview();

	void Think() override;
	void Draw() override;

	HIMAGE hMap;
	HIMAGE hWhite;

private:
	void DrawBackground( Point pt, Size sz, int zoom );
	void DrawClassic( Point origin, int zoom );
#ifdef MAINUI_CROSSHAIR_GEOM
	void DrawLayer( const xhair_params_t *params, bool outline, const float rgba[4], bool additive, Point origin, int zoom );
#endif

	float dynamicGap; // 640x480 units above the resting gap
	int lastTime;
	int nextShot;
};

class CMenuCrosshair : public CMenuFramework
{
public:
	CMenuCrosshair() : CMenuFramework( "CMenuCrosshair" ) { }

	void Show() override;
	void Reload() override;

	// controls to cvars, after any change
	void Apply();

	CMenuCheckBox enable;

	// classic crosshair
	CMenuSpinControl classicSize;
	CMenuSpinControl classicColor;
	CMenuCheckBox classicTranslucent;

	// enhanced crosshair
	CMenuAction panel, previewPanel;
	CMenuAction styleLabel;
	CMenuSpinControl style;
	CMenuCrosshairValue length, gap, thickness, dotSize, outline, scale;
	CMenuCheckBox dot, antialias, additive;
	CMenuCrosshairValue color[4];
	CMenuCrosshairValue outlineColor[4];
	CMenuCheckBox dynamicMove, gapUseWeapon;
	CMenuCrosshairValue dynamicScale;
	bool outlineAlphaSet; // xhair_pad_color has its own alpha

	CMenuCrosshairPreview preview;
	CMenuSpinControl background;
	CMenuCheckBox fire;

	CMenuPicButton done, cancel, defaults;

private:
	void _Init() override;
	void UpdateVisibility();
	void Revert();
	void UseDefaults();
	void SetCvars( bool defaults );

	char saved[V_ARRAYSIZE( g_CrosshairCvars )][CS_SIZE];
	bool loading;
};

/*
=================
CMenuCrosshairValue
=================
*/
void CMenuCrosshairValue::Setup( CMenuCrosshair *p, const char *name, float min, float max, float step, int prec )
{
	page = p;
	precision = prec;
	value = min;

	label.szName = name;
	label.iFlags = QMF_INACTIVE | QMF_DROPSHADOW;
	label.colorBase = uiColorHelp;
	label.SetCharSize( QM_SMALLFONT );

	slider.Setup( min, max, step );
	slider.onChanged = SliderChanged;
	slider.onChanged.pExtra = this;

	field.iMaxLength = 8;
	field.SetCharSize( QM_SMALLFONT );
	field.onChanged = FieldChanged;
	field.onChanged.pExtra = this;

	page->AddItem( label );
	page->AddItem( slider );
	page->AddItem( field );
}

void CMenuCrosshairValue::SetCoord( int x, int y )
{
	label.SetRect( x, y, 150, 26 );
	slider.SetRect( x + 150, y + 6, 120, 14 );
	field.SetRect( x + 280, y, 60, 26 );
}

void CMenuCrosshairValue::SetVisible( bool visible )
{
	CMenuBaseItem *items[] = { &label, &slider, &field };

	for( size_t i = 0; i < V_ARRAYSIZE( items ); i++ )
	{
		if( visible ) items[i]->Show();
		else items[i]->Hide();
	}
}

void CMenuCrosshairValue::SetValue( float v )
{
	value = v;
	slider.SetCurrentValue( v );
	UpdateField();
}

void CMenuCrosshairValue::UpdateField()
{
	char buf[32];
	snprintf( buf, sizeof( buf ), "%.*f", precision, value );

	// no trailing zeros: 4.25, 4.5, 4
	if( strchr( buf, '.' ))
	{
		char *end = buf + strlen( buf ) - 1;
		while( *end == '0' )
			*end-- = 0;
		if( *end == '.' )
			*end = 0;
	}

	field.SetBuffer( buf );
}

void CMenuCrosshairValue::SliderChanged( CMenuBaseItem *, void *pExtra )
{
	CMenuCrosshairValue *self = (CMenuCrosshairValue *)pExtra;

	self->value = self->slider.GetCurrentValue();
	self->UpdateField();
	self->page->Apply();
}

void CMenuCrosshairValue::FieldChanged( CMenuBaseItem *, void *pExtra )
{
	CMenuCrosshairValue *self = (CMenuCrosshairValue *)pExtra;
	const char *text = self->field.GetBuffer();
	char *end;
	float v = strtof( text, &end );

	// the field keeps what is typed, values outside the slider range included
	if( end == text || *end )
		return;

	self->value = v;
	self->slider.SetCurrentValue( v );
	self->page->Apply();
}

/*
=================
CMenuCrosshairPreview
=================
*/
CMenuCrosshairPreview::CMenuCrosshairPreview() : CMenuBaseItem(),
	hMap(), hWhite(), dynamicGap(), lastTime(), nextShot()
{
	iFlags |= QMF_INACTIVE;
}

void CMenuCrosshairPreview::Think()
{
	CMenuCrosshair *page = (CMenuCrosshair *)Parent();
	const int now = uiStatic.realTime;
	float frametime = ( now - lastTime ) * 0.001f;
	bool fired = false;

	if( frametime < 0.0f || frametime > 0.1f )
		frametime = 0.0f;
	lastTime = now;

	// automatic fire at ten shots a second
	if( page->fire.bChecked && now >= nextShot )
	{
		fired = true;
		nextShot = now + 100;
	}

#ifdef MAINUI_CROSSHAIR_GEOM
	// standing still with the default weapon: resting gap 4, 3 per shot
	const float rest = 4.0f;
	dynamicGap = XHair_StepGap( rest + dynamicGap, rest, 3.0f * page->dynamicScale.Value(), fired, frametime ) - rest;
#endif
}

void CMenuCrosshairPreview::DrawBackground( Point pt, Size sz, int zoom )
{
	CMenuCrosshair *page = (CMenuCrosshair *)Parent();

	switch( (int)page->background.GetCurrentValue())
	{
	case 1:
		UI_FillRect( pt, sz, 0xFF000000 );
		break;
	case 2:
		UI_FillRect( pt, sz, 0xFFD8D8D8 );
		break;
	default:
		if( !hMap )
		{
			UI_FillRect( pt, sz, uiPromptBgColor );
			break;
		}

		// the picture enlarged like the crosshair, around the same centre
		EngFuncs::PIC_Set( hMap, 255, 255, 255 );
		EngFuncs::PIC_DrawTrans( pt.x + sz.w / 2 - sz.w * zoom / 2, pt.y + sz.h / 2 - sz.h * zoom / 2, sz.w * zoom, sz.h * zoom );
		break;
	}
}

// screen pixel x of the crosshair lands at origin.x + x * zoom, 0 being the centre
void CMenuCrosshairPreview::DrawClassic( Point origin, int zoom )
{
	CMenuCrosshair *page = (CMenuCrosshair *)Parent();
	int i = page->classicColor.GetCurrentValue();
	int r = 50, g = 250, b = 50, a = 180;
	int length;

	if( i >= 0 && i < (int)V_ARRAYSIZE( g_CrosshairColors ))
	{
		r = g_CrosshairColors[i].r;
		g = g_CrosshairColors[i].g;
		b = g_CrosshairColors[i].b;
	}

	switch( (int)page->classicSize.GetCurrentValue())
	{
	case 1: length = 10; break;
	case 2: length = 20; break;
	case 3: length = 30; break;
	default:
		if( ScreenWidth < 640 ) length = 30;
		else if( ScreenWidth < 1024 ) length = 20;
		else length = 10;
		break;
	}

	length *= ScreenHeight / 768.0f;

	const int delta = length / 2;
	const struct { int x, y, w, h; } bars[] = {
		{ 0, -delta - length, 1, length },
		{ 0, delta, 1, length },
		{ -delta - length, 0, length, 1 },
		{ delta, 0, length, 1 },
	};

	for( size_t k = 0; k < V_ARRAYSIZE( bars ); k++ )
	{
		const int x = origin.x + bars[k].x * zoom, y = origin.y + bars[k].y * zoom;

		EngFuncs::PIC_Set( hWhite, r, g, b, a );
		if( page->classicTranslucent.bChecked )
			EngFuncs::PIC_DrawAdditive( x, y, bars[k].w * zoom, bars[k].h * zoom );
		else EngFuncs::PIC_DrawTrans( x, y, bars[k].w * zoom, bars[k].h * zoom );
	}
}

#ifdef MAINUI_CROSSHAIR_GEOM
// screen pixel x of the crosshair lands at origin.x + x * zoom
void CMenuCrosshairPreview::DrawLayer( const xhair_params_t *params, bool outline, const float rgba[4], bool additive, Point origin, int zoom )
{
	static xhair_rect_t rects[XHAIR_MAX_RECTS];
	const float centre = XHair_Centre( 0 ); // middle of pixel 0
	const int count = XHair_Build( params, centre, centre, outline, rects, XHAIR_MAX_RECTS );

	for( int i = 0; i < count; i++ )
	{
		const xhair_rect_t *rect = &rects[i];
		const int alpha = (int)( rgba[3] * rect->alpha * 255.0f + 0.5f );

		EngFuncs::PIC_Set( hWhite, rgba[0] * 255.0f + 0.5f, rgba[1] * 255.0f + 0.5f, rgba[2] * 255.0f + 0.5f, alpha );
		if( additive )
			EngFuncs::PIC_DrawAdditive( origin.x + rect->x * zoom, origin.y + rect->y * zoom, rect->w * zoom, rect->h * zoom );
		else EngFuncs::PIC_DrawTrans( origin.x + rect->x * zoom, origin.y + rect->y * zoom, rect->w * zoom, rect->h * zoom );
	}
}
#endif

void CMenuCrosshairPreview::Draw()
{
	CMenuCrosshair *page = (CMenuCrosshair *)Parent();
	const bool enhanced = page->enable.bChecked;

	// the real size view on top, the zoomed one below
	const int gapY = 20 * uiStatic.scaleY;
	Size realSize( m_scSize.w, ( m_scSize.h - gapY ) * 2 / 5 );
	Size zoomSize( m_scSize.w, m_scSize.h - gapY - realSize.h );
	Point realPos = m_scPos;
	Point zoomPos( m_scPos.x, m_scPos.y + realSize.h + gapY );

#ifdef MAINUI_CROSSHAIR_GEOM
	xhair_settings_t settings;
	xhair_params_t params;
	float rgba[4], outlineRgba[4];
	char colorStr[64], outlineStr[64];

	settings.style = page->style.GetCurrentValue() == 2 ? XHAIR_STYLE_CIRCLE : XHAIR_STYLE_CROSS;
	settings.t = page->style.GetCurrentValue() == 1;
	settings.size = page->length.Value();
	settings.thick = page->thickness.Value();
	settings.dot = page->dot.bChecked;
	settings.dot_size = page->dotSize.Value();
	settings.pad = page->outline.Value();
	settings.aa = page->antialias.bChecked;
	settings.scale = page->scale.Value();
	XHair_SetupParams( &params, &settings, 4.0f + dynamicGap + page->gap.Value(), ScreenHeight );

	snprintf( colorStr, sizeof( colorStr ), "%g %g %g %g", page->color[0].Value(), page->color[1].Value(), page->color[2].Value(), page->color[3].Value() );
	if( page->outlineAlphaSet )
		snprintf( outlineStr, sizeof( outlineStr ), "%g %g %g %g", page->outlineColor[0].Value(), page->outlineColor[1].Value(), page->outlineColor[2].Value(), page->outlineColor[3].Value() );
	else snprintf( outlineStr, sizeof( outlineStr ), "%g %g %g", page->outlineColor[0].Value(), page->outlineColor[1].Value(), page->outlineColor[2].Value() );
	XHair_ParseColors( colorStr, outlineStr, rgba, outlineRgba );
#endif

	for( int view = 0; view < 2; view++ )
	{
		const Point pt = view ? zoomPos : realPos;
		const Size sz = view ? zoomSize : realSize;
		const int zoom = view ? XHAIR_ZOOM : 1;

		// the centre pixel in the middle of the view, as in game
		const Point origin( pt.x + sz.w / 2 - zoom / 2, pt.y + sz.h / 2 - zoom / 2 );

		UI::Scissor::PushScissor( pt, sz );
		DrawBackground( pt, sz, zoom );

		if( !enhanced )
			DrawClassic( origin, zoom );
#ifdef MAINUI_CROSSHAIR_GEOM
		else
		{
			if( params.outline > 0.0f )
				DrawLayer( &params, true, outlineRgba, false, origin, zoom );
			DrawLayer( &params, false, rgba, page->additive.bChecked, origin, zoom );
		}
#endif
		UI::Scissor::PopScissor();

		UI_DrawString( font, pt.x + 6, pt.y + 4, sz.w, m_scChSize, view ? "x4" : "1:1", uiColorHelp, m_scChSize, QM_LEFT, ETF_SHADOW | ETF_NOSIZELIMIT | ETF_FORCECOL );
		UI_DrawRectangle( pt, sz, uiInputFgColor );
	}
}

/*
=================
CMenuCrosshair
=================
*/
void CMenuCrosshair::Show()
{
	// what Cancel goes back to
	for( size_t i = 0; i < V_ARRAYSIZE( g_CrosshairCvars ); i++ )
		Q_strncpy( saved[i], EngFuncs::GetCvarString( g_CrosshairCvars[i].name ), sizeof( saved[i] ));

	CMenuFramework::Show();
}

void CMenuCrosshair::Reload()
{
	char colorStr[CS_SIZE];
	int rgb[3];
	int c[4];
	int i, n;

	loading = true;
	CMenuFramework::Reload();

	// classic
	Q_strncpy( colorStr, EngFuncs::GetCvarString( "cl_crosshair_color" ), sizeof( colorStr ));
	classicColor.SetCurrentValue( 0.0f );
	if( sscanf( colorStr, "%d %d %d", &rgb[0], &rgb[1], &rgb[2] ) == 3 )
	{
		for( i = 0; i < (int)V_ARRAYSIZE( g_CrosshairColors ); i++ )
		{
			if( g_CrosshairColors[i].r == rgb[0] && g_CrosshairColors[i].g == rgb[1] && g_CrosshairColors[i].b == rgb[2] )
				classicColor.SetCurrentValue( i );
		}
	}

	classicSize.SetCurrentValue( 0.0f );
	for( i = 0; i < (int)V_ARRAYSIZE( g_szCrosshairSizes ); i++ )
	{
		if( !stricmp( EngFuncs::GetCvarString( "cl_crosshair_size" ), g_szCrosshairSizes[i] ))
			classicSize.SetCurrentValue( i );
	}

	// enhanced
	if( EngFuncs::GetCvarFloat( "xhair_style" ) == 1.0f )
		style.SetCurrentValue( 2 );
	else style.SetCurrentValue( EngFuncs::GetCvarFloat( "xhair_t" ) ? 1 : 0 );

	length.SetValue( EngFuncs::GetCvarFloat( "xhair_size" ));
	gap.SetValue( EngFuncs::GetCvarFloat( "xhair_gap" ));
	thickness.SetValue( EngFuncs::GetCvarFloat( "xhair_thick" ));
	dotSize.SetValue( EngFuncs::GetCvarFloat( "xhair_dot_size" ));
	outline.SetValue( EngFuncs::GetCvarFloat( "xhair_pad" ));
	scale.SetValue( EngFuncs::GetCvarFloat( "xhair_scale" ));
	dynamicScale.SetValue( EngFuncs::GetCvarFloat( "xhair_dynamic_scale" ));

	c[0] = 0; c[1] = 255; c[2] = 0; c[3] = 255;
	sscanf( EngFuncs::GetCvarString( "xhair_color" ), "%d %d %d %d", &c[0], &c[1], &c[2], &c[3] );
	for( i = 0; i < 4; i++ )
		color[i].SetValue( bound( 0, c[i], 255 ));

	c[0] = c[1] = c[2] = 0; c[3] = color[3].Value();
	n = sscanf( EngFuncs::GetCvarString( "xhair_pad_color" ), "%d %d %d %d", &c[0], &c[1], &c[2], &c[3] );
	outlineAlphaSet = n == 4;
	for( i = 0; i < 4; i++ )
		outlineColor[i].SetValue( bound( 0, c[i], 255 ));

	loading = false;
	UpdateVisibility();
}

void CMenuCrosshair::Apply()
{
	char str[64];
	int i;

	if( loading )
		return;

	enable.WriteCvar();

	// classic
	i = classicSize.GetCurrentValue();
	if( i >= 0 && i < (int)V_ARRAYSIZE( g_szCrosshairSizes ))
		EngFuncs::CvarSetString( "cl_crosshair_size", g_szCrosshairSizes[i] );

	i = classicColor.GetCurrentValue();
	if( i >= 0 && i < (int)V_ARRAYSIZE( g_CrosshairColors ))
	{
		snprintf( str, sizeof( str ), "%d %d %d", g_CrosshairColors[i].r, g_CrosshairColors[i].g, g_CrosshairColors[i].b );
		EngFuncs::CvarSetString( "cl_crosshair_color", str );
	}

	classicTranslucent.WriteCvar();

	// enhanced
	i = style.GetCurrentValue();
	EngFuncs::CvarSetValue( "xhair_style", i == 2 ? 1.0f : 0.0f );
	EngFuncs::CvarSetValue( "xhair_t", i == 1 ? 1.0f : 0.0f );
	EngFuncs::CvarSetValue( "xhair_size", length.Value() );
	EngFuncs::CvarSetValue( "xhair_gap", gap.Value() );
	EngFuncs::CvarSetValue( "xhair_thick", thickness.Value() );
	EngFuncs::CvarSetValue( "xhair_dot_size", dotSize.Value() );
	EngFuncs::CvarSetValue( "xhair_pad", outline.Value() );
	EngFuncs::CvarSetValue( "xhair_scale", scale.Value() );
	EngFuncs::CvarSetValue( "xhair_dynamic_scale", dynamicScale.Value() );

	snprintf( str, sizeof( str ), "%d %d %d %d", (int)color[0].Value(), (int)color[1].Value(), (int)color[2].Value(), (int)color[3].Value() );
	EngFuncs::CvarSetString( "xhair_color", str );

	// without an alpha of its own, the outline follows the crosshair's
	if( outlineAlphaSet )
		snprintf( str, sizeof( str ), "%d %d %d %d", (int)outlineColor[0].Value(), (int)outlineColor[1].Value(), (int)outlineColor[2].Value(), (int)outlineColor[3].Value() );
	else snprintf( str, sizeof( str ), "%d %d %d", (int)outlineColor[0].Value(), (int)outlineColor[1].Value(), (int)outlineColor[2].Value() );
	EngFuncs::CvarSetString( "xhair_pad_color", str );

	dot.WriteCvar();
	antialias.WriteCvar();
	additive.WriteCvar();
	dynamicMove.WriteCvar();
	gapUseWeapon.WriteCvar();
}

void CMenuCrosshair::SetCvars( bool defaults )
{
	for( size_t i = 0; i < V_ARRAYSIZE( g_CrosshairCvars ); i++ )
		EngFuncs::CvarSetString( g_CrosshairCvars[i].name, defaults ? g_CrosshairCvars[i].def : saved[i] );

	Reload();
}

void CMenuCrosshair::Revert()
{
	SetCvars( false );
	Hide();
}

void CMenuCrosshair::UseDefaults()
{
	SetCvars( true );
}

void CMenuCrosshair::UpdateVisibility()
{
	const bool enhanced = enable.bChecked;
	CMenuBaseItem *classic[] = { &classicSize, &classicColor, &classicTranslucent };
	CMenuBaseItem *items[] = { &styleLabel, &style, &dot, &antialias, &additive, &dynamicMove, &gapUseWeapon, &fire };
	CMenuCrosshairValue *values[] = { &length, &gap, &thickness, &dotSize, &outline, &scale, &dynamicScale,
		&color[0], &color[1], &color[2], &color[3], &outlineColor[0], &outlineColor[1], &outlineColor[2], &outlineColor[3] };

	for( size_t i = 0; i < V_ARRAYSIZE( classic ); i++ )
	{
		if( enhanced ) classic[i]->Hide();
		else classic[i]->Show();
	}

	for( size_t i = 0; i < V_ARRAYSIZE( items ); i++ )
	{
		if( enhanced ) items[i]->Show();
		else items[i]->Hide();
	}

	for( size_t i = 0; i < V_ARRAYSIZE( values ); i++ )
		values[i]->SetVisible( enhanced );
}

void CMenuCrosshair::_Init()
{
	static const char *colorNames[V_ARRAYSIZE( g_CrosshairColors )];
	static CStringArrayModel colorModel( colorNames, V_ARRAYSIZE( g_CrosshairColors ));
	for( size_t i = 0; i < V_ARRAYSIZE( g_CrosshairColors ); i++ )
		colorNames[i] = L( g_CrosshairColors[i].name );

	static const char *sizeNames[] = { L( "GameUI_Auto" ), L( "GameUI_Small" ), L( "GameUI_Medium" ), L( "GameUI_Large" ) };
	static CStringArrayModel sizeModel( sizeNames, V_ARRAYSIZE( sizeNames ));

	static const char *styleNames[] = { L( "Cross" ), L( "T" ), L( "Circle" ) };
	static CStringArrayModel styleModel( styleNames, V_ARRAYSIZE( styleNames ));

	static const char *backgroundNames[] = { L( "Map" ), L( "Dark" ), L( "Light" ) };
	static CStringArrayModel backgroundModel( backgroundNames, V_ARRAYSIZE( backgroundNames ));

	static const char *channels[] = { L( "Red" ), L( "Green" ), L( "Blue" ), L( "Alpha" ) };
	static const char *outlineChannels[] = { L( "Outline red" ), L( "Outline green" ), L( "Outline blue" ), L( "Outline alpha" ) };

	const int colA = 72, colB = 440, top = 320;
	int y;

	banner.SetPicture( ART_BANNER );
	AddItem( banner );

	// dark cards under the controls and the preview, for contrast with the artwork
	panel.iFlags = QMF_INACTIVE;
	panel.SetBackground( 0xC0101010 );
	panel.SetRect( colA - 12, 205, colB + 340 + 12 - ( colA - 12 ), 525 );
	AddItem( panel );

	previewPanel.iFlags = QMF_INACTIVE;
	previewPanel.SetBackground( 0xC0101010 );
	previewPanel.SetRect( 788, 205, 224, 525 );
	AddItem( previewPanel );

	enable.SetNameAndStatus( L( "Enhanced crosshair" ), L( "Enables enhanced crosshair" ) );
	enable.LinkCvar( "xhair_enable" );
	enable.SetCoord( colA, 215 );
	SET_EVENT_MULTI( enable.onChanged,
	{
		CMenuCrosshair *page = (CMenuCrosshair *)pSelf->Parent();
		page->Apply();
		page->UpdateVisibility();
	});
	AddItem( enable );

	done.szName = L( "GameUI_OK" );
	done.SetPicture( PC_DONE );
	done.SetCoord( colA, 262 );
	done.onReleased = VoidCb( &CMenuCrosshair::SaveAndPopMenu );
	AddItem( done );

	cancel.szName = L( "GameUI_Cancel" );
	cancel.SetPicture( PC_CANCEL );
	cancel.SetCoord( colA + 150, 262 );
	cancel.onReleased = VoidCb( &CMenuCrosshair::Revert );
	AddItem( cancel );

	defaults.szName = L( "GameUI_UseDefaults" );
	defaults.SetPicture( PC_USE_DEFAULTS );
	defaults.SetCoord( colA + 300, 262 );
	defaults.onReleased = VoidCb( &CMenuCrosshair::UseDefaults );
	AddItem( defaults );

	// classic crosshair, in place of the first column
	y = top;
	classicSize.Setup( &sizeModel );
	classicSize.SetRect( colA, y, 260, 32 );
	classicSize.onChanged = VoidCb( &CMenuCrosshair::Apply );
	AddItem( classicSize );
	y += 50;

	classicColor.Setup( &colorModel );
	classicColor.SetRect( colA, y, 260, 32 );
	classicColor.onChanged = VoidCb( &CMenuCrosshair::Apply );
	AddItem( classicColor );
	y += 50;

	classicTranslucent.SetNameAndStatus( L( "GameUI_Translucent" ), NULL );
	classicTranslucent.LinkCvar( "cl_crosshair_translucent" );
	classicTranslucent.SetCoord( colA, y );
	classicTranslucent.onChanged = VoidCb( &CMenuCrosshair::Apply );
	AddItem( classicTranslucent );

	// first column: shape
	y = top;
	styleLabel.szName = L( "Style" );
	styleLabel.iFlags = QMF_INACTIVE | QMF_DROPSHADOW;
	styleLabel.colorBase = uiColorHelp;
	styleLabel.SetCharSize( QM_SMALLFONT );
	styleLabel.SetRect( colA, y, 150, 26 );
	AddItem( styleLabel );

	style.Setup( &styleModel );
	style.SetRect( colA + 150, y, 190, 26 );
	style.SetCharSize( QM_SMALLFONT );
	style.onChanged = VoidCb( &CMenuCrosshair::Apply );
	AddItem( style );
	y += XHAIR_ROW_STEP;

	length.Setup( this, L( "Length" ), 0.0f, 20.0f, 0.25f, 2 );
	length.SetCoord( colA, y ); y += XHAIR_ROW_STEP;
	gap.Setup( this, L( "Gap" ), -5.0f, 15.0f, 0.25f, 2 );
	gap.SetCoord( colA, y ); y += XHAIR_ROW_STEP;
	thickness.Setup( this, L( "Thickness" ), 0.0f, 6.0f, 0.05f, 2 );
	thickness.SetCoord( colA, y ); y += XHAIR_ROW_STEP;
	outline.Setup( this, L( "Outline" ), 0.0f, 4.0f, 0.25f, 2 );
	outline.SetCoord( colA, y ); y += XHAIR_ROW_STEP;

	dot.SetNameAndStatus( L( "Dot" ), NULL );
	dot.LinkCvar( "xhair_dot" );
	dot.SetRect( colA, y, 26, 26 );
	dot.SetCharSize( QM_SMALLFONT );
	dot.onChanged = VoidCb( &CMenuCrosshair::Apply );
	AddItem( dot );
	y += XHAIR_ROW_STEP;

	dotSize.Setup( this, L( "Dot size" ), 0.0f, 6.0f, 0.05f, 2 );
	dotSize.SetCoord( colA, y ); y += XHAIR_ROW_STEP;
	scale.Setup( this, L( "Scale" ), 0.25f, 4.0f, 0.05f, 2 );
	scale.SetCoord( colA, y ); y += XHAIR_ROW_STEP;

	antialias.SetNameAndStatus( L( "Smooth edges" ), NULL );
	antialias.LinkCvar( "xhair_aa" );
	antialias.SetRect( colA, y, 26, 26 );
	antialias.SetCharSize( QM_SMALLFONT );
	antialias.onChanged = VoidCb( &CMenuCrosshair::Apply );
	AddItem( antialias );
	y += XHAIR_ROW_STEP;

	additive.SetNameAndStatus( L( "Additive" ), NULL );
	additive.LinkCvar( "xhair_additive" );
	additive.SetRect( colA, y, 26, 26 );
	additive.SetCharSize( QM_SMALLFONT );
	additive.onChanged = VoidCb( &CMenuCrosshair::Apply );
	AddItem( additive );

	// second column: colours and dynamic gap
	y = top;
	for( int i = 0; i < 4; i++ )
	{
		color[i].Setup( this, channels[i], 0.0f, 255.0f, 1.0f, 0 );
		color[i].SetCoord( colB, y );
		y += XHAIR_ROW_STEP;
	}

	for( int i = 0; i < 4; i++ )
	{
		outlineColor[i].Setup( this, outlineChannels[i], 0.0f, 255.0f, 1.0f, 0 );
		outlineColor[i].SetCoord( colB, y );
		y += XHAIR_ROW_STEP;
	}

	// touching the outline alpha gives the outline an alpha of its own
	SET_EVENT_MULTI( outlineColor[3].slider.onPressed,
	{
		((CMenuCrosshair *)pSelf->Parent())->outlineAlphaSet = true;
	});
	SET_EVENT_MULTI( outlineColor[3].field.onGotFocus,
	{
		((CMenuCrosshair *)pSelf->Parent())->outlineAlphaSet = true;
	});

	dynamicMove.SetNameAndStatus( L( "Dynamic" ), NULL );
	dynamicMove.LinkCvar( "xhair_dynamic_move" );
	dynamicMove.SetRect( colB, y, 26, 26 );
	dynamicMove.SetCharSize( QM_SMALLFONT );
	dynamicMove.onChanged = VoidCb( &CMenuCrosshair::Apply );
	AddItem( dynamicMove );

	gapUseWeapon.SetNameAndStatus( L( "Weapon gap" ), NULL );
	gapUseWeapon.LinkCvar( "xhair_gap_useweaponvalue" );
	gapUseWeapon.SetRect( colB + 170, y, 26, 26 );
	gapUseWeapon.SetCharSize( QM_SMALLFONT );
	gapUseWeapon.onChanged = VoidCb( &CMenuCrosshair::Apply );
	AddItem( gapUseWeapon );
	y += XHAIR_ROW_STEP;

	dynamicScale.Setup( this, L( "Dynamic scale" ), 0.0f, 3.0f, 0.1f, 2 );
	dynamicScale.SetCoord( colB, y );

	// preview
	preview.SetRect( 800, 215, 200, 420 );
	preview.SetCharSize( QM_SMALLFONT );
	preview.hMap = EngFuncs::PIC_Load( "gfx/vgui/crosshair.tga", 0 );
	preview.hWhite = EngFuncs::PIC_Load( "*white" );
	AddItem( preview );

	background.Setup( &backgroundModel );
	background.SetRect( 800, 650, 200, 26 );
	background.SetCharSize( QM_SMALLFONT );
	background.LinkCvar( "ui_xhair_background", CMenuEditable::CVAR_VALUE );
	background.onChanged = CMenuEditable::WriteCvarCb;
	AddItem( background );

	fire.SetNameAndStatus( L( "Fire" ), L( "Shoots to show the dynamic gap" ) );
	fire.SetRect( 800, 690, 26, 26 );
	fire.SetCharSize( QM_SMALLFONT );
	fire.bChecked = false;
	AddItem( fire );
}

ADD_MENU( menu_crosshair, CMenuCrosshair, UI_Crosshair_Menu );
