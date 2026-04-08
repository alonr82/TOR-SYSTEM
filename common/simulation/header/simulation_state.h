#ifndef SIMULATION_STATE_H
#define SIMULATION_STATE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    bool attack_sybil_active;
    bool def_sybil_active;
    bool attack_replay_active;
    bool def_replay_active;
    bool attack_circ_ext_active;
    bool def_circ_ext_active;
    bool attack_onion_bomb_active;
    bool def_onion_bomb_active;
    bool attack_fingerprint_active;
    bool def_fingerprint_active;
    uint32_t mock_cpu_load;
}simulation_state_t;

void simulation_state_init(void);

void simulation_state_reset(void);

simulation_state_t simulation_state_get_copy(void);

bool simulation_state_set_flag(const char* attack_name, bool is_defense, bool state);

#endif