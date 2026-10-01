#ifndef NLE_UI_H /* Binary UI modifications, 2026-10-01; see doc/nle/BINARY_UI.md. */
#define NLE_UI_H
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
#define NLE_UI_VERSION 3
#define NLE_UI_ENABLED 1u
#define NLE_UI_SCREEN 2u
#define NLE_UI_DIAGNOSTICS 4u
#define NLE_UI_FEATURES 383u
enum { NLE_UI_COMMAND, NLE_UI_MORE, NLE_UI_YN, NLE_UI_LINE, NLE_UI_MENU,
       NLE_UI_POSITION, NLE_UI_TEXT, NLE_UI_END };
typedef struct { uint32_t offset, length; } nle_ui_string;
typedef struct {
 int32_t row,x,y,attr,color;
 uint32_t letter,group,flags;
 nle_ui_string text;
} nle_ui_menu;
typedef struct { int32_t row,x,y,attr,color; nle_ui_string text; } nle_ui_text;
typedef struct { int32_t kind,x,y,color,attr; nle_ui_string text; } nle_ui_draw;
typedef struct nle_ui_v3 {
 uint32_t version,size;
 uint64_t generation,features;
 int32_t complete,screen,wait_kind,window,window_type,page,pages,pick_mode,cursor_y,cursor_x;
 uint32_t default_answer,legacy_flags;
 nle_ui_string question,choices,input,prompt,message,title,status;
 uint32_t message_count,menu_count,text_count,draw_count,bytes_count;
 const nle_ui_string *messages;
 const nle_ui_menu *menu;
 const nle_ui_text *text;
 const nle_ui_draw *draws;
 const char *bytes;
} nle_ui_v3;
int nle_ui_configure_v3(uint32_t flags);
const nle_ui_v3 *nle_ui_current_v3(void);
int nle_ui_enabled(void);
int nle_ui_screen(void);
void nle_ui_move(int x,int y);
void nle_ui_relative(int dx,int dy);
void nle_ui_clear(int kind);
void nle_ui_char(int c);
void nle_ui_style(int attr,int color,int on);
void nle_ui_graphics(int on);
void nle_ui_position(int active);
void nle_ui_prompt(int kind,const char *query,const char *choices,int def,const char *input);
void nle_ui_message(const char *text,int replace);
void nle_ui_more_marker(void);
void nle_ui_more_shown(void);
void nle_ui_yn_shown(const char *choices,int def);
void nle_ui_menu_state(int window,void *start,void *end,int page,const char *responses);
void nle_ui_text_state(int window,int first,int count);
void nle_ui_text_reset(int window);
void nle_ui_text_begin(int row,int attr);
void nle_ui_text_end(void);
void nle_ui_window_end(int window);
void nle_ui_capture(int ended);
void nle_ui_resume(void);
void nle_ui_release(void);
#ifdef __cplusplus
}
#endif
#endif
