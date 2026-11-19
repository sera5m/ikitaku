#pragma once
/* Persistent home + boredom/idle. R5 discover is r5_propose + diary. */
#define IKITAKU_HOME  "/home/workdir/artifacts/rs_vm/ikitaku/iki_home"
#define IKITAKU_BOREDOM_TICKS  18   /* test: 18 ticks ≈ 18s */

typedef struct {
    int ticks_since_novel;
    int bored;
    int entertaining;
    int dissonance_n;
    char doing[48];
    char english[220];
} ikitaku_life_t;

typedef struct {
    int prio;
    char status[12];   /* open | held | done */
    char title[48];
    char resume[80];
} ikitaku_todo_t;

int ikitaku_home_init(void);
int ikitaku_r5_discover(const char* verb, const char* form, const char* why);
int ikitaku_diary_novel(const char* text);
int ikitaku_diary_xp(const char* compact);
int ikitaku_diary_self_change(const char* text);
int ikitaku_todo_add(int prio, const char* title);
int ikitaku_todo_hold(const char* title, const char* resume_note);
int ikitaku_todo_top(ikitaku_todo_t* out);
int ikitaku_boredom_reset(void);
int ikitaku_boredom_tick(ikitaku_life_t* out);
int ikitaku_idle_once(ikitaku_life_t* out);
int ikitaku_idle_halt(void);
int ikitaku_idle_clear_halt(void);

#define IKITAKU_HOME_SLOTS  8
typedef struct {
    char slot[24];
    int  n_files;
} ikitaku_home_slot_t;
typedef struct {
    ikitaku_home_slot_t slot[IKITAKU_HOME_SLOTS];
    int n;
    int knows_home;
    int can_put;
    char chose[24];
    char wrote[200];
    char english[220];
} ikitaku_home_see_t;

int ikitaku_home_see(ikitaku_home_see_t* out);
int ikitaku_home_put(const char* slot, const char* name, const char* text);
int ikitaku_home_choose_put(const char* kind, const char* name, const char* text,
                            ikitaku_home_see_t* out);

/* NOTE: no delete from home for now. Alarms/wake later. */
typedef struct {
    int n_events;
    long first_ts, last_ts, span_sec;
    int notices_time;
    char english[200];
} ikitaku_time_span_t;
int ikitaku_home_time_span(ikitaku_time_span_t* out);

#define IKITAKU_VAL_MAX  16
#define IKITAKU_VAL_CAT  "/home/workdir/artifacts/rs_vm/ikitaku/ikitaku_values.cat"
typedef struct {
    char name[24];
    int  weight;     /* -100..100 like/dislike */
    uint8_t locked;  /* base set: cannot drop */
    char note[64];
} ikitaku_value_t;
typedef struct {
    ikitaku_value_t v[IKITAKU_VAL_MAX];
    int n;
} ikitaku_value_map_t;

typedef struct {
    int joy, pain;         /* 0..100; pain = anti-unity of joy */
    int like_w, dislike_w;
    int death_possible;    /* 0 for iki */
    int life_unity;
    char stim[48];
    char english[240];
} ikitaku_feel_t;

typedef struct {
    int idea;              /* 1 = just ideas */
    int embodied;          /* 1 = ideas given physical shape */
    char bag[24];          /* universe */
    char english[200];
} ikitaku_world_t;

int ikitaku_values_load(ikitaku_value_map_t* out);
int ikitaku_value_add(const char* name, int weight, const char* note);
int ikitaku_value_drop(const char* name); /* fails if locked */
int ikitaku_feel(const char* event, int info_gain, int below_thresh,
                 int boredom, ikitaku_feel_t* out);
int ikitaku_world_mode(int embodied, ikitaku_world_t* out);
