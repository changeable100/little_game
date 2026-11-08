#include "maze_ui_runtime.h"
#include "maze_runtime.h"
#include "maze_levels.h"
#include <stdint.h>
#include <stddef.h>

static bool owned, bound, have_snapshot, locked;
static uintptr_t session;
static maze_result_t last_result = MAZE_OK;
static maze_level_t level_copy;

const char *maze_ui_runtime_startup_text(void)
{
    switch (maze_runtime_init_status()) {
    case MAZE_RUNTIME_READY: return "进入后点开始";
    case MAZE_RUNTIME_NOT_INITIALIZED: return "游戏尚未初始化";
    case MAZE_RUNTIME_SELFTEST_FAILED: return "自检失败，请重启设备";
    case MAZE_RUNTIME_LEVEL_FAILED: return "关卡初始化失败";
    case MAZE_RUNTIME_QUEUE_FAILED: return "操作队列初始化失败";
    case MAZE_RUNTIME_TIMER_FAILED: return "刷新时钟初始化失败";
    default: return "启动状态异常，请重启设备";
    }
}

maze_result_t maze_ui_runtime_last_result(void) { return last_result; }

static void deleted(void *context)
{
    if ((uintptr_t)context != session || !owned) return;
    if (bound) { bound=false; last_result=maze_runtime_detach(); }
    owned=have_snapshot=false;
}

maze_result_t maze_ui_runtime_release(void)
{
    maze_result_t result=MAZE_OK;
    if (!owned) return result;
    if (bound) { bound=false; result=maze_runtime_detach(); }
    owned=have_snapshot=false;
    maze_ui_destroy();
    last_result=result;
    return result;
}

static void render(void *context, const maze_snapshot_t *s, const maze_level_t *l)
{
    if ((uintptr_t)context!=session || !owned || !bound) return;
    if(l)level_copy=*l; /* cells are immutable game-lifetime storage. */
    have_snapshot=s!=NULL;
    maze_ui_render(s,l);
    if(locked)maze_ui_feedback(MAZE_EXHAUSTED,have_snapshot);
}

static maze_result_t submit(void *context, const maze_action_t *intent)
{
    if((uintptr_t)context!=session || !owned || !bound || !have_snapshot)
        return MAZE_STALE;
    maze_result_t result;
    maze_action_t action;
    if(!intent) result=MAZE_INVALID;
    else if(locked) result=MAZE_EXHAUSTED;
    else {
        action=*intent;
        result=maze_runtime_next_request(&action.request_id);
        if(result==MAZE_OK)result=maze_runtime_action(NULL,&action);
    }
    if(result==MAZE_EXHAUSTED)locked=true;
    maze_snapshot_t actual;
    maze_result_t read=maze_runtime_snapshot(&actual);
    if(read==MAZE_OK)render(context,&actual,&level_copy);
    else have_snapshot=false;
    last_result=result==MAZE_OK?read:result;
    maze_ui_feedback(last_result,read==MAZE_OK);
    /* Failed BACK or reread leaves the page and its failure visible. */
    if(intent && intent->action==MAZE_ACTION_BACK && result==MAZE_OK && read==MAZE_OK)
        (void)maze_ui_runtime_release();
    return result;
}

int maze_ui_runtime_open(void)
{
    if(owned)return MAZE_UI_ERR_EXISTS;
    if(maze_runtime_init_status()!=MAZE_RUNTIME_READY) {
        last_result=MAZE_WRONG_STATE; return MAZE_UI_ERR_NOT_CONNECTED;
    }
    if(session==UINTPTR_MAX)return MAZE_UI_ERR_NOT_CONNECTED;
    ++session;
    int result=maze_ui_create(submit,(void *)session);
    if(result!=MAZE_UI_SUCCESS)return result;
    owned=true;have_snapshot=bound=false;
    maze_ui_set_delete_handler(deleted,(void *)session);
    last_result=maze_runtime_attach(render,(void *)session);
    if(last_result!=MAZE_OK) {
        /* Not bound: deletion must not detach another renderer. */
        owned=false;maze_ui_destroy();return MAZE_UI_ERR_NOT_CONNECTED;
    }
    bound=true;
    level_copy=*maze_levels_default();
    maze_snapshot_t s;
    last_result=maze_runtime_snapshot(&s);
    if(last_result!=MAZE_OK) {
        maze_result_t read_error=last_result;
        (void)maze_ui_runtime_release();last_result=read_error;
        return MAZE_UI_ERR_NOT_CONNECTED;
    }
    render((void *)session,&s,&level_copy);
    return MAZE_UI_SUCCESS;
}
