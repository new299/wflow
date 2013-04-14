#include <string.h>
#include <SDL.h>
#include "nsdl.h"
#include "nunifont.h"
#include "ngui.h"
#include <stdbool.h>

typedef struct {
  bool valid;
  int  x;
  int  y;
  bool passwordbox;
  int  x_padding;
  int  y_padding;
  int  width;
  bool selected;
  char text[100];
  void (*callback)(char *);
} ngui_textbox_data;

int ngui_textboxs_size = 0;
ngui_textbox_data ngui_textboxs[50];

void ngui_receive_event_textbox(SDL_Event *event, ngui_textbox_data *d) {
  if(event->type == SDL_MOUSEBUTTONDOWN) {

    int x = event->button.x;
    int y = event->button.y;
    if((x > (d->x-d->x_padding)) && (x < ((d->x)+(d->width*8)+d->x_padding)) &&
       (y > (d->y-d->y_padding)) && (y < ((d->y)+16+d->y_padding))) {
      d->selected = true;
      if(d->callback != NULL) d->callback("redraw");
    } else {
      d->selected = false;
      if(d->callback != NULL) d->callback("redraw");
    }
  }

  // key press processing
  
  if(d->selected) {
    if(event->type == SDL_KEYDOWN) {

      if(event->key.keysym.sym == SDLK_BACKSPACE) {
        int p = strlen(d->text);
        if(p > 0) d->text[p-1]=0;
        if(d->callback != NULL) d->callback("redraw");
      } else {
        // normal key processing
        int p;
        for(p=0;p<100;p++) {
          if(d->text[p] == 0) break;
        }

        d->text[p]   = event->key.keysym.unicode;
        d->text[p+1] = 0;
        if(d->callback != NULL) d->callback("redraw");
      }
    }
  }


}

void ngui_render_textbox(ngui_textbox_data *d) {

  uint16_t text[100];
  for(int n=0;n<100;n++) text[n] = d->text[n];

  printf("text is %s\n",d->text);
  if(d->selected) {
//    nsdl_rectangle_shade(ngui_screen,d->x-d->x_padding,d->y-d->y_padding,d->x+(strlen(d->text))*8+d->x_padding,d->y+16+d->y_padding,1000,10000);
  }

  if(d->passwordbox == false) {
    draw_unitext_renderer(ngui_renderer,
                 d->x,
                 d->y,
                 text,
                 0,
                 65535,0,0,0,0);
  } else {
    for(int n=0;n<100;n++) {
      if(text[n] != 0) text[n] = '*';
    }

    draw_unitext_renderer(ngui_renderer,
                 d->x,
                 d->y,
                 text,
                 0,
                 65535,0,0,0,0);
  }
}

int ngui_delete_textbox(int id) {
  ngui_textboxs[id].valid = false;
}

int ngui_add_textbox(int x,int y,char *text,bool passwordbox,void *callback) {

  ngui_textboxs[ngui_textboxs_size].valid = true;
  ngui_textboxs[ngui_textboxs_size].x = x;
  ngui_textboxs[ngui_textboxs_size].y = y;
  ngui_textboxs[ngui_textboxs_size].passwordbox = passwordbox;
  ngui_textboxs[ngui_textboxs_size].x_padding = 10;
  ngui_textboxs[ngui_textboxs_size].y_padding = 10;
  ngui_textboxs[ngui_textboxs_size].width     = 8*20;
  strcpy(ngui_textboxs[ngui_textboxs_size].text,text);
  ngui_textboxs[ngui_textboxs_size].callback = callback;

  ngui_textboxs_size++;
  return ngui_textboxs_size-1;
}

void ngui_receiveall_textbox(SDL_Event *event) {
  for(int n=0;n<ngui_textboxs_size;n++) {
    ngui_textbox_data *d = &ngui_textboxs[n];
    if(d->valid == true) {
      ngui_receive_event_textbox(event,d);
    }
  }
}

void ngui_renderall_textbox() {
  for(int n=0;n<ngui_textboxs_size;n++) {
    ngui_textbox_data *d = &ngui_textboxs[n];
    if(d->valid == true) {
      ngui_render_textbox(d);
    }
  }
}

char *ngui_textbox_get_value(int id) {

  return ngui_textboxs[id].text;

}
