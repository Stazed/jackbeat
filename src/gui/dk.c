/*
 *   Jackbeat - JACK sequencer
 *    
 *   Copyright (c) 2004-2008 Olivier Guilyardi <olivier {at} samalyse {dot} com>
 *    
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *   SVN:$Id: grid.c 191 2008-06-30 19:23:58Z olivier $
 */

#include <stdlib.h>
#include <math.h>
#include "util.h"
#include "gui/dk.h"

GdkGC *
dk_make_gc (GdkDrawable *drawable, dk_color_t *color)
{
  GdkGC *gc = gdk_gc_new (drawable);
  GdkColor _color;
  GdkColormap *colormap = gdk_colormap_get_system ();
  gdk_gc_set_colormap (gc, colormap);
  _color.red = color->red << 8;
  _color.green = color->green << 8;
  _color.blue = color->blue << 8;
  gdk_gc_set_rgb_fg_color (gc, &_color);
  return gc;
}

void    
dk_draw_line (GdkDrawable *drawable, dk_color_t *color, int x1, int y1, int x2, int y2)
{
  GdkGC *gc = dk_make_gc (drawable, color);
  gdk_draw_line (drawable, gc, x1, y1, x2, y2);
  g_object_unref (G_OBJECT (gc));

}
void
dk_make_gradient (GdkGC *colors[], GdkDrawable *drawable,  
                  dk_color_t *from, dk_color_t *to, int steps)
{
  int i;
  dk_color_t color;
  for (i = 0; i < steps; i++)
  {
    color.red = i * (to->red - from->red) / (steps - 1) + from->red;
    color.green = i * (to->green - from->green) / (steps - 1) + from->green;
    color.blue = i * (to->blue - from->blue) / (steps - 1) + from->blue;
    colors[i] = dk_make_gc (drawable, &color);
  }      
  
}

void    
dk_draw_hgradient (GdkDrawable *drawable, GtkAllocation *alloc, 
                   dk_color_t *from, dk_color_t *to)
{
  GdkGC **colors = malloc (alloc->width * sizeof (GdkGC *));
  dk_make_gradient (colors, drawable, from, to, alloc->width);
  int i;
  for (i = 0; i < alloc->width; i++)
  {
    gdk_draw_line (drawable, colors[i], alloc->x + i, alloc->y, 
                   alloc->x + i, alloc->y + alloc->height - 1);
    g_object_unref (G_OBJECT (colors[i]));
  }
  free (colors);
}

void    
dk_draw_vgradient (GdkDrawable *drawable, GtkAllocation *alloc, 
                   dk_color_t *from, dk_color_t *to)
{
  GdkGC **colors = malloc (alloc->height * sizeof (GdkGC *));
  dk_make_gradient (colors, drawable, from, to, alloc->height);
  int i;
  for (i = 0; i < alloc->height; i++)
  {
    gdk_draw_line (drawable, colors[i], alloc->x, alloc->y + i, 
                   alloc->x + alloc->width - 1, alloc->y + i);
    g_object_unref (G_OBJECT (colors[i]));
  }
  free (colors);
}

int 
dk_em (GtkWidget *widget, float em_size)
{
  PangoLayout *layout = gtk_widget_create_pango_layout (widget, "A");
  int em;
  pango_layout_get_pixel_size (layout, NULL, &em);
  g_object_unref (layout);
  return util_round (em_size * (float) em);
}

void
dk_draw_track_bg (GdkDrawable *drawable, GtkAllocation *alloc, int active, dk_hsv_t *hsv_shift,
                  dk_color_t *hborder)
{
    dk_color_t gradient_bottom, gradient_top, bottom, top, top2, border1, border2, border3;
    DK_COLOR_SET (border1, 0xf3, 0xf3, 0xf3);
    DK_COLOR_SET (border2, 0xe7, 0xe7, 0xe7);
    DK_COLOR_SET (border3, 0x79, 0x79, 0x79);

    if (active)
    {
      DK_COLOR_SET (gradient_bottom, 0x74, 0x8c, 0xac);
      dk_transform_hsv (&gradient_bottom, hsv_shift);
      DK_COLOR_SET (gradient_top, 0x8d, 0xa1, 0xbc);
      dk_transform_hsv (&gradient_top, hsv_shift);
      DK_COLOR_SET (top, 0xc2, 0xcc, 0xda);
      dk_transform_hsv (&top, hsv_shift);
      DK_COLOR_SET (top2, 0x95, 0xa7, 0xc1);
      dk_transform_hsv (&top2, hsv_shift);
      DK_COLOR_SET (bottom, 0x40, 0x51, 0x66);
      dk_transform_hsv (&bottom, hsv_shift);
    }
    else
    {
      DK_COLOR_SET (gradient_bottom, 0x77, 0x77, 0x77);
      DK_COLOR_SET (gradient_top, 0x91, 0x91, 0x91);
      DK_COLOR_SET (top, 0xc4, 0xc5, 0xc4);
      DK_COLOR_SET (top2, 0x99, 0x99, 0x99);
      DK_COLOR_SET (bottom, 0x46, 0x46, 0x46);
    }

    int x = alloc->x;
    int y = alloc->y;
    int width = alloc->width;
    int height = alloc->height;
    dk_draw_line (drawable, &top, x, y, x + width - 1, y);
    dk_draw_line (drawable, &top2, x, y + 1, x + width - 1, y + 1);
    GtkAllocation _alloc = {x, y + 2, width, height - 6};
    dk_draw_vgradient (drawable, &_alloc, &gradient_top, &gradient_bottom);
    dk_draw_line (drawable, &bottom, x, y + height - 4, x + width - 1, y + height - 4);
    dk_draw_line (drawable, &border1, x, y + height - 3, x + width - 1, y + height - 3);
    dk_draw_line (drawable, &border2, x, y + height - 2, x + width - 1, y + height - 2);
    dk_draw_line (drawable, &border3, x, y + height - 1, x + width - 1, y + height - 1);

    if (hborder)
    {
      dk_draw_line (drawable, hborder, x, y + 1, x, y + height - 4);
      dk_draw_line (drawable, hborder, x + width - 1, y + 1, x + width - 1, y + height - 4);
    }
}

// ######################################################################
// T. Nathan Mundhenk
// mundhenk@usc.edu
// C/C++ function RGB to HSV
// see: http://ilab.usc.edu/wiki/index.php/HSV_And_H2SV_Color_Space
static void 
pixRGBtoHSVCommon(const double R, const double G, const double B, 
                  double *H, double *S, double *V)
{
  if((B > G) && (B > R))
  {
    (*V) = B;
    if((*V) != 0)
    {
      double min;
      if(R > G) min = G;
      else      min = R;
      const double delta = (*V) - min;
      if(delta != 0)
        { (*S) = (delta/(*V)); (*H) = 4 + (R - G) / delta; }
      else
        { (*S) = 0;         (*H) = 4 + (R - G); }
      (*H) *=   60; if((*H) < 0) (*H) += 360;
      (*V)  = (*V) * 100 / 255;
      (*S) *= (100);
    }
    else
      { (*S) = 0; (*H) = 0;}
  }
  else if(G > R)
  {
    (*V) = G;
    if((*V) != 0)
    {
      double min;
      if(R > B) min = B;
      else      min = R;
      const double delta = (*V) - min;
      if(delta != 0)
        { (*S) = (delta/(*V)); (*H) = 2 + (B - R) / delta; }
      else
        { (*S) = 0;         (*H) = 2 + (B - R); }
      (*H) *=   60; if((*H) < 0) (*H) += 360;
      (*V)  = (*V) * 100 / 255;
      (*S) *= (100);
    }
    else
      { (*S) = 0; (*H) = 0;}
  }
  else
  {
    (*V) = R;
    if((*V) != 0)
    {
      double min;
      if(G > B) min = B;
      else      min = G;
      const double delta = (*V) - min;
      if(delta != 0)
        { (*S) = (delta/(*V)); (*H) = (G - B) / delta; }
      else
        { (*S) = 0;         (*H) = (G - B); }
      (*H) *=   60; if((*H) < 0) (*H) += 360;
      (*V)  = (*V) * 100 / 255;
      (*S) *= (100);
    }
    else
      { (*S) = 0; (*H) = 0;}
  }
}

// ######################################################################
// T. Nathan Mundhenk
// mundhenk@usc.edu
// C/C++ Macro HSV to RGB
// see: http://ilab.usc.edu/wiki/index.php/HSV_And_H2SV_Color_Space
static void 
pixHSVtoRGBCommon(const double H, const double _S, const double _V, 
                  double *R, double *G, double *B)
{ 
  double S = _S / 100;
  double V = _V / 100;
  if( V == 0 )
  { (*R) = 0; (*G) = 0; (*B) = 0; }
  else if( S == 0 )
  {
    (*R) = V;
    (*G) = V;
    (*B) = V;
  }
  else
  {
    const double hf = H / 60.0;
    const int    i  = (int) floor( hf );
    const double f  = hf - i;
    const double pv  = V * ( 1 - S );
    const double qv  = V * ( 1 - S * f );
    const double tv  = V * ( 1 - S * ( 1 - f ) );
    switch( i )
      {
      case 0:
        (*R) = V;
        (*G) = tv;
        (*B) = pv;
        break;
      case 1:
        (*R) = qv;
        (*G) = V;
        (*B) = pv;
        break;
      case 2:
        (*R) = pv;
        (*G) = V;
        (*B) = tv;
        break;
      case 3:
        (*R) = pv;
        (*G) = qv;
        (*B) = V;
        break;
      case 4:
        (*R) = tv;
        (*G) = pv;
        (*B) = V;
        break;
      case 5:
        (*R) = V;
        (*G) = pv;
        (*B) = qv;
        break;
      case 6:
        (*R) = V;
        (*G) = tv;
        (*B) = pv;
        break;
      case -1:
        (*R) = V;
        (*G) = pv;
        (*B) = qv;
        break;
      default:
        //LFATAL(&quot;i Value error in Pixel conversion, Value is %d&quot;,i);
        break;
      }
  }
  (*R) *= 255.0F;
  (*G) *= 255.0F;
  (*B) *= 255.0F;
}

void    
dk_transform_hsv (dk_color_t *color, dk_hsv_t *hsv_shift) {
    if (hsv_shift) {
        double h, s, v, r = 0, g = 0, b = 0;
        pixRGBtoHSVCommon(color->red, color->green, color->blue, &h, &s, &v);
        h += hsv_shift->hue;
        s += hsv_shift->saturation;
        v += hsv_shift->value;
        pixHSVtoRGBCommon(h, s, v, &r, &g, &b);
        color->red   = r;
        color->green = g;
        color->blue  = b;
    }
}
