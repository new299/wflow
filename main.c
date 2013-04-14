//#define _POSIX_C_SOURCE 199309L
//#define _BSD_SOURCE

#include "fontmap_static.h"
#include "widthmap_static.h"

#include "nunifont.h"
#include <string.h>
#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include "vterm.h"
#include <locale.h>

#include <limits.h>

#include "nsdl.h"
#include <time.h>
#include <stdbool.h>
#include "local.h"
#include "ngui.h"
#include "utf8proc.h"

#ifdef IOS_BUILD
#include "iphone_pasteboard.h"
#endif

#ifdef OSX_BUILD
#include "osx_pasteboard.h"
#endif

void redraw_required();

int font_width  = 8;
int font_height = 16;
int font_space  = 0;

bool any_blinking       = false;
bool new_screen_size    = false;
int  new_screen_size_x;
int  new_screen_size_y;

static int cols;
static int rows;

int display_width;
int display_height;
int display_width_last_kb=0;
int display_height_last_kb=0;
int display_width_abs;
int display_height_abs;

struct SDL_Window  *screen=1;
struct SDL_Renderer *renderer=1;

bool draw_selection = false;
int draw_fade_selection=0;
int select_start_x=0;
int select_start_y=0;
int select_end_x  =0;
int select_end_y  =0;
int select_start_scroll_offset;
int select_end_scroll_offset;
int connection_type=0;

bool hterm_quit = false;


void redraw_screen() {
  SDL_SetRenderDrawColor(renderer,0x00,0x00,0x00,0xff);
  SDL_RenderClear(renderer);
    
  ngui_render();

  SDL_RenderPresent(renderer);
}

void do_sdl_init() {
    if(SDL_Init(SDL_INIT_VIDEO)<0) {
        return;
    }
    
    #if defined(OSX_BUILD) || defined(IOS_BUILD)
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE  , 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE , 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 32);

    #ifdef OSX_BUILD
//      SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 0 );
    #endif
    
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    #endif
    #ifdef IOS_BUILD
    screen=SDL_CreateWindow(NULL, 0, 0, 0, 0,SDL_WINDOW_FULLSCREEN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN | SDL_WINDOW_BORDERLESS);
    #endif
    #if defined(OSX_BUILD) || defined(LINUX_BUILD)
    screen=SDL_CreateWindow("hterm", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 800, 600,SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    #endif
 
    #ifdef IOS_BUILD
      ios_connect();
    #endif
    
    #ifdef IOS_BUILD
    SDL_GetWindowSize(screen,&display_width,&display_height);
    display_width_abs  = display_width;
    display_height_abs = display_height;
    #endif

    if (screen == 0) {
      printf("Could not initialize Window\n");
      printf("Error: %s\n",SDL_GetError());
    }
    
    renderer = SDL_CreateRenderer(screen, -1, SDL_RENDERER_ACCELERATED);
    
    //SDL_BITSPERPIXEL(format);
    
    SDL_SetRenderDrawBlendMode(renderer,
                               SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer,0x00,0x00,0xff,0xff);
    SDL_RenderClear(renderer);
    set_system_bg(255);//alpha always 255
}

void sdl_read_thread();

bool redraw_req=true;
int forced_recreaterenderer=0;
int last_kb_shown=-2;

void sdl_render_thread() {
  
  SDL_Event event;
  SDL_StartTextInput();
  
  for(;;) {

    if(redraw_req) {
      redraw_req=false; // should go first as draw can itself trigger a redraw.
      redraw_screen();
    } else {
      SDL_Delay(10);
    }

    SDL_Event event;
    
    int ret = 1;
    for(;ret==1;) {
    
      ret = SDL_PollEvent(&event);
      if(ret != 0) {
        redraw_req=true;
        sdl_read_thread(&event);
      }
    }
      
    if(hterm_quit == true) return;
 
    #ifdef IOS_BUILD
    if((SDL_IsScreenKeyboardShown(screen) != last_kb_shown) &&
       (last_kb_shown != -3)) {
      SDL_GetWindowSize(screen,&display_width,&display_height);
      display_width_abs = display_width;
      display_height_abs = display_height;
      
      #ifdef IOS_BUILD
      if(SDL_IsScreenKeyboardShown(screen)) {
        display_width  = display_width_last_kb;
        display_height = display_height_last_kb;
      } else {
      }
      #endif

      redraw_required();
    }
    last_kb_shown = SDL_IsScreenKeyboardShown(screen);
    #endif
  }
}

void redraw_required() {
  redraw_req=true;
}


uint8_t *paste_text() {

  #ifdef IOS_BUILD
  return iphone_paste();
  #endif

  #ifdef OSX_BUILD
  return osx_paste();
  #endif

}

void copy_text(uint16_t *itext,int len) {

  // TODO: This needs to be updated to generate UTF8 text
  
  size_t pos=0;
  char text[20000];
  for(int n=0;n<len;n++) {
    size_t s = utf8proc_encode_char(itext[n],text+pos);
    pos+=s;
    text[pos  ]=0;
    text[pos+1]=0;
    text[pos+2]=0;
    text[pos+3]=0;
    text[pos+4]=0;
  }
  
  #ifdef IOS_BUILD
  iphone_copy(text);
  #endif

  #ifdef OSX_BUILD
  osx_copy(text);
  #endif
 
  // execute these two commands on Linux/XWindows by default
  //echo "test" | xclip -selection c
  //echo "test" | xclip -i 
}

int delta_sum=0;

bool select_disable=false;

int last_text_point_x = -1;
int last_text_point_y = -1;
void process_mouse_event(SDL_Event *event) {
  

  #ifdef IOS_BUILD
  if(event->type == SDL_FINGERMOTION) {
   
     SDL_Touch *t = SDL_GetTouch(event->tfinger.touchId);

     if(t->num_fingers != 0) {
     }

     if(t->num_fingers == 2) {
       return; // prevent select code from running.
     } else {
       select_disable=false;
     }
  }
  
  if(event->type == SDL_FINGERUP) {
  }

  if(select_disable) {
    draw_selection = false;
    return;
  }
  #endif

  if((event->type != SDL_MOUSEMOTION) && (event->type != SDL_MOUSEBUTTONUP) && (event->type != SDL_MOUSEBUTTONDOWN) && (event->type != SDL_MOUSEWHEEL)) return;

  int mouse_x = event->motion.x;
  int mouse_y = event->motion.y;

  #if defined(OSX_BUILD) || defined(LINUX_BUILD)
  if(event->type == SDL_MOUSEWHEEL) {

    if(event->wheel.y > 0) {
      redraw_required();
    } else {
      redraw_required();
    }
  } else
  #endif
  if(event->type == SDL_MOUSEMOTION    ) {

  } else
  if(event->type == SDL_MOUSEBUTTONUP  ) {
  } else
  if(event->type == SDL_MOUSEBUTTONDOWN) {
  }
}

void sdl_read_thread(SDL_Event *event) {
  ngui_receive_event(event);

  process_mouse_event(event);
    
  // I can't remember what effect this has on the iOS build, so it's not used there for now.
  #ifndef IOS_BUILD
  if(event->type == SDL_QUIT) {
    hterm_quit = true;
    return;
  }
  #endif

  if(forced_recreaterenderer>1) forced_recreaterenderer--;
    
  if((forced_recreaterenderer==1) ||
     ((event->type == SDL_WINDOWEVENT) &&
     ((event->window.event == SDL_WINDOWEVENT_RESIZED) || (event->window.event == SDL_WINDOWEVENT_RESTORED)))
    ) {
      forced_recreaterenderer=0;
      SDL_GetWindowSize(screen,&display_width_abs,&display_height_abs);

      display_width  = event->window.data1;
      display_height = event->window.data2;

      if(SDL_IsScreenKeyboardShown(screen)) {
        display_width  = display_width_last_kb;
        display_height = display_height_last_kb;
      }

      SDL_DestroyRenderer(renderer);
      renderer = SDL_CreateRenderer(screen, -1, SDL_RENDERER_ACCELERATED);
      ngui_set_renderer(renderer, redraw_required);
      nunifont_initcache();
      SDL_StartTextInput();

      SDL_RaiseWindow(screen);

      #ifdef IOS_BUILD
      if(SDL_IsScreenKeyboardShown(screen)) {
      } else {
      }
      #endif
      redraw_required();
  }
    
  if((event->type == SDL_WINDOWEVENT) && (event->window.event == SDL_WINDOWEVENT_ROTATE)) {
    int w = event->window.data1;
    int h = event->window.data2;

    display_width  = w;
    display_height = h;

    if(SDL_IsScreenKeyboardShown(screen)) {
      display_width_last_kb  = w;
      display_height_last_kb = h;
    }
      
    #ifdef IOS_BUILD
    if(SDL_IsScreenKeyboardShown(screen)) {
    } else {
    }
    #endif
  }
  
  if(event->type == SDL_TEXTINPUT) {
    char buffer[255];
        
    strcpy(buffer, event->text.text);
        
    if(buffer[0] == 10) buffer[0]=13; // hack round return sending 10, which causes issues for e.g. nano.
                                      // really this should be a full utf8 decode/reencode.

  }
    
  if(event->type == SDL_TEXTEDITING) {
  }

  #if defined(OSX_BUILD) || defined(LINUX_BUILD)
  if(event->type == SDL_KEYUP) {
     SDL_Scancode scancode = event->key.keysym.scancode;
     if((scancode == SDL_SCANCODE_LCTRL) || (scancode == SDL_SCANCODE_RCTRL)) {
     }
  }
  #endif

  if(event->type == SDL_KEYDOWN) {
   
     SDL_Scancode scancode = event->key.keysym.scancode;
     if((scancode == SDL_SCANCODE_DELETE) || (scancode == SDL_SCANCODE_BACKSPACE)) {
       char buf[4];
       buf[0] = 127;
       buf[1] = 0;
     }

     #if defined(OSX_BUILD) || defined(LINUX_BUILD)
     if(scancode == SDL_SCANCODE_ESCAPE) {
       char buf[4];
       buf[0] = 27;
       buf[1] = 0;
     }
     #endif

     #if defined(OSX_BUILD)
     if(scancode == SDL_SCANCODE_RETURN) {
       char buf[4];
       buf[0] = 13;
       buf[1] = 0;
     }
     if(scancode == SDL_SCANCODE_TAB) {
       char buf[4];
       buf[0] = '\t';
       buf[1] = 0;
     }
     #endif

     #if defined(OSX_BUILD) || defined(LINUX_BUILD)
     if((scancode == SDL_SCANCODE_LCTRL) || (scancode == SDL_SCANCODE_RCTRL)) {
     }
     #endif
   
     if(scancode == SDL_SCANCODE_LEFT) {
       char buf[4];
       buf[0] = 0x1b;
       buf[1] = 'O';
       buf[2] = 'D';
       buf[3] = 0;
     } else 
       if(scancode == SDL_SCANCODE_RIGHT) {
       char buf[4];
       buf[0] = 0x1b;
       buf[1] = 'O';
       buf[2] = 'C';
       buf[3] = 0;
     } else 
     if(scancode == SDL_SCANCODE_UP) {
       char buf[4];
       buf[0] = 0x1b;
       buf[1] = 'O';
       buf[2] = 'A';
       buf[3] = 0;
     } else 
     if(scancode == SDL_SCANCODE_DOWN) {
       char buf[4];
       buf[0] = 0x1b;
       buf[1] = 'O';
       buf[2] = 'B';
       buf[3] = 0;
     }
     #if defined(OSX_BUILD) || defined (LINUX_BUILD) 
     #endif

     // non iOS paste code.
     SDL_Keymod mod = SDL_GetModState();
     #if defined(OSX_BUILD) || defined (LINUX_BUILD)
     if(((event->key.keysym.sym == 'v') && (mod & KMOD_CTRL)) || 
        ((event->key.keysym.sym == 'v') && (mod & KMOD_GUI)) 
       ) {
       // perform text paste
       uint8_t *text = paste_text();
       if(text != 0) {
       }
     }
     #endif
  }
}

void do_run() {
  ngui_flowbox_run();
}

int main(int argc, char **argv) {
    
  do_sdl_init();
  SDL_GetWindowSize(screen,&display_width,&display_height);
  display_width_abs = display_width;
  display_height_abs = display_height;

  ngui_set_renderer(renderer, redraw_required);
  
  nunifont_load_staticmap(__fontmap_static,__widthmap_static,__fontmap_static_len,__widthmap_static_len);
  
  ngui_add_flowbox(20,30,"google search",NULL);
  ngui_add_flowbox(20,100,"and"         ,NULL);
  ngui_add_flowbox(20,180,"combine"     ,NULL);
  ngui_add_flowbox(20,250,"filter"      ,NULL);
  ngui_add_flowbox(20,320,"report"      ,NULL);
  ngui_add_button (0 ,0  ,"Run/Stop"    ,do_run);
  sdl_render_thread();
   
  return 0;
}
