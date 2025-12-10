#include "/home/alon/Desktop/TOR system/server/relay_manager/header/relay_manager.h"
#include "/home/alon/Desktop/TOR system/server/configurations/header/dir_server_config.h"
int main(int argc, char *argv[])
{
    server_config_metadata_t* server_conf = fetch_server_config("serverconfig.conf");
    if(server_conf != NULL)
    {
        printf("server ip: %hhu.%hhu.%hhu.%hhu\n",server_conf->ip_server[0],server_conf->ip_server[1],server_conf->ip_server[2],server_conf->ip_server[3]);
        printf("server port: %hu\n",server_conf->port_server);
        free(server_conf);
    }
    return 0;
}