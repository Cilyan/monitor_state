/*
 * monitor_state.c
 * 
 * Copyright 2014 Cilyan Olowen <gaknar@gmail.com>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 * 
 * 
 */

#include <dbus/dbus.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#define LED_COLOR_PATH "/sys/bus/platform/devices/bubbatwo/color"
#define LED_MODE_PATH  "/sys/bus/platform/devices/bubbatwo/ledmode"

#define LED_BLUE     (0)
#define LED_RED      (1)
#define LED_GREEN    (2)
#define LED_PURPLE   (3)

#define SYSTEM_STATE_UNKNOWN_STR     "unknown"
#define SYSTEM_STATE_UNKNOWN         'u'

#define SYSTEM_STATE_STARTING_STR    "starting"
#define SYSTEM_STATE_STARTING        's'

#define SYSTEM_STATE_RUNNING_STR     "running"
#define SYSTEM_STATE_RUNNING         'r'

#define SYSTEM_STATE_MAINTENANCE_STR "maintenance"
#define SYSTEM_STATE_MAINTENANCE     'm'

#define SYSTEM_STATE_DEGRADED_STR    "degraded"
#define SYSTEM_STATE_DEGRADED        'd'

#define SYSTEM_STATE_STOPPING_STR    "stopping"
#define SYSTEM_STATE_STOPPING        't'

#define SYSTEM_STATE_SIZE            ((size_t)12)

#define SERVICE_NAME         "org.freedesktop.systemd1"
#define SERVICE_OBJECT_PATH  "/org/freedesktop/systemd1"
#define SERVICE_IFACE        DBUS_INTERFACE_PROPERTIES
#define SERVICE_METHOD       "Get"

static const char* TARGET_OBJECT   = "org.freedesktop.systemd1.Manager";
static const char* TARGET_PROPERTY = "SystemState";

struct Monitor_ {
    char system_state_str[SYSTEM_STATE_SIZE];
    char system_state;
    char system_state_old;
    void ((*on_system_state_change)(char,char));
};

typedef struct Monitor_ Monitor;

/**
 * Create a new Monitor object
 * 
 * @return an empty Monitor object or NULL on failure
 */
Monitor* monitor_new(void) {
    Monitor* monitor;
    monitor = malloc(sizeof(Monitor));
    if (monitor == NULL) {
        return NULL;
    }
    memset(monitor, 0, sizeof(Monitor));
    /* Default values ("unknown") */
    strncpy(
        monitor->system_state_str,
        SYSTEM_STATE_UNKNOWN_STR,
        SYSTEM_STATE_SIZE
    );
    monitor->system_state     = SYSTEM_STATE_UNKNOWN;
    monitor->system_state_old = SYSTEM_STATE_UNKNOWN;
    return monitor;
}

/**
 * Free a monitor object, deallocating all attached ressources.
 * 
 * @arg monitor The monitor object.
 */
void monitor_destroy(Monitor* monitor) {
    free(monitor);
}

void monitor_register_on_system_state_change_cb(
    Monitor* monitor,
    void ((*on_system_state_change)(char,char))
) {
    monitor->on_system_state_change = on_system_state_change;
}

/**
 * Get current SystemState property from Systemd over DBus
 * 
 * The current state retrieved over DBus and updated in the
 * `system_state_str` property. Resulting `system_state_str` can be
 * `"starting"`, `"running"`, `"degraded"`, `"maintenance"` or
 * `"stopping"`. See Systemd manual for more information.
 * 
 * @arg monitor A Monitor object
 * @return 0 on success, -1 on failure.
 */
int _monitor_update_system_state_str(Monitor* monitor) {
    
    DBusConnection* bus = NULL;
    DBusMessage* msg = NULL;
    DBusMessage* reply = NULL;
    DBusMessageIter iter, sub;
    DBusError error;
    dbus_bool_t rval = FALSE;
    int arg_type = DBUS_TYPE_INVALID;
    char * system_state_str;
    
    memset(monitor->system_state_str, 0, SYSTEM_STATE_SIZE);
    
    dbus_error_init(&error);
    
    /* Get System Bus connection */
    bus = dbus_bus_get(DBUS_BUS_SYSTEM, &error);
    if (dbus_error_is_set(&error)) {
        goto error_bus;
    }
    dbus_connection_set_exit_on_disconnect(bus, FALSE);
    
    /* Create method call */
    msg = dbus_message_new_method_call(
        SERVICE_NAME,
        SERVICE_OBJECT_PATH,
        SERVICE_IFACE,
        SERVICE_METHOD
    );
    
    if (msg == NULL) {
        dbus_set_error(
            &error, DBUS_ERROR_NO_MEMORY,
            "Memory allocation failed for message"
        );
        goto error_msg;
    }
    
    /* Add arguments to method call */
    rval = dbus_message_append_args(
        msg,
        DBUS_TYPE_STRING, &TARGET_OBJECT,
        DBUS_TYPE_STRING, &TARGET_PROPERTY,
        DBUS_TYPE_INVALID
    );
    if (!rval) {
        dbus_set_error(
            &error, DBUS_ERROR_NO_MEMORY,
            "Memory allocation failed for message arguments"
        );
        goto error_args;
    }
    
    /* Send message, wait return */
    reply = dbus_connection_send_with_reply_and_block(
        bus,
        msg,
        400,
        &error
    );
    if (!reply) {
        goto error_args;
    }
    
    /* Get iterator on reply argument */
    rval = dbus_message_iter_init(reply, &iter);
    if (!rval) {
        dbus_set_error(
            &error, DBUS_ERROR_INVALID_ARGS,
            "No arguments in return message"
        );
        goto error_reply;
    }
    
    /* First argument is Variant */
    arg_type = dbus_message_iter_get_arg_type (&iter);
    if (arg_type != DBUS_TYPE_VARIANT) {
        dbus_set_error(
            &error, DBUS_ERROR_INVALID_ARGS,
            "Argument type error in return message: %c expected v<s>",
            (char) arg_type
        );
        goto error_reply;
    }
    
    /* Enter variant, we should find string */
    dbus_message_iter_recurse(&iter, &sub);
    arg_type = dbus_message_iter_get_arg_type (&sub);
    if (arg_type != DBUS_TYPE_STRING) {
        dbus_set_error(
            &error, DBUS_ERROR_INVALID_ARGS,
            "Argument type error in return message: "
            "v<%c> expected v<s>",
            (char) arg_type
        );
        goto error_reply;
    }
    
    /* Get argument */
    dbus_message_iter_get_basic(&sub, &system_state_str);
    strncpy(
        monitor->system_state_str,
        system_state_str,
        SYSTEM_STATE_SIZE
    );
    
    /* Free ressources */
    dbus_message_unref(reply);
    dbus_message_unref(msg);
    dbus_connection_unref(bus);
    
    return 0;
    
    /* Error handling */
error_reply:
    dbus_message_unref(reply);
error_args:
    dbus_message_unref(msg);
error_msg:
    dbus_connection_unref(bus);
error_bus:
    if (dbus_error_is_set(&error)) {
        fprintf(
            stderr,
            "Error in the Bus: %s\n%s\n",
            error.name, error.message
        );
        dbus_error_free(&error);
    }
    return -1;
}

/**
 * Parse the `system_state_str` to fill `system_state`.
 * 
 * @arg monitor The monitor object.
 */
void _monitor_parse_system_state(Monitor* monitor) {
    char c0;
    
    c0 = monitor->system_state_str[0];
    switch(c0) {
        case 's':
            if (!strncmp(
                monitor->system_state_str,
                SYSTEM_STATE_STARTING_STR,
                SYSTEM_STATE_SIZE
            )) {
                monitor->system_state = SYSTEM_STATE_STARTING;
            }
            else if (!strncmp(
                monitor->system_state_str,
                SYSTEM_STATE_STOPPING_STR,
                SYSTEM_STATE_SIZE
            )) {
                monitor->system_state = SYSTEM_STATE_STOPPING;
            }
            else {
                monitor->system_state = SYSTEM_STATE_UNKNOWN;
            }
            break;
        case 'r':
            if (!strncmp(
                monitor->system_state_str,
                SYSTEM_STATE_RUNNING_STR,
                SYSTEM_STATE_SIZE
            )) {
                monitor->system_state = SYSTEM_STATE_RUNNING;
            }
            else {
                monitor->system_state = SYSTEM_STATE_UNKNOWN;
            }
            break;
        case 'm':
            if (!strncmp(
                monitor->system_state_str,
                SYSTEM_STATE_MAINTENANCE_STR,
                SYSTEM_STATE_SIZE
            )) {
                monitor->system_state = SYSTEM_STATE_MAINTENANCE;
            }
            else {
                monitor->system_state = SYSTEM_STATE_UNKNOWN;
            }
            break;
        case 'd':
            if (!strncmp(
                monitor->system_state_str,
                SYSTEM_STATE_DEGRADED_STR,
                SYSTEM_STATE_SIZE
            )) {
                monitor->system_state = SYSTEM_STATE_DEGRADED;
            }
            else {
                monitor->system_state = SYSTEM_STATE_UNKNOWN;
            }
            break;
        default:
            monitor->system_state = SYSTEM_STATE_UNKNOWN;
            break;
    }
}

/**
 * Update `system_state*` properties from Systemd over DBus
 * 
 * The current state retrieved over DBus and updated in the
 * `system_state_str` property. Resulting `system_state_str` can be
 * `"starting"`, `"running"`, `"degraded"`, `"maintenance"` or
 * `"stopping"`. See Systemd manual for more information.
 * 
 * @arg monitor A Monitor object
 * @return 0 on success, -1 on failure.
 */
int monitor_update_system_state(Monitor* monitor) {
    int rval;
    
    /* Shift current state to old state */
    monitor->system_state_old = monitor->system_state;
    
    /* Update system_state_str over DBus */
    rval = _monitor_update_system_state_str(monitor);
    
    /* Handle error */
    if (rval != 0) {
        strncpy(
            monitor->system_state_str,
            SYSTEM_STATE_UNKNOWN_STR,
            SYSTEM_STATE_SIZE
        );
    }
    
    /* Update system_state */
    _monitor_parse_system_state(monitor);
    
    return rval;
}

void monitor_run(Monitor* monitor) {
    
    int rval = 0;
    
    const struct timespec halfsecond = {
        .tv_sec   = 0,
        .tv_nsec = 500000000
    };
    
    while (1) {
        
        monitor_update_system_state(monitor);
        
        if (monitor->system_state != monitor->system_state_old) {
            if (monitor->on_system_state_change != NULL) {
                monitor->on_system_state_change(
                    monitor->system_state,
                    monitor->system_state_old
                );
            }
        }
        
        rval = nanosleep(&halfsecond, NULL);
        
        if (rval != 0) {
            if (errno == EINTR) {
                break;
            }
        }
    }
}

void on_system_state_change(char system_state, char system_state_old) {
    FILE* led_color;
    
    led_color = fopen(LED_COLOR_PATH, "w");
    
    if (led_color == NULL) {
        return;
    }
    
    switch(system_state) {
        case SYSTEM_STATE_STARTING:
        case SYSTEM_STATE_STOPPING:
            fprintf(led_color, "%u", LED_PURPLE);
            break;
        case SYSTEM_STATE_DEGRADED:
        case SYSTEM_STATE_MAINTENANCE:
            fprintf(led_color, "%u", LED_RED);
            break;
        case SYSTEM_STATE_RUNNING:
            fprintf(led_color, "%u", LED_BLUE);
            break;
        default:
            /* Do not change led color */
            break;
    }
    
    fclose(led_color);
}

void signal_handler (int signo) {
    /* Leave a few seconds for the program to terminate */
    alarm(2);
}

int main(int argc, char **argv)
{
    FILE* led_color;
    FILE* led_mode;
    
    Monitor* monitor;
    
    monitor = monitor_new();
    
    if (monitor == NULL) {
        fprintf(stderr, "Couldn't create monitor object\n");
        return -1;
    }
    
    monitor_register_on_system_state_change_cb(monitor, on_system_state_change);
    
    /* Check color control file */
    led_color = fopen(LED_COLOR_PATH, "w");
    if (led_color == NULL) {
        fprintf(stderr, "Couldn't open led color control file for writing\n");
        return -1;
    }
    fclose(led_color);
    
    /* Light led */
    led_mode = fopen(LED_MODE_PATH, "w");
    if (led_mode == NULL) {
        fprintf(stderr, "Couldn't open led mode control file for writing\n");
        return -1;
    }
    fprintf(led_mode, "lit");
    fclose(led_mode);
    
    /* Install signal handlers */
    if (signal(SIGINT, signal_handler) == SIG_ERR) {
        fprintf(stderr, "Couldn't install signal handler for SIGINT\n");
    }
    
    if (signal(SIGTERM, signal_handler) == SIG_ERR) {
        fprintf(stderr, "Couldn't install signal handler for SIGTERM\n");
    }
    
    /* Do loop */
    monitor_run(monitor);
    
    /* Free object */
    monitor_destroy(monitor);
    
    /* Deactivate alarm */
    alarm(0);
    
    return 0;
}
