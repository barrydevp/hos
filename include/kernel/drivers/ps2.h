#pragma once

/// @brief Initializes ps2 devices.
/// @return 0 on success, 1 on failure.
int ps2_init();

/// @brief Writes data to the PS/2 port.
/// @param data the data to write.
void ps2_write(unsigned char data);

/// @brief Reads data from the PS/2 port.
/// @return the data coming from the PS/2 port.
unsigned char ps2_read();
