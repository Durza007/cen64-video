/* ============================================================================
 *  Controller.c: Video rendering.
 *
 *  VIDEOSIM: VIDEO Interface SIMulator.
 *  Copyright (C) 2013, Tyler J. Stachecki.
 *  All rights reserved.
 *
 *  This file is subject to the terms and conditions defined in
 *  file 'LICENSE', which is part of this source code package.
 * ========================================================================= */
#include "Controller.h"
#include "Externs.h"
#include "Render.h"

#ifdef __cplusplus
#include <cassert>
#include <cstring>
#else
#include <assert.h>
#include <string.h>
#endif

#include <GL/glfw.h>

/* Hack to get access to this */
#define GL_UNSIGNED_SHORT_5_5_5_1         0x8034

static void RenderFrame16(struct VIFController *controller);

/* ============================================================================
 *  RenderFrame: Draws a frame to the display.
 * ========================================================================= */
void
RenderFrame(struct VIFController *controller) {
  glClear(GL_COLOR_BUFFER_BIT);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  debug("== Rastering Frame ==");
  debugarg("Height:  [%u].", controller->renderArea.height);
  debugarg("Width:   [%u].", controller->renderArea.width);
  debugarg("X Start: [%u].", controller->renderArea.x.start);
  debugarg("X End:   [%u].", controller->renderArea.x.end);
  debugarg("Y Start: [%u].", controller->renderArea.y.start);
  debugarg("Y End:   [%u].", controller->renderArea.y.end);

  switch(controller->regs[VI_STATUS_REG] & 0x3) {
  case 0:
    break;

  case 1:
    assert(0 && "Attempted to use reversed frame type.");
    break;

  case 2:
    RenderFrame16(controller);
    break;

  case 3:
    assert(0 && "32-bit frame.");
    break;
  }

  glfwPollEvents();
  glfwSwapBuffers();
}

/* ============================================================================
 *  Renders a 16-bit frame.
 * ========================================================================= */
static void
RenderFrame16(struct VIFController *controller) {
  uint32_t offset = controller->regs[VI_ORIGIN_REG] & 0xFFFFFF;
  const uint8_t *buffer = BusGetRDRAMPointer(controller->bus) + offset;

  int hdiff = controller->renderArea.x.end - controller->renderArea.x.start;
  int vdiff = controller->renderArea.y.end - controller->renderArea.y.start;
  float hcoeff = (float)(controller->regs[VI_X_SCALE_REG] & 0xFFF) / (1 << 10);
  float vcoeff = (float)(controller->regs[VI_Y_SCALE_REG] & 0xFFF) / (1 << 10);
  unsigned hres = (hdiff * hcoeff);
  unsigned vres = (vdiff * vcoeff);

  int hskip = controller->regs[VI_WIDTH_REG] - hres;

  debugarg("H Res:   [%u].", hres);
  debugarg("V Res:   [%u].", vres);
  debugarg("H Skip:  [%d].", hskip);

  if (vdiff <= 0 || hdiff <= 0 || !offset)
    return;

  /* Hacky? */
  if (hres > 640) {
    hskip += (hres - 640);
    hres = 640;
  }

  if (vres > 480) {
    vres = 480;
  }

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
    controller->renderArea.width + hskip,
    controller->renderArea.height,
    0, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, buffer);

  glBegin(GL_QUADS);
    glTexCoord2f(0, 0);
    glVertex2i(0, 0);
    glTexCoord2f((controller->renderArea.width - hskip) / controller->renderArea.width, 0);
    glVertex2i(controller->renderArea.width - hskip, 0);
    glTexCoord2f((controller->renderArea.width - hskip) / controller->renderArea.width, 1);
    glVertex2i(controller->renderArea.width - hskip, controller->renderArea.height);
    glTexCoord2f(0, 1);
    glVertex2i(0, controller->renderArea.height);
  glEnd();
}

/* ============================================================================
 *  ResetPerspective: Calculates the coordinates of the visible area.
 * ========================================================================= */
void
ResetPerspective(struct VIFController *controller) {
  struct RenderArea *renderArea = &controller->renderArea;

  /* Calculate the bounding positions. */
  renderArea->x.start = controller->regs[VI_H_START_REG] >> 16 & 0x3FF;
  renderArea->x.end = controller->regs[VI_H_START_REG] & 0x3FF;
  renderArea->y.start = controller->regs[VI_V_START_REG] >> 16 & 0x3FF;
  renderArea->y.end = controller->regs[VI_V_START_REG] & 0x3FF;

  renderArea->y.end /= 2;
  renderArea->y.start /= 2;

  /* Calculate the height and width. */
  renderArea->height = ((controller->regs[VI_Y_SCALE_REG] & 0xFFF) *
    (renderArea->y.end - renderArea->y.start)) / 0x400;

  renderArea->width = ((controller->regs[VI_X_SCALE_REG] & 0xFFF) *
    (renderArea->x.end - renderArea->x.start)) / 0x400;

  /* Catch impossible conditions. */
  renderArea->width = (renderArea->width > 640) ? 640 : renderArea->width;
  renderArea->height = (renderArea->height > 480) ? 480 : renderArea->height;

  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, renderArea->width, renderArea->height, 0, 0, 1);
}

