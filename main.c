#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "math.h"
 
#define MAX_GRID_SIZE (26 * 26 * 26)

enum BlockType {
    BLK_CASING = 0,
    BLK_AIR,

    BLK_CELL,
    BLK_GRAPHITE,
    BLK_SPEED,

    BLK_COOLER_STD,
    BLK_COOLER_WATER,
    BLK_COOLER_CRYOTHEUM,
    BLK_COOLER_REDSTONE,
    BLK_COOLER_ENDER,
    BLK_COOLER_GLOWSTONE,
    BLK_COOLER_COOLANT,
    BLK_COOLER_HELIUM,
    BLK_COUNT
};

typedef struct {
    int width;
    int height;
    int depth;
    int strides[6];
    int cooler_heat;
    int cell_rf;
    int cell_heat;
    int cell_total;
    int fuel_heat;
    int fuel_rf;
    int graphite_total;
    int active_graphite_total;
    int speed_block_total;
} ReactorState; // "Just make a fat struct, your life is too short to avoid it.
                // It's going to get taken over by robots in 5 years. 
                // Did you see that crazy video of the 1600$ wheeled Chinese robot?"
                // - JBlow

static uint8_t grid[MAX_GRID_SIZE];

static inline int idx3d(int x, int y, int z, int width, int height) {
    return x + y*width + z*width*height;
}

static inline uint8_t is_adjacent(const uint8_t* grid, int base_idx, int* strides, uint8_t block_type) {
    int num_adjacent = 0;
    for (int i = 0; i < 6; ++i) {
        num_adjacent += (grid[base_idx + strides[i]] == block_type);
    }
    return num_adjacent;
}

static inline uint8_t is_cell_adjacent_through_graphite(const uint8_t* grid, int base_idx, int* strides) {
    int n = 0;
    for (int i = 0; i < 6; ++i) {
        int idx = base_idx;
        do {
            idx += strides[i];
        } while (grid[idx] == BLK_GRAPHITE);

        n += (grid[idx] == BLK_CELL);
    }
    
    return n;
}

typedef struct {
    int base_cool;
    int bonus_cool;
    uint8_t adj_block;
    uint8_t min_count;
    uint8_t invert;
} CoolerRule;

static const CoolerRule COOLER_RULES[BLK_COUNT] = {
    [BLK_COOLER_STD]       = { -30,  -30, BLK_COOLER_STD,       1, 0 },
    [BLK_COOLER_WATER]     = { -30,  -30, BLK_CASING,           1, 0 },
    [BLK_COOLER_CRYOTHEUM] = { -80,  -80, BLK_COOLER_CRYOTHEUM, 1, 1 },
    [BLK_COOLER_REDSTONE]  = { -80,  -80, BLK_CELL,             1, 0 },
    [BLK_COOLER_ENDER]     = { -80,  -80, BLK_GRAPHITE,         1, 0 },
    [BLK_COOLER_GLOWSTONE] = { -80, -240, BLK_GRAPHITE,         6, 0 },
    [BLK_COOLER_COOLANT]   = { -80,  -80, BLK_COOLER_WATER,     1, 0 },
    [BLK_COOLER_HELIUM]    = { -125,   0, BLK_CASING,           0, 0 },
};

void eval_cell(const uint8_t* grid, int base_idx, ReactorState* state, int sign) {
    uint8_t block_type = grid[base_idx];

    switch (block_type)
    {
        case BLK_CASING:
        case BLK_AIR:
            break;
        case BLK_CELL: {
            int n = is_cell_adjacent_through_graphite(grid, base_idx, state->strides);
            state->cell_heat += sign * ((n+1)*(n+2)/2 * state->fuel_heat);
            state->cell_rf += sign * ((n+1) * state->fuel_rf);
            state->cell_total += sign;
            break;
        }
        case BLK_GRAPHITE:
            state->graphite_total += sign;
            if (is_adjacent(grid, base_idx, state->strides, BLK_CELL)) {
                state->active_graphite_total += sign;
            }
            break;
        case BLK_SPEED:
            state->speed_block_total += sign;
            break;
        default: {
            if (block_type >= BLK_COUNT) {
                printf("Unknown cell type in array! base index: %d type: %d", base_idx, block_type);
                break;
            }
            uint8_t adj_block_count[BLK_COUNT] = {0}; 
            for (int i = 0; i < 6; ++i) {
                ++adj_block_count[grid[base_idx + state->strides[i]]];
            }
            const CoolerRule* cooler_rule = &COOLER_RULES[block_type];
            state->cooler_heat += sign * (cooler_rule->base_cool + ((adj_block_count[cooler_rule->adj_block] >= cooler_rule->min_count) ^ cooler_rule->invert)*cooler_rule->bonus_cool);
            break;
        }
    }
}

void reset_accumulators(ReactorState* state) {
    state->cooler_heat = 0;
    state->cell_rf = 0;
    state->cell_heat = 0;
    state->cell_total = 0;
    state->graphite_total = 0;
    state->active_graphite_total = 0;
    state->speed_block_total = 0;
}

static inline void calc_totals(const ReactorState* state, int* total_rf, int* total_heat) {
    *total_rf = state->cell_rf + state->active_graphite_total * state->cell_total * state->fuel_rf / 10;
    *total_heat = state->cell_heat + state->cooler_heat + state->graphite_total * state->cell_total * state->fuel_rf / 5;
}

void eval_full(const uint8_t* grid, ReactorState* state, int* total_rf, int* total_heat) {
    reset_accumulators(state);
    for (int i = 1; i < state->width - 1; ++i) {
        for (int j = 1; j < state->height - 1; ++j) {
            for (int k = 1; k < state->depth - 1; ++k) {
                eval_cell(grid, idx3d(i, j, k, state->width, state->height), state, +1);
            }
        }
    }
    calc_totals(state, total_rf, total_heat);
}

void init_grid(uint8_t* grid, int inner_w, int inner_h, int inner_d, ReactorState* state) {
    state->width  = inner_w + 2;
    state->height = inner_h + 2;
    state->depth  = inner_d + 2;
    state->strides[0] = 1;
    state->strides[1] = -1;
    state->strides[2] = state->width;
    state->strides[3] = -state->width;
    state->strides[4] = state->width * state->height;
    state->strides[5] = -(state->width * state->height);


    int total_cells = state->width * state->height * state->depth;
    memset(grid, BLK_CASING, total_cells);

    for (int z = 1; z <= inner_d; ++z) {
        for (int y = 1; y <= inner_h; ++y) {
            for (int x = 1; x <= inner_w; ++x) {
                grid[idx3d(x, y, z, state->width, state->height)] = BLK_AIR;
            }
        }
    }
}

int count_affected(uint8_t* grid, int base_idx, int* strides, int* out_idx) {
    int count = 0;

    out_idx[count++] = base_idx;
    for (int i = 0; i < 6; ++i) {
        int idx = base_idx + strides[i];
        uint8_t block_type = grid[idx];
        if (block_type == BLK_CASING) continue;
        out_idx[count++] = idx;
        if (block_type != BLK_GRAPHITE) continue;
        do {
            idx += strides[i];  
            block_type = grid[idx];  
        } while (block_type == BLK_GRAPHITE);
        if (block_type == BLK_CELL) {
            out_idx[count++] = idx;
        }
    }
    return count;
}

void replace_block(uint8_t* grid, ReactorState* state, int base_idx, uint8_t new_block_type) {
    int affected[13];
    int n = count_affected(grid, base_idx, state->strides, affected);
    for (int i = 0; i < n; ++ i) {
        eval_cell(grid, affected[i], state, -1);
    }
    grid[base_idx] = new_block_type;
    for (int i = 0; i < n; ++ i) {
        eval_cell(grid, affected[i], state, +1);
    }
}

enum ScoreMode {
    MODE_POWER = 0,
    MODE_EFFICIENCY,
    MODE_BREEDING
};

static inline double score_fun(int total_rf, int total_heat, const ReactorState* state, int mode) {
    if (total_heat > 0) {
        return -1000 - total_heat*10;
    }
    if (state->cell_total == 0) {
        return 0;
    }

    switch (mode) {
        case MODE_POWER:
            return (double)total_rf;
        case MODE_BREEDING:
            return (double)state->cell_total * (1. + (double)state->speed_block_total);
        case MODE_EFFICIENCY:
        default:
            return (double)total_rf / (double)state->cell_total + 0.01*total_rf;
    }
}


static uint32_t rng_state = 123456789u;
static inline uint32_t xorshift_rand() {
    uint32_t x = rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return rng_state = x;
}

static inline double rand_double() {
    return (double)xorshift_rand() / (double)UINT32_MAX;
}

static const uint8_t ALLOWED_BLOCKS[] = {
    BLK_AIR,
    BLK_CELL,
    BLK_GRAPHITE,
    BLK_COOLER_STD,
    BLK_COOLER_WATER,
    BLK_COOLER_CRYOTHEUM,
    BLK_COOLER_REDSTONE,
    BLK_COOLER_ENDER,
    BLK_COOLER_GLOWSTONE,
    BLK_COOLER_COOLANT,
    BLK_COOLER_HELIUM
}; // TODO(idmp152): Can possibly be changed by user
#define ALLOWED_COUNT (sizeof(ALLOWED_BLOCKS) / sizeof(ALLOWED_BLOCKS[0]))

void optimize(uint8_t* grid, ReactorState* state, double T_start, double T_end, int iterations, int mode) {
    double T = T_start;
    double alpha = pow(T_end / T_start, 1./iterations);

    double score = 0.;
    int total_cells = state->width * state->height * state->depth;
    uint8_t best_grid[MAX_GRID_SIZE];
    memcpy(best_grid, grid, total_cells);

    double best_score = -99999999999999;
    for(int i = 0; i < iterations; ++i) {
        int replacements  = 0;
        int replacement_idx[5];
        int replacement_old_blocks[5];
        ReactorState old_state = *state;
        do {
            int r_idx = idx3d(1 + xorshift_rand() % (state->width - 2),
                1 + xorshift_rand() % (state->height - 2),
                1 + xorshift_rand() % (state->depth - 2), state->width, state->height);
            uint8_t r_block = ALLOWED_BLOCKS[xorshift_rand() % ALLOWED_COUNT];

            if (r_block == grid[r_idx]) continue;
            replacement_idx[replacements] = r_idx;
            replacement_old_blocks[replacements] = grid[r_idx];
            replace_block(grid, state, r_idx, r_block);
            ++replacements;
        } while (rand_double() < 0.5 * (T / T_start) && replacements < 5); // TODO(idmp152): magic, 0.5 coeff can be varied for efficiency, as well as max replacements

        int rf = 0, heat = 0;
        calc_totals(state, &rf, &heat);
        double new_score = score_fun(rf, heat, state, mode);
        if (new_score > score || rand_double() < exp((new_score - score)/T)) {
            score = new_score;

            if (new_score > best_score && heat <= 0) {
                memcpy(best_grid, grid, total_cells);
                best_score = new_score;
            }
        } else {
            for (int j = replacements - 1; j >= 0; --j) {
                grid[replacement_idx[j]] = replacement_old_blocks[j];
            }
            *state = old_state;
        }
        T = alpha*T;
    }

    memcpy(grid, best_grid, total_cells);
}

static const char BLK_CODES[BLK_COUNT][3] = {
    "XX", // BLK_CASING
    "..", // BLK_AIR
    "[]", // BLK_CELL
    "##", // BLK_GRAPHITE
    "Sp", // BLK_SPEED
    "St", // BLK_COOLER_STD
    "Wt", // BLK_COOLER_WATER
    "Cr", // BLK_COOLER_CRYOTHEUM
    "Rs", // BLK_COOLER_REDSTONE
    "Ed", // BLK_COOLER_ENDER
    "Gl", // BLK_COOLER_GLOWSTONE
    "Cl", // BLK_COOLER_COOLANT
    "He"  // BLK_COOLER_HELIUM
};


void print_grid(const uint8_t* grid, ReactorState* state) {
    char buf[65536];
    char* p = buf;
    for (int y = 1; y < state->height - 1; ++y) {
        p += snprintf(p, buf + 32768 - p, "Layer %d:\n", y);
        for (int x = 1; x < state->width - 1; ++x) {
            for (int z = 1; z < state->depth - 1; z++) {
                const char* code = BLK_CODES[grid[idx3d(x, y, z, state->width, state->height)]];
                *p++ = code[0];
                *p++ = code[1];
                *p++ = ' ';
            }
            *p++ = '\n';
        }
        *p++ = '\n';
    }
    *p = '\0';
    puts(buf);
}

int prompt_int(const char* label, int default_val, int min_val, int max_val) {
    char line[64];
    for(;;) {
        printf("%s [%d]: ", label, default_val);

        if (!fgets(line, sizeof(line), stdin)) {
            return default_val;
        }

        if (line[0] == '\n') {
            return default_val;
        }

        char* endptr = NULL;
        long val = strtol(line, &endptr, 10);
        if (endptr != line && val >= min_val && val <= max_val) {
            return (int)val;
        }
        printf("Please enter a number between %d and %d.\n", min_val, max_val);
    }
}

int prompt_mode(void) {
    char line[64];
    for(;;) {
        printf("Mode (p = power, e = efficiency, b = breeding) [p]: ");

        if (!fgets(line, sizeof(line), stdin) || line[0] == '\n') {
            return MODE_POWER;
        }
        if (line[0] == 'p' || line[0] == 'P') return MODE_POWER;
        if (line[0] == 'e' || line[0] == 'E') return MODE_EFFICIENCY;
        if (line[0] == 'b' || line[0] == 'B') return MODE_BREEDING;

        printf("Unknown mode. Type p, e, or b.\n");
    }
}


int main() {
    int w = 0, h = 0, d = 0, fuel_rf = 0, fuel_heat = 0, iterations = 0, mode = MODE_POWER;
    printf("--- NuclearCraft Fission Reactor Optimizer (pre 2.0) \n\n");
    w = prompt_int("Inner width (X)", 3, 1, 24);
    h = prompt_int("Inner height (Y)", 3, 1, 24);
    d = prompt_int("Inner depth (Z)", 3, 1, 24);
    fuel_rf = prompt_int("Fuel Base Power (RF/t)", 100, 1, INT32_MAX);
    fuel_heat = prompt_int("Fuel Base Heat (H/t)", 20, 0, INT32_MAX);
    mode = prompt_mode();
    iterations = prompt_int("Iterations", 1000000, 1000, INT32_MAX);

    ReactorState state = {0};
    state.fuel_rf = fuel_rf;
    state.fuel_heat = fuel_heat;

    init_grid(grid, w, h, d, &state);

    optimize(grid, &state, 1000., 0.1, iterations, mode);

    int rf = 0, heat = 0;
    eval_full(grid, &state, &rf, &heat);
    print_grid(grid, &state);
    printf("rf/t: %d, heat: %d H/t, cells: %d, efficiency: %d%%", rf, heat, state.cell_total, rf / state.cell_total);
    return 0;
}
//TODO(idmp152): get rid of magic numbers hopefully
//TODO(idmp152): rewrite eval_cell to probably using lookup tables (at least for coolers) 
//TODO(idmp152): look for better solutions wherever possible, this is a draft