#ifndef NGUI
#define NGUI

#include <SDL.h>

struct SDL_Renderer *ngui_renderer;

#include "ngui_info_prompt.h"
#include "ngui_textlabel.h"
#include "ngui_flowbox.h"
#include "ngui_button.h"

void ngui_set_renderer(struct SDL_Renderer *s,void *redraw_callback);
void ngui_receive_event(SDL_Event *event);
void ngui_render();

#endif
