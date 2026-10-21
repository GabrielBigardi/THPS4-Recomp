/* CPU-side runtime: indirect-call dispatcher (overlay aware), COP0, syscall/break, diagnostics. */
#include "psx.h"

CPU g_cpu;
int g_trace_calls;

bool bios_rom_call(CPU *c, uint32_t addr);

/* ------------------------------------------------------------------ call trace ring */
#define TRACE_RING 64
static uint32_t trace_ring[TRACE_RING];
static unsigned trace_pos;

void trace_func(CPU *c, uint32_t addr) {
    trace_ring[trace_pos++ % TRACE_RING] = addr;
    if (g_trace_calls > 1) LOGI("-> %08X (ra=%08X a0=%08X a1=%08X)", addr, c->r[31], c->r[4], c->r[5]);
}

void dump_backtrace(void) {
    LOGE("current function: %08X", g_cpu.cur_func);
    if (g_trace_calls) {
        char line[1024];
        size_t o = 0;
        for (unsigned i = 0; i < TRACE_RING; i++) {
            uint32_t a = trace_ring[(trace_pos + i) % TRACE_RING];
            if (!a) continue;
            o += (size_t)snprintf(line + o, sizeof line - o, "%08X ", a);
            if (o > 900) { LOGE("  recent calls: %s", line); o = 0; }
        }
        if (o) LOGE("  recent calls: %s", line);
    }
    char regs[1024];
    size_t o = 0;
    for (int i = 0; i < 32; i++) o += (size_t)snprintf(regs + o, sizeof regs - o, "r%d=%08X%s", i, g_cpu.r[i], (i % 8 == 7) ? "\n" : " ");
    LOGE("registers:\n%s", regs);
}

/* ------------------------------------------------------------------ dispatcher */
static const FuncEntry *find_entry(const ModuleDesc *m, uint32_t a) {
    int lo = 0, hi = m->nfuncs - 1;
    while (lo <= hi) {
        int mid = (lo + hi) >> 1;
        uint32_t v = m->funcs[mid].addr;
        if (v == a) return &m->funcs[mid];
        if (v < a) lo = mid + 1; else hi = mid - 1;
    }
    return NULL;
}

/* the function's check words are in RAM at load address `base` (0 for fixed modules) */
static bool check_words(uint32_t base, const FuncEntry *e) {
    if (e->c0 == 0xFFFF) return true;
    if (ram_r32(base + e->addr + e->c0) != e->w0) return false;
    return e->c1 == 0xFFFF || ram_r32(base + e->addr + e->c1) == e->w1;
}

/* is relocatable module m loaded at `base`? (all its signature words match) */
static bool signature_at(const ModuleDesc *m, uint32_t base) {
    if ((base & 3) || base < 0x80000000u || base + m->end > 0x80200000u) return false;
    for (int i = 0; i < m->nsig; i++)
        if (ram_r32(base + m->sig[2 * i]) != m->sig[2 * i + 1]) return false;
    return true;
}

/* small direct-mapped cache of validated lookups */
#define DCACHE 4096
static struct { uint32_t addr, base; const FuncEntry *e; const ModuleDesc *m; } dcache[DCACHE];

/* relocatable modules are loaded into heap memory that later holds other modules (the menu
 * modules share boilerplate code at the same offsets), so a module only counts as present
 * while its whole signature is in RAM at its load address */
static bool reloc_present(const ModuleDesc *m, uint32_t base) {
    return base && g_reloc_base[m->reloc_index] == base && signature_at(m, base);
}

static bool cache_valid(unsigned h) {
    const ModuleDesc *m = dcache[h].m;
    if (m->kind == MOD_FIXED) return true;
    return reloc_present(m, dcache[h].base) && check_words(dcache[h].base, dcache[h].e);
}

/* module m was found at base: forget other modules whose memory it now occupies */
static void reloc_set_base(const ModuleDesc *m, uint32_t base) {
    for (int i = 0; i < g_num_modules; i++) {
        const ModuleDesc *o = &g_modules[i];
        uint32_t ob = o->kind == MOD_RELOC ? g_reloc_base[o->reloc_index] : 0;
        if (o == m || !ob) continue;
        if (ob < base + m->end && base < ob + o->end) {
            LOGI("module %s (at %08X) replaced by %s", o->name, ob, m->name);
            g_reloc_base[o->reloc_index] = 0;
        }
    }
    if (g_reloc_base[m->reloc_index] != base) LOGI("module %s loaded at %08X", m->name, base);
    g_reloc_base[m->reloc_index] = base;
}

static RecompFunc found(unsigned h, uint32_t a, uint32_t base, const ModuleDesc *m, const FuncEntry *e,
                        const char **modname) {
    dcache[h].addr = a;
    dcache[h].base = base;
    dcache[h].e = e;
    dcache[h].m = m;
    if (modname) *modname = m->name;
    return e->fn;
}

RecompFunc dispatch_lookup(uint32_t addr, const char **modname) {
    uint32_t a = 0x80000000u | (addr & 0x1FFFFFFFu);
    unsigned h = (a >> 2) & (DCACHE - 1);
    if (dcache[h].addr == a && dcache[h].m && cache_valid(h)) {
        if (modname) *modname = dcache[h].m->name;
        return dcache[h].e->fn;
    }
    /* fixed modules, then relocatable modules at their known load addresses */
    for (int i = 0; i < g_num_modules; i++) {
        const ModuleDesc *m = &g_modules[i];
        uint32_t base = m->kind == MOD_RELOC ? g_reloc_base[m->reloc_index] : 0;
        if (m->kind == MOD_RELOC && !base) continue;
        if (a < base + m->code_lo || a >= base + m->code_hi) continue;
        if (m->kind == MOD_RELOC && !reloc_present(m, base)) continue;
        const FuncEntry *e = find_entry(m, a - base);
        if (e && check_words(base, e)) return found(h, a, base, m, e, modname);
    }
    /* code the game loaded somewhere new: find which relocatable module it is by trying each
     * of its functions as the call target and checking the module signature there */
    for (int i = 0; i < g_num_modules; i++) {
        const ModuleDesc *m = &g_modules[i];
        if (m->kind != MOD_RELOC) continue;
        for (int k = 0; k < m->nfuncs; k++) {
            const FuncEntry *e = &m->funcs[k];
            uint32_t base = a - e->addr;
            if (!signature_at(m, base) || !check_words(base, e)) continue;
            reloc_set_base(m, base);
            return found(h, a, base, m, e, modname);
        }
    }
    return NULL;
}

void call_addr(CPU *c, uint32_t addr) {
    uint32_t p = addr & 0x1FFFFFFFu;
    if (p == 0xA0 || p == 0xB0 || p == 0xC0) {
        bios_call(c, p, c->r[9]);
        return;
    }
    if (p >= 0x1FC00000u && p < 0x1FC80000u) {
        bios_rom_call(c, addr);
        return;
    }
    RecompFunc f = dispatch_lookup(addr, NULL);
    if (UNLIKELY(!f)) {
        /* describe which module (if any) covers that address */
        uint32_t a = 0x80000000u | p;
        for (int i = 0; i < g_num_modules; i++) {
            const ModuleDesc *m = &g_modules[i];
            uint32_t base = m->kind == MOD_RELOC ? g_reloc_base[m->reloc_index] : 0;
            if ((m->kind == MOD_FIXED || base) && a >= base + m->base && a < base + m->end)
                LOGE("  inside module %s (loaded at %08X, offset %X)%s", m->name, base, a - base,
                     find_entry(m, a - base) ? " - check words differ" : " - no function entry at that address");
        }
        dump_backtrace();
        fatal("no recompiled function at %08X (called from function %08X, ra=%08X)", addr, c->cur_func, c->r[31]);
    }
    f(c);
}

void jumptable_miss(CPU *c, uint32_t site, uint32_t target) {
    dump_backtrace();
    fatal("jump table at %08X: unexpected target %08X (func %08X)", site, target, c->cur_func);
}

/* ------------------------------------------------------------------ COP0 / exceptions */
uint32_t cop0_read(CPU *c, int r) {
    switch (r) {
    case 12: return c->sr;
    case 13: return (c->cause & ~0x400u) | ((g_istat & g_imask) ? 0x400u : 0);
    case 14: return c->epc;
    case 15: return 0x00000002;
    default: return 0;
    }
}

void cop0_write(CPU *c, int r, uint32_t v) {
    switch (r) {
    case 12:
        if ((v & 0x10000) && !(c->sr & 0x10000)) LOGW("cache isolation enabled by game code (func %08X)", c->cur_func);
        c->sr = v;
        c->next_event = 0;
        break;
    case 13: c->cause = (c->cause & ~0x300u) | (v & 0x300u); break;
    default: break;
    }
}

void cop0_rfe(CPU *c) {
    c->sr = (c->sr & ~0x0Fu) | ((c->sr >> 2) & 0x0Fu);
    c->next_event = 0;
}

void psx_syscall(CPU *c, uint32_t pc) {
    (void)pc;
    bios_syscall(c);
    if (!g_in_exception) irq_check(c);
}

void psx_break(CPU *c, uint32_t pc, uint32_t code) {
    LOGW("break %X at %08X (func %08X) - ignored", code, pc, c->cur_func);
}

void psx_invalid(CPU *c, uint32_t pc, uint32_t word) {
    dump_backtrace();
    fatal("invalid instruction %08X at %08X (func %08X)", word, pc, c->cur_func);
}
