// -*- Mode: C++; indent-tabs-mode: nil; tab-width: 2 -*-
/*
* Copyright (C) 2013 Canonical Ltd
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 3 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
* Authored by: Marco Trevisan <marco.trevisan@canonical.com>
*/

#include "CompizUtils.h"
#include <cairo-xlib.h>
#include <cairo-xlib-xrender.h>
#include <core/screen.h>

namespace unity
{
namespace compiz_utils
{
namespace
{
  const unsigned PIXMAP_DEPTH  = 32;
  const float    DEFAULT_SCALE = 1.0f;
}

SimpleTexture::SimpleTexture(GLTexture::List const& tex)
  : texture_(tex)
{}

//

SimpleTextureQuad::SimpleTextureQuad()
  : scale(DEFAULT_SCALE)
{}

bool SimpleTextureQuad::SetTexture(SimpleTexture::Ptr const& simple_texture)
{
  if (st == simple_texture)
    return false;

  st = simple_texture;

  if (st && st->texture())
  {
    auto* tex = st->texture();
    CompPoint old_coords(quad.box.x(), quad.box.y());
    short invalid = std::numeric_limits<short>::min();
    quad.box.setGeometry(invalid, invalid, tex->width() * scale, tex->height() * scale);
    SetCoords(old_coords.x(), old_coords.y());
  }

  return true;
}

bool SimpleTextureQuad::SetScale(float s)
{
  if (!st || scale == s)
    return false;

  scale = s;
  auto* tex = st->texture();
  quad.box.setWidth(tex->width() * scale);
  quad.box.setHeight(tex->height() * scale);
  UpdateMatrix();
  return true;
}

bool SimpleTextureQuad::SetCoords(int x, int y)
{
  if (x == quad.box.x() && y == quad.box.y())
    return false;

  quad.box.setX(x);
  quad.box.setY(y);
  UpdateMatrix();
  return true;
}

void SimpleTextureQuad::UpdateMatrix()
{
  int x = quad.box.x();
  int y = quad.box.y();

  quad.matrix = (st && st->texture()) ? st->texture()->matrix() : GLTexture::Matrix();
  quad.matrix.xx /= scale;
  quad.matrix.yy /= scale;
  quad.matrix.x0 = 0.0f - COMP_TEX_COORD_X(quad.matrix, x);
  quad.matrix.y0 = 0.0f - COMP_TEX_COORD_Y(quad.matrix, y);
}

bool SimpleTextureQuad::SetX(int x)
{
  return SetCoords(x, quad.box.y());
}

bool SimpleTextureQuad::SetY(int y)
{
  return SetCoords(quad.box.x(), y);
}

//

PixmapTexture::PixmapTexture(int w, int h)
 : pixmap_(0)
{
  if (w > 0 && h > 0)
  {
    pixmap_ = XCreatePixmap(screen->dpy(), screen->root(), w, h, PIXMAP_DEPTH);
    texture_ = GLTexture::bindPixmapToTexture(pixmap_, w, h, PIXMAP_DEPTH);
  }
}

PixmapTexture::~PixmapTexture()
{
  texture_.clear();

  if (pixmap_)
    XFreePixmap(screen->dpy(), pixmap_);
}

//

CairoContext::CairoContext(int w, int h, double scale)
  : pixmap_texture_(std::make_shared<PixmapTexture>(w, h))
  , surface_(nullptr)
  , cr_(nullptr)
{
  Screen *xscreen = ScreenOfDisplay(screen->dpy(), screen->screenNum());
  XRenderPictFormat* format = XRenderFindStandardFormat(screen->dpy(), PictStandardARGB32);
  surface_ = cairo_xlib_surface_create_with_xrender_format(screen->dpy(), pixmap_texture_->pixmap(),
                                                           xscreen, format, w, h);
  cairo_surface_set_device_scale(surface_, scale, scale);

  cr_ = cairo_create(surface_);
  cairo_save(cr_);
  cairo_set_operator(cr_, CAIRO_OPERATOR_CLEAR);
  cairo_paint(cr_);
  cairo_restore(cr_);
}

CairoContext::~CairoContext()
{
  if (cr_)
    cairo_destroy(cr_);

  if (surface_)
    cairo_surface_destroy(surface_);
}

int CairoContext::width() const
{
  return cairo_xlib_surface_get_width(surface_);
}

int CairoContext::height() const
{
  return cairo_xlib_surface_get_height(surface_);
}

bool IsWindowShadowDecorable(CompWindow* win)
{
  if (!win)
    return false;

  if (!win->isViewable())
    return false;

  if (win->wmType() & (CompWindowTypeDockMask | CompWindowTypeDesktopMask))
    return false;

  if (win->region().numRects() != 1) // Non rectangular windows
    return false;

  if (win->alpha())
    return WindowHasMotifDecorations(win);

  return true;
}

bool IsWindowFullyDecorable(CompWindow* win)
{
  if (!win)
    return false;

  if (!IsWindowShadowDecorable(win))
    return false;

  return WindowHasMotifDecorations(win);
}

bool WindowHasMotifDecorations(CompWindow* win)
{
  if (!win)
    return false;

  if (win->overrideRedirect())
    return false;

  switch (win->type())
  {
    case CompWindowTypeDialogMask:
    case CompWindowTypeModalDialogMask:
    case CompWindowTypeUtilMask:
    case CompWindowTypeMenuMask:
    case CompWindowTypeNormalMask:
      if (win->mwmDecor() & (MwmDecorAll | MwmDecorTitle))
        return true;
  }

  return false;
}

} // compiz_utils namespace
} // unity namespace

std::ostream& operator<<(std::ostream &os, CompRect const& r)
{
  return os << "CompRect: coords = " << r.x() << "x" << r.y() << ", size = " << r.width() << "x" << r.height();
}
