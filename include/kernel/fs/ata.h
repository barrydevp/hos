#pragma once

/// @brief Initializes the ATA drivers.
/// @return 0 on success, 1 on error.
int ata_init();

/// @brief De-initializes the ATA drivers.
/// @return 0 on success, 1 on error.
int ata_finalize();
