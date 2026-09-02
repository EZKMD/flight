#ifndef AIRPORT_BOARD_RENDERER_H
#define AIRPORT_BOARD_RENDERER_H

#include "airport_board_state.h"
#include "animation.h"
#include "frame.h"
#include "layout.h"

size_t airport_board_visible_capacity(const Layout *layout);
void airport_board_render(Frame *frame, AirportBoardState *board,
                          const AnimationState *animation, const Layout *layout);

#endif
