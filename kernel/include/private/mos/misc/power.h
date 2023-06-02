// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdnoreturn.h>
typedef void (*power_callback_t)(void *data);

/**
 * @brief Initialize the power management subsystem.
 */
void power_init(void);

/**
 * @brief Register a callback to be called when the system is about to shut down.
 *
 * @param callback The callback to be called.
 * @param data The data to be passed to the callback.
 */
void power_register_shutdown_callback(power_callback_t callback, void *data);

/**
 * @brief Shutdown the system.
 */
noreturn void power_shutdown(void);
