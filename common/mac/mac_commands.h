#ifndef LORAWAN_MAC_COMMANDS_COMMON_H_
#define LORAWAN_MAC_COMMANDS_COMMON_H_

#include <stdint.h>
#include <stddef.h>

void process_mac_commands(const uint8_t *payload, size_t len);
void append_uplink_mac_cmds(void);

#endif /* LORAWAN_MAC_COMMANDS_COMMON_H_ */
