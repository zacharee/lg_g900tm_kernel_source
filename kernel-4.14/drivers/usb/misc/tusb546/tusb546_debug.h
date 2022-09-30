#ifndef __TUSB546_DEBUG_H__
#define __TUSB546_DEBUG_H__

#ifdef CONFIG_DEBUG_FS
extern void tusb_dbg_event(const char*, int);
extern void tusb_dbg_print(const char*, int, const char*);
#else
static inline void tusb_dbg_event(const char *name, int status)
{  }
static inline void tusb_dbg_print(const char *name, int status, const char *extra)
{  }
#endif
#endif /* __TUSB546_DEBUG_H__ */
