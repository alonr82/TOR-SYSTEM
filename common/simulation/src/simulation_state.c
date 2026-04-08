#include "simulation_state.h"

#include <string.h>
#include <pthread.h>

static simulation_state_t g_simulation_state;
static pthread_mutex_t g_simulation_state_lock = PTHREAD_MUTEX_INITIALIZER;
static bool g_simulation_initialized = false;

void simulation_state_init(void)
{
    pthread_mutex_lock(&g_simulation_state_lock);
    if(g_simulation_initialized == false)
    {
        memset(&g_simulation_state, 0, sizeof(simulation_state_t));
        g_simulation_state.mock_cpu_load = 15;
        g_simulation_initialized = true;
    }
    pthread_mutex_unlock(&g_simulation_state_lock);
}

void simulation_state_reset(void)
{
    pthread_mutex_lock(&g_simulation_state_lock);
    memset(&g_simulation_state, 0, sizeof(simulation_state_t));
    g_simulation_state.mock_cpu_load = 15;
    g_simulation_initialized = true;
    pthread_mutex_unlock(&g_simulation_state_lock);
}

simulation_state_t simulation_state_get_copy(void)
{
    simulation_state_t retval;
    pthread_mutex_lock(&g_simulation_state_lock);
    if(g_simulation_initialized == false)
    {
        memset(&g_simulation_state, 0, sizeof(simulation_state_t));
        g_simulation_state.mock_cpu_load = 15;
        g_simulation_initialized = true;
    }
    retval = g_simulation_state;
    pthread_mutex_unlock(&g_simulation_state_lock);
    return retval;
}

bool simulation_state_set_flag(const char* attack_name, bool is_defense, bool state)
{
    bool retval = true;
    pthread_mutex_lock(&g_simulation_state_lock);

    if(g_simulation_initialized == false)
    {
        memset(&g_simulation_state, 0, sizeof(simulation_state_t));
        g_simulation_state.mock_cpu_load = 15;
        g_simulation_initialized = true;
    }

    if(attack_name == NULL)
    {
        retval = false;
    }
    else
    {
        if(strcmp(attack_name, "sybil") == 0)
        {
            if(is_defense == true)
            {
                g_simulation_state.def_sybil_active = state;
            }
            else
            {
                g_simulation_state.attack_sybil_active = state;
            }
        }
        else if(strcmp(attack_name, "replay") == 0)
        {
            if(is_defense == true)
            {
                g_simulation_state.def_replay_active = state;
            }
            else
            {
                g_simulation_state.attack_replay_active = state;
            }
        }
        else if(strcmp(attack_name, "circ_ext") == 0)
        {
            if(is_defense == true)
            {
                g_simulation_state.def_circ_ext_active = state;
            }
            else
            {
                g_simulation_state.attack_circ_ext_active = state;
            }
        }
        else if(strcmp(attack_name, "onion_bomb") == 0)
        {
            if(is_defense == true)
            {
                g_simulation_state.def_onion_bomb_active = state;
            }
            else
            {
                g_simulation_state.attack_onion_bomb_active = state;
            }
        }
        else if(strcmp(attack_name, "fingerprint") == 0)
        {
            if(is_defense == true)
            {
                g_simulation_state.def_fingerprint_active = state;
            }
            else
            {
                g_simulation_state.attack_fingerprint_active = state;
            }
        }
        else
        {
            retval = false;
        }
    }

    pthread_mutex_unlock(&g_simulation_state_lock);
    return retval;
}