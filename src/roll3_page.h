#ifndef ROLL3_PAGE_H
#define ROLL3_PAGE_H

#define ROLL3_PAGE_REFRESH_MS 33u

/* LVGL owner only. */
int roll3_page_open(void);
int roll3_page_open_level(unsigned level);
void roll3_page_close(void);

#endif
