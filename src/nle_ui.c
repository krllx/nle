/* Direct UI entities from the stock tty controller. No screen recognition. Binary UI modifications, 2026-10-01; see doc/nle/BINARY_UI.md.
 * Optional draw operations observe native positioning/style, never ANSI. */
#include "hack.h"
#include "wintty.h"
#include "nle_ui.h"
_Static_assert(sizeof(nle_ui_v3)==192, "UI ABI header");
_Static_assert(sizeof(nle_ui_menu)==40, "UI ABI menu");
_Static_assert(sizeof(nle_ui_text)==28, "UI ABI text");
_Static_assert(sizeof(nle_ui_draw)==28, "UI ABI draw");
extern boolean xwaitingforspace;
static uint32_t config=NLE_UI_SCREEN;
static nle_ui_v3 ui;
static char *bytes;
static size_t bytes_cap,msg_cap,menu_cap,text_cap,draw_cap;
static nle_ui_string *messages;
static nle_ui_menu *menus;
static nle_ui_text *texts;
static nle_ui_draw *draws;
static int ux,uy,fg=-1,bold,inverse,attr,graphics,position,failed;
static char visible_message[TBUFSZ],status_line[81];
static int prompt_kind,default_answer,more_full;
static char question[BUFSZ],choices[QBUFSZ],input[BUFSZ];
static int menu_window=-1,menu_page,text_window=-1,text_first,text_count;
static tty_menu_item *menu_start,*menu_end;
static struct { int valid,x,y,width,flag; } indicators[8];
static int more_x,more_y,more_seen,more_active;
static void indicator(int x,int y,int width,int flag) {
 int i;
 if(x<0||x+width>80||y<0||y>=24)return;
 for(i=0;i<8;i++)if(indicators[i].valid&&indicators[i].x==x&&indicators[i].y==y&&indicators[i].width==width&&indicators[i].flag==flag)return;
 for(i=0;i<8;i++)if(!indicators[i].valid)break;
 if(i==8){failed=1;return;}
 indicators[i].valid=1;indicators[i].x=x;indicators[i].y=y;
 indicators[i].width=width;indicators[i].flag=flag;
}
static void erase_indicators(int kind,int x,int y) {
 int i;for(i=0;i<8;i++)if(indicators[i].valid &&
  (kind==2 || (kind==1&&indicators[i].y>y) ||
   (indicators[i].y==y&&x<indicators[i].x+indicators[i].width)))indicators[i].valid=0;
}
static void overwrite_indicators(int x,int y) {
 int i;for(i=0;i<8;i++)if(indicators[i].valid&&indicators[i].y==y&&
  x>=indicators[i].x&&x<indicators[i].x+indicators[i].width)indicators[i].valid=0;
}
static void scroll_indicators(void) {
 int i;for(i=0;i<8;i++)if(indicators[i].valid&&--indicators[i].y<0)indicators[i].valid=0;
}
void nle_ui_more_marker(void) {
 more_x=ux;more_y=uy;more_seen=0;more_active=1;
 more_full=ux>=0&&ux+(int)strlen(defmorestr)<=80&&uy>=0&&uy<24;
}
void nle_ui_more_shown(void) {
 more_active=0;
 if(more_full&&more_seen==(int)strlen(defmorestr))indicator(more_x,more_y,more_seen,1);
}
void nle_ui_yn_shown(const char *c,int def) {
 int suffix=(def?4:0)+1;
 if(!nle_ui_enabled()||!c)return;
 if(strcspn(c,"\033")==2&&c[0]=='y'&&c[1]=='n')indicator(ux-suffix-4,uy,4,2);
}
static char menu_responses[QBUFSZ];
/* Presentation spans carry their semantic origin. Clear/scroll operations
 * invalidate or move spans; no queued window data is inspected at yield. */
static struct { int valid,row,x,y,attr,color; unsigned len; char value[81]; } shown[24];
static int shown_next,shown_active=-1;
static void erase_spans(int kind,int x,int y) {
 int i;
 for(i=0;i<shown_next;i++)if(shown[i].valid) {
  if(kind==2 || (kind==1 && shown[i].y>y)){shown[i].valid=0;continue;}
  if(shown[i].y==y) {
   if(x<=shown[i].x)shown[i].valid=0;
   else if(x<shown[i].x+(int)shown[i].len){shown[i].len=x-shown[i].x;shown[i].value[shown[i].len]=0;}
  }
 }
}
static void scroll_spans(void) {int i;for(i=0;i<shown_next;i++)if(shown[i].valid&&--shown[i].y<0)shown[i].valid=0;}
void nle_ui_text_reset(int w) {if(nle_ui_enabled()){shown_next=0;shown_active=-1;text_window=w;}}
void nle_ui_text_begin(int row,int a) {
 int i;
 if(!nle_ui_enabled())return;
 for(i=0;i<shown_next;i++)if(!shown[i].valid)break;
 if(i==24){failed=1;shown_active=-1;return;}
 if(i==shown_next)shown_next++;
 memset(&shown[i],0,sizeof shown[i]);shown[i].valid=1;shown[i].row=row;
 shown[i].x=ux;shown[i].y=uy;shown[i].attr=a;shown[i].color=7;shown_active=i;
}
void nle_ui_text_end(void){shown_active=-1;}

int nle_ui_enabled(void) { return !!(config & NLE_UI_ENABLED); }
int nle_ui_screen(void) { return !!(config & NLE_UI_SCREEN); }
static int grow(void **p,size_t *cap,size_t count,size_t size) {
 size_t n; void *q;
 if(count<=*cap)return 1;
 n=*cap ? *cap*2 : 32; if(n<count)n=count;
 if(n>1048576 || size>SIZE_MAX/n){failed=1;return 0;}
 q=realloc(*p,n*size); if(!q){failed=1;return 0;} *p=q;*cap=n;return 1;
}
static nle_ui_string stringn(const char *s,size_t n) {
 nle_ui_string v={0,0}; if(!s||!n)return v;
 if(n>1048576 || ui.bytes_count>1048576-n){failed=1;return v;}
 if(!grow((void**)&bytes,&bytes_cap,ui.bytes_count+n,1))return v;
 v.offset=ui.bytes_count;v.length=n;memcpy(bytes+ui.bytes_count,s,n);ui.bytes_count+=n;return v;
}
static nle_ui_string str(const char *s) {return stringn(s,s?strlen(s):0);}
static int color(int c) {return (fg<0 ? (c==' '?0:7) : fg|(bold?8:0))+(inverse?16:0);}
static void draw(int kind,int x,int y,int c) {
 nle_ui_draw *d; char ch=c;
 if(!(config&NLE_UI_DIAGNOSTICS)||!nle_ui_enabled())return;
 if(kind==1 && ui.draw_count) {
  d=&draws[ui.draw_count-1];
  if(d->kind==1 && d->y==y && d->x+(int)d->text.length==x && d->color==color(c)
     && d->attr==attr && d->text.offset+d->text.length==ui.bytes_count) {
   if(!grow((void**)&bytes,&bytes_cap,ui.bytes_count+1,1))return;
   bytes[ui.bytes_count++]=ch;d->text.length++;return;
  }
 }
 if(!grow((void**)&draws,&draw_cap,ui.draw_count+1,sizeof *draws))return;
 d=&draws[ui.draw_count++];memset(d,0,sizeof *d);
 d->kind=kind;d->x=x;d->y=y;d->color=kind==1?color(c):0;d->attr=attr;
 if(kind==1)d->text=stringn(&ch,1);
}
int nle_ui_configure_v3(uint32_t flags) {
 if((flags&~7u)||!(flags&NLE_UI_SCREEN)&&!(flags&NLE_UI_ENABLED))return -1;
 config=flags;memset(&ui,0,sizeof ui);ui.version=3;ui.size=sizeof ui;
 ui.features=NLE_UI_FEATURES|((flags&NLE_UI_DIAGNOSTICS)?128u:0);
 ux=uy=bold=inverse=attr=graphics=position=failed=prompt_kind=0;fg=-1;
 shown_next=0;shown_active=-1;memset(indicators,0,sizeof indicators);more_active=0;
 menu_window=text_window=-1;visible_message[0]=question[0]=choices[0]=input[0]=0;
 memset(status_line,' ',80);status_line[80]=0;
 return 0;
}
const nle_ui_v3 *nle_ui_current_v3(void) {return nle_ui_enabled()?&ui:0;}
void nle_ui_release(void) {
 free(bytes);free(messages);free(menus);free(texts);free(draws);
 bytes=0;messages=0;menus=0;texts=0;draws=0;
 bytes_cap=msg_cap=menu_cap=text_cap=draw_cap=0;
}
void nle_ui_resume(void) {
 if(!nle_ui_enabled())return;
 ui.bytes_count=ui.message_count=ui.draw_count=ui.menu_count=ui.text_count=0;
}
void nle_ui_move(int x,int y) {if(nle_ui_enabled()){ux=max(0,min(79,x));uy=max(0,min(23,y));}}
void nle_ui_relative(int dx,int dy){if(nle_ui_enabled()){ux=max(0,min(79,ux+dx));uy=max(0,min(23,uy+dy));}}
void nle_ui_clear(int kind) {
 if(!nle_ui_enabled())return;
 draw(kind==0?2:kind==1?3:4,ux,uy,0);
 erase_spans(kind,ux,uy);
 erase_indicators(kind,ux,uy);
 if(kind==2 || uy==0&&ux==0)visible_message[0]=0;
 if(kind==2 || kind==1&&uy<=23)memset(status_line,' ',80);
 else if(uy==23&&ux>=0&&ux<80)memset(status_line+ux,' ',80-ux);
}
void nle_ui_graphics(int on){graphics=on;}
void nle_ui_char(int c) {
 if(!nle_ui_enabled())return;
 c=(unsigned char)c; /* NetHack display bytes can have their high bit set. */
 if(c=='\b'){ux=max(0,ux-1);return;}
 if(c=='\r'){ux=0;return;}
 if(c=='\n'){if(more_active)more_full=0;uy++;if(uy>23){uy=23;draw(5,0,0,0);scroll_spans();scroll_indicators();}return;}
 if(c<32||c==127)return;
 if(ux>=80){ux=0;uy++;if(uy>23){uy=23;draw(5,0,0,0);scroll_spans();scroll_indicators();}}
 if(more_active){if(ux!=more_x+more_seen||uy!=more_y)more_full=0;more_seen++;}
 overwrite_indicators(ux,uy);
 if(shown_active>=0) {
  unsigned n=shown[shown_active].len;
  if(n<80){shown[shown_active].value[n]=c;shown[shown_active].value[n+1]=0;shown[shown_active].len++;
   if(c!=' ')shown[shown_active].color=color(c);}
 }
 draw(1,ux,uy,c);
 if(uy==23&&ux>=0&&ux<80)status_line[ux]=c;
 ux++;
 if(ux>=80){ux=0;uy++;if(uy>23){uy=23;draw(5,0,0,0);scroll_spans();scroll_indicators();}}
}
void nle_ui_style(int a,int c,int on) {
 if(!nle_ui_enabled())return;
 if(c>=0){if(on==2){attr=inverse=0;}fg=c&7;bold=!!(c&8);return;}
 if(a<0){fg=-1;bold=inverse=attr=0;return;}
 if(a==0)return;
 if(a==ATR_BOLD)bold=on;
 if(a==ATR_INVERSE)inverse=on;
 if(on)attr|=1<<a;else {attr=0;bold=inverse=0;fg=-1;}
}
void nle_ui_position(int active){if(nle_ui_enabled())position=active;}
void nle_ui_prompt(int kind,const char *q,const char *c,int def,const char *in) {
 size_t n;
 if(!nle_ui_enabled())return;
 prompt_kind=kind;default_answer=(kind==NLE_UI_YN&&!c)?0:def;
 snprintf(question,sizeof question,"%.*s",(kind==NLE_UI_YN&&c)?QBUFSZ-1:BUFSZ-1,q?q:"");
 n=c?strcspn(c,"\033"):0;n=min(n,sizeof choices-1);
 if(n)memcpy(choices,c,n);choices[n]=0;
 snprintf(input,sizeof input,"%s",in?in:"");
}
void nle_ui_message(const char *s,int replace) {
 size_t n;
 if(!nle_ui_enabled())return;
 if(replace)snprintf(visible_message,sizeof visible_message,"%s",s?s:"");
 else {
  const char *last; int width,start,gap;
  n=strlen(visible_message);last=strrchr(visible_message,'\n');
  width=last?(int)strlen(last+1):(int)n;
  start=(WIN_MESSAGE>=0&&WIN_MESSAGE<MAXWIN&&wins[WIN_MESSAGE])?(int)wins[WIN_MESSAGE]->curx-(int)strlen(s?s:""):width;
  gap=max(0,start-width);
  while(gap--&&n<sizeof visible_message-1)visible_message[n++]=' ';
  visible_message[n]=0;
  snprintf(visible_message+n,sizeof visible_message-n,"%s",s?s:"");
 }
 if(s&&*s) {
  const char *literal=strstr(s,defmorestr);
  if(literal){int tail=strlen(literal);indicator(ux-tail,uy,strlen(defmorestr),1);}
 }
 if(s&&*s&&grow((void**)&messages,&msg_cap,ui.message_count+1,sizeof *messages))messages[ui.message_count++]=str(s);
}
void nle_ui_menu_state(int w,void *start,void *end,int page,const char *responses) {
 if(!nle_ui_enabled())return;
 menu_window=w;menu_start=start;menu_end=end;menu_page=page;
 snprintf(menu_responses,sizeof menu_responses,"%s",responses?responses:"");
}
void nle_ui_text_state(int w,int first,int count) {if(nle_ui_enabled()){text_window=w;text_first=first;text_count=count;}}
void nle_ui_window_end(int w){if(menu_window==w)menu_window=-1;if(text_window==w)text_window=-1;}
void nle_ui_capture(int ended) {
 struct WinDesc *cw=0;tty_menu_item *p;int row,n,w=-1;
 if(!nle_ui_enabled())return;
 ++ui.generation;ui.screen=nle_ui_screen();ui.wait_kind=NLE_UI_COMMAND;
 ui.window=-1;ui.window_type=ui.page=ui.pages=ui.pick_mode=0;ui.default_answer=0;
 ui.question=ui.choices=ui.input=ui.prompt=ui.title=(nle_ui_string){0,0};
 ui.menu_count=ui.text_count=0;
 if(ended)ui.wait_kind=NLE_UI_END;
 else if(ttyDisplay&&ttyDisplay->inmore) {ui.wait_kind=NLE_UI_MORE;ui.prompt=str(more_full?defmorestr:"");}
 else if(xwaitingforspace && text_window>=0) {ui.wait_kind=NLE_UI_TEXT;w=text_window;}
 else if(xwaitingforspace && menu_window>=0) {ui.wait_kind=NLE_UI_MENU;w=menu_window;}
 else if(xwaitingforspace){ui.wait_kind=NLE_UI_MORE;ui.prompt=str(defmorestr);}
 else if(prompt_kind){ui.wait_kind=prompt_kind;ui.question=str(question);ui.choices=str(choices);ui.input=str(input);ui.default_answer=default_answer;}
 else if(position)ui.wait_kind=NLE_UI_POSITION;
 if(w>=0&&w<MAXWIN&&(cw=wins[w])) {
  ui.window=w;ui.window_type=cw->type;ui.pick_mode=cw->how;ui.prompt=str(cw->morestr?cw->morestr:defmorestr);
  if(ui.wait_kind==NLE_UI_MENU) {
   ui.page=menu_page+1;ui.pages=cw->npages;
   for(row=0,p=menu_start;p&&p!=menu_end;p=p->next,row++) {
    nle_ui_menu *m;size_t len=strlen(p->str),off=p->nle_text_offset;
    if(!grow((void**)&menus,&menu_cap,ui.menu_count+1,sizeof *menus))break;
    m=&menus[ui.menu_count++];memset(m,0,sizeof *m);
    m->row=row;m->x=cw->offx+1+off;m->y=cw->offy+row;m->attr=p->attr;m->color=7;
    m->letter=p->selector;m->group=0; /* unprinted group accelerators are not visible data */
    m->flags=(p->identifier.a_void?1:0)|(p->selected?2:0)|(p->selected&&p->count!=-1L?4:0);
    n=max(0,min((int)len,(int)ttyDisplay->cols-cw->offx-2));
    m->text=stringn(p->str+min(off,len),max(0,n-(int)off));
    if(p->nle_title)ui.title=m->text;
   }
  }else {
   ui.page=1+text_first/max(1,ttyDisplay->rows-cw->offy-1);
   for(row=0;row<shown_next;row++)if(shown[row].valid) {
    nle_ui_text *out;
    if(!grow((void**)&texts,&text_cap,ui.text_count+1,sizeof *texts))break;
    out=&texts[ui.text_count++];memset(out,0,sizeof *out);
    out->row=shown[row].row;out->x=shown[row].x;out->y=shown[row].y;
    out->attr=shown[row].attr;out->color=shown[row].color;
    out->text=stringn(shown[row].value,shown[row].len);
   }
  }
 }
 ui.message=str(!ttyDisplay||!ttyDisplay->toplin||(cw&&cw->offx==0&&cw->offy==0)?"":
  ((ui.wait_kind==NLE_UI_LINE||ui.wait_kind==NLE_UI_COMMAND||ui.wait_kind==NLE_UI_POSITION)?toplines:visible_message));
 ui.legacy_flags=0;
 for(row=0;row<8;row++)if(indicators[row].valid)ui.legacy_flags|=indicators[row].flag;
 ui.status=stringn(status_line,80);
 ui.cursor_y=uy;ui.cursor_x=ux;
 ui.complete=!failed;ui.messages=messages;ui.menu=menus;ui.text=texts;ui.draws=draws;ui.bytes=bytes;
}
