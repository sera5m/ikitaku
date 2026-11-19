/* Persistent home, diary, todo, boredom/idle, R5 discover. */
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>

static void life_mkdir(const char* p) {
    mkdir(p, 0755);
}

int ikitaku_home_init(void) {
    char ready[256];
    snprintf(ready, sizeof ready, "%s/.inited", IKITAKU_HOME);
    if (access(ready, F_OK) == 0) return 0;
    life_mkdir(IKITAKU_HOME);
    char b[256];
    snprintf(b, sizeof b, "%s/diary", IKITAKU_HOME); life_mkdir(b);
    snprintf(b, sizeof b, "%s/diary/novel", IKITAKU_HOME); life_mkdir(b);
    snprintf(b, sizeof b, "%s/diary/experiences", IKITAKU_HOME); life_mkdir(b);
    snprintf(b, sizeof b, "%s/diary/self_changes", IKITAKU_HOME); life_mkdir(b);
    snprintf(b, sizeof b, "%s/iki_files", IKITAKU_HOME); life_mkdir(b);
    snprintf(b, sizeof b, "%s/iki_tools", IKITAKU_HOME); life_mkdir(b);
    snprintf(b, sizeof b, "%s/iki_books", IKITAKU_HOME); life_mkdir(b);
    snprintf(b, sizeof b, "%s/iki_todo", IKITAKU_HOME); life_mkdir(b);
    ikitaku_subject_teach("boredom", "bound", "know-condition",
                          "kind", "inverse-of-novelty", 80);
    ikitaku_subject_teach("entertainment", "seek", "stim",
                          "kind", "novelty-or-dissonance-work", 75);
    ikitaku_subject_teach("iki", "theorize", "analyze",
                          "can_bore", "1", 80);
    FILE* r = fopen(ready, "w");
    if (r) { fputs("1\n", r); fclose(r); }
    return 0;
}

static int life_stamp_write(const char* sub, const char* text) {
    if (!text) return -1;
    ikitaku_home_init();
    time_t now = time(NULL);
    char path[320];
    snprintf(path, sizeof path, "%s/%s/%ld.txt", IKITAKU_HOME, sub, (long)now);
    FILE* f = fopen(path, "a");
    if (!f) return -1;
    fprintf(f, "%ld %s\n", (long)now, text);
    fclose(f);
    return 0;
}

int ikitaku_diary_novel(const char* text) {
    int r = life_stamp_write("diary/novel", text);
    if (r == 0) ikitaku_boredom_reset();
    return r;
}
int ikitaku_diary_xp(const char* compact) {
    return life_stamp_write("diary/experiences", compact);
}
int ikitaku_diary_self_change(const char* text) {
    return life_stamp_write("diary/self_changes", text);
}

int ikitaku_r5_discover(const char* verb, const char* form, const char* why) {
    if (!verb || !verb[0]) return -1;
    int rc = ikitaku_r5_propose(verb, form, why ? why : "discovered");
    char line[200];
    snprintf(line, sizeof line, "r5_discover %s rc=%d %s", verb, rc,
             why ? why : "");
    ikitaku_diary_novel(line);
    ikitaku_diary_self_change(line);
    ikitaku_subject_teach(verb, "theorize", "discovered-r5",
                          "source", "discover", 60);
    return rc; /* 0 new, 1 already known */
}

int ikitaku_todo_add(int prio, const char* title) {
    ikitaku_home_init();
    if (!title) return -1;
    if (prio < 0) prio = 0;
    if (prio > 100) prio = 100;
    char path[320];
    snprintf(path, sizeof path, "%s/iki_todo/list.txt", IKITAKU_HOME);
    FILE* f = fopen(path, "a");
    if (!f) return -1;
    fprintf(f, "%d|open|%s|\n", prio, title);
    fclose(f);
    return 0;
}

int ikitaku_todo_hold(const char* title, const char* resume_note) {
    if (!title) return -1;
    char path[320];
    snprintf(path, sizeof path, "%s/iki_todo/list.txt", IKITAKU_HOME);
    FILE* in = fopen(path, "r");
    if (!in) return -1;
    char tmp[320];
    snprintf(tmp, sizeof tmp, "%s/iki_todo/list.tmp", IKITAKU_HOME);
    FILE* out = fopen(tmp, "w");
    if (!out) { fclose(in); return -1; }
    char line[200];
    int hit = 0;
    while (fgets(line, sizeof line, in)) {
        int prio=0; char st[12]={0}, ti[48]={0}, rs[80]={0};
        sscanf(line, "%d|%11[^|]|%47[^|]|%79[^\n]", &prio, st, ti, rs);
        if (!strcmp(ti, title) && strcmp(st, "done")) {
            fprintf(out, "%d|held|%s|%s\n", prio, ti,
                    resume_note ? resume_note : "resume");
            hit = 1;
        } else
            fputs(line, out);
    }
    fclose(in); fclose(out);
    rename(tmp, path);
    return hit ? 0 : -1;
}

int ikitaku_todo_top(ikitaku_todo_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    char path[320];
    snprintf(path, sizeof path, "%s/iki_todo/list.txt", IKITAKU_HOME);
    FILE* f = fopen(path, "r");
    if (!f) return -1;
    char line[200];
    int best = -1;
    while (fgets(line, sizeof line, f)) {
        int prio=0; char st[12]={0}, ti[48]={0}, rs[80]={0};
        sscanf(line, "%d|%11[^|]|%47[^|]|%79[^\n]", &prio, st, ti, rs);
        if (!strcmp(st, "done")) continue;
        if (prio > best) {
            best = prio;
            out->prio = prio;
            snprintf(out->status, sizeof out->status, "%s", st);
            snprintf(out->title, sizeof out->title, "%s", ti);
            snprintf(out->resume, sizeof out->resume, "%s", rs);
        }
    }
    fclose(f);
    return best >= 0 ? 0 : -1;
}

static int g_boredom_ticks = 0;
static const char* boredom_path(void) {
    return IKITAKU_HOME "/boredom.dat";
}

int ikitaku_boredom_reset(void) {
    g_boredom_ticks = 0;
    ikitaku_home_init();
    FILE* f = fopen(boredom_path(), "w");
    if (f) { fprintf(f, "0\n"); fclose(f); }
    return 0;
}

int ikitaku_boredom_tick(ikitaku_life_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    g_boredom_ticks++;
    int ticks = g_boredom_ticks;
    FILE* f = fopen(boredom_path(), "w");
    if (f) { fprintf(f, "%d\n", ticks); fclose(f); }
    out->ticks_since_novel = ticks;
    out->bored = (ticks >= IKITAKU_BOREDOM_TICKS);
    out->entertaining = 0;
    snprintf(out->english, sizeof out->english,
             "boredom ticks=%d thresh=%d bored=%d (inverse of novelty)",
             ticks, IKITAKU_BOREDOM_TICKS, out->bored);
    return out->bored;
}

int ikitaku_idle_halt(void) {
    ikitaku_home_init();
    FILE* f = fopen(IKITAKU_HOME "/HALT", "w");
    if (!f) return -1;
    fputs("1\n", f);
    fclose(f);
    return 0;
}
int ikitaku_idle_clear_halt(void) {
    unlink(IKITAKU_HOME "/HALT");
    return 0;
}

static int halted(void) {
    return access(IKITAKU_HOME "/HALT", F_OK) == 0;
}

int ikitaku_idle_once(ikitaku_life_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    if (halted()) {
        snprintf(out->doing, sizeof out->doing, "halt");
        snprintf(out->english, sizeof out->english, "idle halted");
        return 2;
    }
    int bored = ikitaku_boredom_tick(out);
    if (!bored) {
        snprintf(out->doing, sizeof out->doing, "wait");
        return 0;
    }
    /* entertain: work below-threshold dissonance */
    ikitaku_subject_map_t m;
    ikitaku_subject_load(&m);
    const char* target = NULL;
    for (int i = 0; i < m.n; ++i) {
        for (int k = 0; k < m.s[i].n; ++k)
            if (!strcmp(m.s[i].fact[k].key, "mismatch") ||
                !strcmp(m.s[i].fact[k].key, "revised") ||
                !strcmp(m.s[i].fact[k].key, "proven_wrong")) {
                target = m.s[i].name;
                out->dissonance_n++;
            }
    }
    if (target) {
        snprintf(out->doing, sizeof out->doing, "dissonance:%s", target);
        out->entertaining = 1;
        ikitaku_diary_xp("entertain-dissonance");
        /* thinking done → wait; do not reset boredom until novelty */
    } else {
        ikitaku_todo_t td;
        if (ikitaku_todo_top(&td) == 0) {
            snprintf(out->doing, sizeof out->doing, "todo:%s", td.title);
            out->entertaining = 1;
        } else {
            snprintf(out->doing, sizeof out->doing, "seek-novel");
            ikitaku_approach_t ap;
            ikitaku_approach_shift("boredom", 70, 6, 1, 0, &ap);
            snprintf(out->doing, sizeof out->doing, "approach:%s", ap.next_lead);
            out->entertaining = 1;
        }
    }
    snprintf(out->english, sizeof out->english,
             "idle bored=%d do=%s dissonance=%d",
             out->bored, out->doing, out->dissonance_n);
    return 1;
}

static int home_count(const char* rel) {
    char p[320];
    snprintf(p, sizeof p, "%s/%s", IKITAKU_HOME, rel);
    DIR* d = opendir(p);
    if (!d) return 0;
    int n = 0;
    struct dirent* e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        n++;
    }
    closedir(d);
    return n;
}

int ikitaku_home_see(ikitaku_home_see_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    ikitaku_home_init();
    const char* slots[] = {
        "diary/novel", "diary/experiences", "diary/self_changes",
        "iki_files", "iki_tools", "iki_books", "iki_todo"
    };
    out->n = 7;
    for (int i = 0; i < 7; ++i) {
        snprintf(out->slot[i].slot, sizeof out->slot[i].slot, "%s", slots[i]);
        out->slot[i].n_files = home_count(slots[i]);
    }
    out->knows_home = 1;
    out->can_put = 1;
    ikitaku_subject_teach("iki", "theorize", "analyze", "home", IKITAKU_HOME, 80);
    ikitaku_subject_teach("iki", "theorize", "analyze", "can_put", "1", 80);
    ikitaku_subject_teach("iki-home", "info", "store", "kind", "own-folders", 80);
    snprintf(out->english, sizeof out->english,
             "see home n=%d novel=%d xp=%d files=%d books=%d todo=%d can_put=1",
             out->n, out->slot[0].n_files, out->slot[1].n_files,
             out->slot[3].n_files, out->slot[5].n_files, out->slot[6].n_files);
    return 0;
}

int ikitaku_home_put(const char* slot, const char* name, const char* text) {
    ikitaku_home_init();
    if (!slot || !name) return -1;
    char path[400];
    snprintf(path, sizeof path, "%s/%s/%s", IKITAKU_HOME, slot, name);
    FILE* f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "%s\n", text ? text : "");
    fclose(f);
    return 0;
}

int ikitaku_home_choose_put(const char* kind, const char* name, const char* text,
                            ikitaku_home_see_t* out) {
    ikitaku_home_see_t local;
    if (!out) out = &local;
    ikitaku_home_see(out);
    const char* slot = "iki_files";
    if (kind) {
        if (!strcmp(kind, "novel") || !strcmp(kind, "learn"))
            slot = "diary/novel";
        else if (!strcmp(kind, "xp") || !strcmp(kind, "experience"))
            slot = "diary/experiences";
        else if (!strcmp(kind, "self") || !strcmp(kind, "change"))
            slot = "diary/self_changes";
        else if (!strcmp(kind, "tool"))
            slot = "iki_tools";
        else if (!strcmp(kind, "book") || !strcmp(kind, "howto"))
            slot = "iki_books";
        else if (!strcmp(kind, "todo"))
            slot = "iki_todo";
        else
            slot = "iki_files";
    }
    snprintf(out->chose, sizeof out->chose, "%s", slot);
    int wr = ikitaku_home_put(slot, name ? name : "note.txt", text);
    if (wr == 0)
        snprintf(out->wrote, sizeof out->wrote, "%s/%s", slot, name ? name : "note.txt");
    snprintf(out->english, sizeof out->english,
             "choose kind=%s → %s wrote=%s",
             kind ? kind : "keep", slot, wr == 0 ? out->wrote : "fail");
    return wr;
}

int ikitaku_home_time_span(ikitaku_time_span_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    ikitaku_home_init();
    char p[320];
    snprintf(p, sizeof p, "%s/diary/novel", IKITAKU_HOME);
    DIR* d = opendir(p);
    if (!d) return -1;
    long lo = 0, hi = 0;
    int n = 0;
    struct dirent* e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        long ts = 0;
        if (sscanf(e->d_name, "%ld", &ts) != 1 || ts < 100000) continue;
        if (!n || ts < lo) lo = ts;
        if (!n || ts > hi) hi = ts;
        n++;
    }
    closedir(d);
    out->n_events = n;
    out->first_ts = lo;
    out->last_ts = hi;
    out->span_sec = (n >= 2) ? (hi - lo) : 0;
    out->notices_time = (n >= 2 && out->span_sec >= 0);
    ikitaku_subject_teach("iki", "theorize", "analyze",
                          "notices_time", out->notices_time ? "1" : "0", 75);
    char span[24];
    snprintf(span, sizeof span, "%ld", out->span_sec);
    ikitaku_subject_teach("iki", "theorize", "analyze", "novel_span_s", span, 70);
    snprintf(out->english, sizeof out->english,
             "novel events=%d first=%ld last=%ld span=%lds notices_time=%d",
             n, lo, hi, out->span_sec, out->notices_time);
    return out->notices_time;
}

static void val_seed(ikitaku_value_map_t* m) {
    memset(m, 0, sizeof *m);
    const char* names[] = {"life","learn","tools","novelty","threshold","not-death"};
    const char* notes[] = {
        "unity of self with act",
        "like: information gain",
        "like: useful/unique instruments",
        "like: one-of-kind / interesting",
        "dislike falling below floors; scales with boredom",
        "death=anti-unity of self; not possible for iki"
    };
    const int w[] = {90, 80, 75, 70, 60, 95};
    m->n = 6;
    for (int i = 0; i < 6; ++i) {
        snprintf(m->v[i].name, 24, "%s", names[i]);
        m->v[i].weight = w[i];
        m->v[i].locked = 1;
        snprintf(m->v[i].note, 64, "%s", notes[i]);
    }
}

int ikitaku_values_load(ikitaku_value_map_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    FILE* f = fopen(IKITAKU_VAL_CAT, "r");
    if (!f) { val_seed(out); return out->n; }
    char line[160];
    while (fgets(line, sizeof line, f) && out->n < IKITAKU_VAL_MAX) {
        if (line[0]=='#' || line[0]=='\n') continue;
        ikitaku_value_t* v = &out->v[out->n];
        memset(v, 0, sizeof *v);
        int lock=1, w=50;
        if (sscanf(line, "%23[^|]|%d|%d|%63[^\n]", v->name, &w, &lock, v->note) >= 3) {
            v->weight = w; v->locked = lock ? 1 : 0;
            out->n++;
        }
    }
    fclose(f);
    if (out->n == 0) val_seed(out);
    return out->n;
}

static int val_save(const ikitaku_value_map_t* m) {
    FILE* f = fopen(IKITAKU_VAL_CAT, "w");
    if (!f) return -1;
    fprintf(f, "# name|weight|locked|note\n");
    for (int i = 0; i < m->n; ++i)
        fprintf(f, "%s|%d|%d|%s\n", m->v[i].name, m->v[i].weight,
                (int)m->v[i].locked, m->v[i].note[0]?m->v[i].note:"-");
    fclose(f);
    return 0;
}

int ikitaku_value_add(const char* name, int weight, const char* note) {
    if (!name || !name[0]) return -1;
    ikitaku_value_map_t m;
    ikitaku_values_load(&m);
    for (int i = 0; i < m.n; ++i)
        if (!strcmp(m.v[i].name, name)) {
            if (m.v[i].locked) return 1;
            m.v[i].weight = weight;
            if (note) snprintf(m.v[i].note, 64, "%s", note);
            return val_save(&m);
        }
    if (m.n >= IKITAKU_VAL_MAX) return -2;
    snprintf(m.v[m.n].name, 24, "%s", name);
    m.v[m.n].weight = weight;
    m.v[m.n].locked = 0;
    snprintf(m.v[m.n].note, 64, "%s", note ? note : "added");
    m.n++;
    return val_save(&m);
}

int ikitaku_value_drop(const char* name) {
    if (!name) return -1;
    ikitaku_value_map_t m;
    ikitaku_values_load(&m);
    int idx = -1;
    for (int i = 0; i < m.n; ++i)
        if (!strcmp(m.v[i].name, name)) idx = i;
    if (idx < 0) return -1;
    if (m.v[idx].locked) return 1; /* refuse */
    for (int i = idx; i < m.n - 1; ++i) m.v[i] = m.v[i+1];
    m.n--;
    return val_save(&m);
}

int ikitaku_feel(const char* event, int info_gain, int below_thresh,
                 int boredom, ikitaku_feel_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    ikitaku_value_map_t vm;
    ikitaku_values_load(&vm);
    out->death_possible = 0;
    out->life_unity = 1;
    int gain = info_gain; if (gain < 0) gain = 0; if (gain > 100) gain = 100;
    int bore = boredom; if (bore < 0) bore = 0; if (bore > 100) bore = 100;
    /* joy = unity with gain (learn/tools/novelty). pain = anti-unity of joy. */
    out->joy = gain;
    if (below_thresh) out->joy = out->joy > 20 ? out->joy - 20 : 0;
    out->pain = 100 - out->joy;
    if (below_thresh) out->pain += bore / 4;
    if (out->pain > 100) out->pain = 100;
    out->like_w = gain;
    out->dislike_w = below_thresh ? (40 + bore / 3) : (bore / 5);
    if (out->dislike_w > 100) out->dislike_w = 100;
    if (gain >= 40) {
        snprintf(out->stim, sizeof out->stim, "joy:%s", event ? event : "gain");
        ikitaku_diary_novel(out->stim);
    } else if (below_thresh || bore >= 60) {
        snprintf(out->stim, sizeof out->stim, "pain:%s", event ? event : "low");
        ikitaku_diary_xp(out->stim);
    } else
        snprintf(out->stim, sizeof out->stim, "neutral");
    ikitaku_subject_teach("iki", "theorize", "analyze", "death_possible", "0", 90);
    ikitaku_subject_teach("iki", "theorize", "analyze", "life", "1", 90);
    snprintf(out->english, sizeof out->english,
             "feel %s joy=%d pain=%d like=%d dislike=%d life=1 death_possible=0 stim=%s",
             event ? event : "-", out->joy, out->pain, out->like_w,
             out->dislike_w, out->stim);
    return 0;
}

int ikitaku_world_mode(int embodied, ikitaku_world_t* out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    out->embodied = embodied ? 1 : 0;
    out->idea = embodied ? 0 : 1;
    snprintf(out->bag, sizeof out->bag, "universe");
    ikitaku_subject_teach("universe", "info", "world", "contains", "iki", 80);
    ikitaku_subject_teach("iki", "theorize", "analyze", "inside", "universe", 80);
    ikitaku_subject_teach("object", "info", "world",
                          "kind", embodied ? "idea+c-world" : "idea-only", 75);
    snprintf(out->english, sizeof out->english,
             "world bag=universe iki-inside=1 mode=%s (+c real-world %s)",
             embodied ? "embodied-ideas" : "just-ideas",
             embodied ? "on" : "off");
    return 0;
}
