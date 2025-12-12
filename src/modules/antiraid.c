/*
 * Himiko Discord Bot (C Edition) - Anti-Raid Module
 * Copyright (C) 2025 Himiko Contributors
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "modules/antiraid.h"
#include "bot.h"
#include "database.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Discord epoch (2015-01-01 00:00:00 UTC) in milliseconds */
#define DISCORD_EPOCH 1420070400000LL

/* Global raid tracker */
static raid_tracker_t g_raid_tracker;

/* Convert snowflake to timestamp in milliseconds */
int64_t snowflake_to_timestamp_ms(u64snowflake id) {
    return ((int64_t)(id >> 22)) + DISCORD_EPOCH;
}

/* Get or create guild state */
static raid_guild_state_t *get_guild_state(const char *guild_id) {
    pthread_mutex_lock(&g_raid_tracker.mutex);

    for (int i = 0; i < g_raid_tracker.guild_count; i++) {
        if (strcmp(g_raid_tracker.guilds[i].guild_id, guild_id) == 0) {
            pthread_mutex_unlock(&g_raid_tracker.mutex);
            return &g_raid_tracker.guilds[i];
        }
    }

    /* Create new state if room */
    if (g_raid_tracker.guild_count < 100) {
        raid_guild_state_t *state = &g_raid_tracker.guilds[g_raid_tracker.guild_count++];
        memset(state, 0, sizeof(*state));
        strncpy(state->guild_id, guild_id, sizeof(state->guild_id) - 1);
        pthread_mutex_unlock(&g_raid_tracker.mutex);
        return state;
    }

    pthread_mutex_unlock(&g_raid_tracker.mutex);
    return NULL;
}

/* Auto-silence mode labels */
static const char *autosilence_label(int mode) {
    switch (mode) {
        case -2: return "Log only";
        case -1: return "Alert on joins";
        case 0:  return "Off";
        case 1:  return "Raid mode";
        case 2:  return "All joins";
        default: return "Unknown";
    }
}

/* Format duration in human-readable form */
static void format_duration(int64_t ms, char *buf, size_t buf_size) {
    int64_t secs = ms / 1000;
    int64_t mins = secs / 60;
    int64_t hours = mins / 60;
    int64_t days = hours / 24;

    if (days > 0) {
        snprintf(buf, buf_size, "%ldd %lldh", (long long)days, (long long)(hours % 24));
    } else if (hours > 0) {
        snprintf(buf, buf_size, "%lldh %lldm", (long long)hours, (long long)(mins % 60));
    } else if (mins > 0) {
        snprintf(buf, buf_size, "%lldm %llds", (long long)mins, (long long)(secs % 60));
    } else {
        snprintf(buf, buf_size, "%llds", (long long)secs);
    }
}

void antiraid_init(himiko_bot_t *bot) {
    (void)bot;
    memset(&g_raid_tracker, 0, sizeof(g_raid_tracker));
    pthread_mutex_init(&g_raid_tracker.mutex, NULL);
}

void antiraid_cleanup(himiko_bot_t *bot) {
    (void)bot;
    pthread_mutex_destroy(&g_raid_tracker.mutex);
}

/* Check for raid and handle it */
static int check_for_raid(struct discord *client, const char *guild_id, antiraid_config_t *cfg, time_t now) {
    int64_t since_time = (now - cfg->raid_time) * 1000LL; /* Convert to ms */
    int count = 0;

    db_count_recent_joins(&g_bot->database, guild_id, since_time, &count);

    if (count >= cfg->raid_size) {
        raid_guild_state_t *state = get_guild_state(guild_id);
        if (!state) return 1;

        pthread_mutex_lock(&g_raid_tracker.mutex);
        time_t last_alert = state->last_raid_alert;
        int alert_cooldown = cfg->raid_time * 2;

        if (now - last_alert > alert_cooldown) {
            state->last_raid_alert = now;
            pthread_mutex_unlock(&g_raid_tracker.mutex);

            /* Raid detected - send alert */
            if (cfg->log_channel_id[0]) {
                char alert_text[512];
                int offset = 0;

                if (cfg->alert_role_id[0]) {
                    offset = snprintf(alert_text, sizeof(alert_text), "<@&%s> ", cfg->alert_role_id);
                }

                char msg[1024];
                snprintf(msg, sizeof(msg),
                    "%s**RAID DETECTED**\n\n"
                    "%d users joined in the past %d seconds!\n"
                    "Action: %s\n"
                    "Auto-Silence: %s",
                    alert_text,
                    count, cfg->raid_time,
                    cfg->action, autosilence_label(cfg->auto_silence));

                u64snowflake channel_id = strtoull(cfg->log_channel_id, NULL, 10);
                struct discord_create_message params = { .content = msg };
                discord_create_message(client, channel_id, &params, NULL);
            }

            /* Auto-lockdown if configured */
            if (cfg->lockdown_duration > 0 && state) {
                pthread_mutex_lock(&g_raid_tracker.mutex);
                if (!state->in_lockdown) {
                    state->in_lockdown = 1;
                    state->lockdown_start = now;
                    pthread_mutex_unlock(&g_raid_tracker.mutex);

                    /* Raise verification level */
                    u64snowflake gid = strtoull(guild_id, NULL, 10);
                    struct discord_modify_guild params = {
                        .verification_level = DISCORD_VERIFICATION_HIGH
                    };
                    discord_modify_guild(client, gid, &params, NULL);

                    if (cfg->log_channel_id[0]) {
                        char lockdown_msg[256];
                        snprintf(lockdown_msg, sizeof(lockdown_msg),
                            "**Server Lockdown Enabled**\n"
                            "Verification level raised to **High** for %d seconds",
                            cfg->lockdown_duration);

                        u64snowflake channel_id = strtoull(cfg->log_channel_id, NULL, 10);
                        struct discord_create_message params = { .content = lockdown_msg };
                        discord_create_message(client, channel_id, &params, NULL);
                    }
                } else {
                    pthread_mutex_unlock(&g_raid_tracker.mutex);
                }
            }

            return 1;
        }
        pthread_mutex_unlock(&g_raid_tracker.mutex);
        return 1; /* Still in raid window */
    }

    return 0;
}

/* Silence a member */
static void silence_member(struct discord *client, const char *guild_id, const char *user_id, antiraid_config_t *cfg) {
    if (!cfg->silent_role_id[0]) return;

    u64snowflake gid = strtoull(guild_id, NULL, 10);
    u64snowflake uid = strtoull(user_id, NULL, 10);
    u64snowflake role_id = strtoull(cfg->silent_role_id, NULL, 10);

    if (strcmp(cfg->action, "silence") == 0) {
        discord_add_guild_member_role(client, gid, uid, role_id, NULL, NULL);
    } else if (strcmp(cfg->action, "kick") == 0) {
        discord_remove_guild_member(client, gid, uid, NULL, NULL);
    } else if (strcmp(cfg->action, "ban") == 0) {
        struct discord_create_guild_ban params = {
            .delete_message_days = 1
        };
        discord_create_guild_ban(client, gid, uid, &params, NULL);
    }
}

void antiraid_on_member_join(struct discord *client, u64snowflake guild_id, const struct discord_user *user) {
    char guild_id_str[32], user_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)guild_id);
    snprintf(user_id_str, sizeof(user_id_str), "%lu", (unsigned long)user->id);

    antiraid_config_t cfg;
    if (db_get_antiraid_config(&g_bot->database, guild_id_str, &cfg) != 0 || !cfg.enabled) {
        return;
    }

    time_t now = time(NULL);
    int64_t now_ms = now * 1000LL;
    int64_t account_created = snowflake_to_timestamp_ms(user->id);

    /* Record the join */
    db_record_member_join(&g_bot->database, guild_id_str, user_id_str, now_ms, account_created);

    /* Handle based on auto-silence mode */
    switch (cfg.auto_silence) {
        case -2: /* Log only */
            if (cfg.log_channel_id[0]) {
                int64_t age_ms = now_ms - account_created;
                char age_str[32];
                format_duration(age_ms, age_str, sizeof(age_str));

                char msg[512];
                snprintf(msg, sizeof(msg),
                    "**Member Joined**\n<@%s> joined the server\nAccount Age: %s",
                    user_id_str, age_str);

                u64snowflake channel_id = strtoull(cfg.log_channel_id, NULL, 10);
                struct discord_create_message params = { .content = msg };
                discord_create_message(client, channel_id, &params, NULL);
            }
            break;

        case -1: /* Alert on all joins */
            if (cfg.log_channel_id[0]) {
                int64_t age_ms = now_ms - account_created;
                char age_str[32];
                format_duration(age_ms, age_str, sizeof(age_str));

                const char *warning = "";
                if (age_ms < 24 * 60 * 60 * 1000LL) {
                    warning = " **[NEW ACCOUNT]**";
                } else if (age_ms < 7 * 24 * 60 * 60 * 1000LL) {
                    warning = " [Recent Account]";
                }

                char alert_text[64] = "";
                if (cfg.alert_role_id[0]) {
                    snprintf(alert_text, sizeof(alert_text), "<@&%s> ", cfg.alert_role_id);
                }

                char msg[512];
                snprintf(msg, sizeof(msg),
                    "%s**Member Joined%s**\n<@%s> joined the server\nAccount Age: %s\nUser ID: %s",
                    alert_text, warning, user_id_str, age_str, user_id_str);

                u64snowflake channel_id = strtoull(cfg.log_channel_id, NULL, 10);
                struct discord_create_message params = { .content = msg };
                discord_create_message(client, channel_id, &params, NULL);
            }
            break;

        case 0: /* Off - just check for raid */
            check_for_raid(client, guild_id_str, &cfg, now);
            break;

        case 1: /* Raid mode - silence if raid detected */
            if (check_for_raid(client, guild_id_str, &cfg, now)) {
                silence_member(client, guild_id_str, user_id_str, &cfg);
            }
            break;

        case 2: /* All mode - silence everyone */
            silence_member(client, guild_id_str, user_id_str, &cfg);
            if (cfg.log_channel_id[0]) {
                int64_t age_ms = now_ms - account_created;
                char age_str[32];
                format_duration(age_ms, age_str, sizeof(age_str));

                char alert_text[64] = "";
                if (cfg.alert_role_id[0]) {
                    snprintf(alert_text, sizeof(alert_text), "<@&%s> ", cfg.alert_role_id);
                }

                char msg[512];
                snprintf(msg, sizeof(msg),
                    "%s**Member Joined & Silenced**\n<@%s>\nAccount Age: %s",
                    alert_text, user_id_str, age_str);

                u64snowflake channel_id = strtoull(cfg.log_channel_id, NULL, 10);
                struct discord_create_message params = { .content = msg };
                discord_create_message(client, channel_id, &params, NULL);
            }
            break;
    }
}

void antiraid_check_lockdown_expiry(struct discord *client) {
    time_t now = time(NULL);

    pthread_mutex_lock(&g_raid_tracker.mutex);
    for (int i = 0; i < g_raid_tracker.guild_count; i++) {
        raid_guild_state_t *state = &g_raid_tracker.guilds[i];
        if (!state->in_lockdown) continue;

        antiraid_config_t cfg;
        if (db_get_antiraid_config(&g_bot->database, state->guild_id, &cfg) != 0) {
            continue;
        }

        if (now - state->lockdown_start > cfg.lockdown_duration) {
            state->in_lockdown = 0;

            /* Lower verification level */
            u64snowflake gid = strtoull(state->guild_id, NULL, 10);
            struct discord_modify_guild params = {
                .verification_level = DISCORD_VERIFICATION_MEDIUM
            };
            discord_modify_guild(client, gid, &params, NULL);

            if (cfg.log_channel_id[0]) {
                u64snowflake channel_id = strtoull(cfg.log_channel_id, NULL, 10);
                struct discord_create_message params = {
                    .content = "**Lockdown Expired**\nServer verification level restored to **Medium**"
                };
                discord_create_message(client, channel_id, &params, NULL);
            }
        }
    }
    pthread_mutex_unlock(&g_raid_tracker.mutex);
}

/* Command: antiraid status/enable/disable/set */
void cmd_antiraid(struct discord *client, const struct discord_interaction *interaction) {
    /* This is a complex slash command with subcommands - simplified version */
    char guild_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)interaction->guild_id);

    antiraid_config_t cfg;
    if (db_get_antiraid_config(&g_bot->database, guild_id_str, &cfg) != 0) {
        memset(&cfg, 0, sizeof(cfg));
        strncpy(cfg.guild_id, guild_id_str, sizeof(cfg.guild_id) - 1);
        strncpy(cfg.action, "silence", sizeof(cfg.action) - 1);
        cfg.raid_time = 300;
        cfg.raid_size = 5;
        cfg.lockdown_duration = 120;
    }

    /* Build status response */
    char response[2048];
    snprintf(response, sizeof(response),
        "**Anti-Raid Configuration**\n\n"
        "**Status:** %s\n"
        "**Action:** %s\n"
        "**Auto-Silence:** %s\n"
        "**Raid Time:** %d seconds\n"
        "**Raid Size:** %d users\n"
        "**Lockdown Duration:** %d seconds\n"
        "**Silent Role:** %s\n"
        "**Alert Channel:** %s\n"
        "**Alert Role:** %s\n\n"
        "Use prefix commands to configure:\n"
        "`antiraid enable/disable`\n"
        "`antiraid set <setting> <value>`\n"
        "`antiraid setrole <@role>`\n"
        "`antiraid setalert <#channel> [@role]`\n"
        "`antiraid autosilence <mode>`",
        cfg.enabled ? "Enabled" : "Disabled",
        cfg.action,
        autosilence_label(cfg.auto_silence),
        cfg.raid_time,
        cfg.raid_size,
        cfg.lockdown_duration,
        cfg.silent_role_id[0] ? cfg.silent_role_id : "Not set",
        cfg.log_channel_id[0] ? cfg.log_channel_id : "Not set",
        cfg.alert_role_id[0] ? cfg.alert_role_id : "Not set");

    respond_message(client, interaction, response);
}

void cmd_antiraid_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    char guild_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)msg->guild_id);

    antiraid_config_t cfg;
    if (db_get_antiraid_config(&g_bot->database, guild_id_str, &cfg) != 0) {
        memset(&cfg, 0, sizeof(cfg));
        strncpy(cfg.guild_id, guild_id_str, sizeof(cfg.guild_id) - 1);
        strncpy(cfg.action, "silence", sizeof(cfg.action) - 1);
        cfg.raid_time = 300;
        cfg.raid_size = 5;
        cfg.lockdown_duration = 120;
    }

    if (!args || !*args || strcmp(args, "status") == 0) {
        /* Show status */
        char response[2048];
        snprintf(response, sizeof(response),
            "**Anti-Raid Configuration**\n\n"
            "**Status:** %s\n"
            "**Action:** %s\n"
            "**Auto-Silence:** %s\n"
            "**Raid Time:** %d seconds\n"
            "**Raid Size:** %d users\n"
            "**Lockdown Duration:** %d seconds\n"
            "**Silent Role:** %s\n"
            "**Alert Channel:** %s\n"
            "**Alert Role:** %s",
            cfg.enabled ? "Enabled" : "Disabled",
            cfg.action,
            autosilence_label(cfg.auto_silence),
            cfg.raid_time,
            cfg.raid_size,
            cfg.lockdown_duration,
            cfg.silent_role_id[0] ? cfg.silent_role_id : "Not set",
            cfg.log_channel_id[0] ? cfg.log_channel_id : "Not set",
            cfg.alert_role_id[0] ? cfg.alert_role_id : "Not set");

        struct discord_create_message params = { .content = response };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    if (strcmp(args, "enable") == 0) {
        cfg.enabled = 1;
        db_set_antiraid_config(&g_bot->database, &cfg);
        struct discord_create_message params = { .content = "Anti-raid protection **enabled**." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    if (strcmp(args, "disable") == 0) {
        cfg.enabled = 0;
        db_set_antiraid_config(&g_bot->database, &cfg);
        struct discord_create_message params = { .content = "Anti-raid protection **disabled**." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    if (strncmp(args, "setrole ", 8) == 0) {
        u64snowflake role_id = parse_role_mention(args + 8);
        if (role_id == 0) {
            struct discord_create_message params = { .content = "Please mention a valid role." };
            discord_create_message(client, msg->channel_id, &params, NULL);
            return;
        }
        snprintf(cfg.silent_role_id, sizeof(cfg.silent_role_id), "%lu", (unsigned long)role_id);
        db_set_antiraid_config(&g_bot->database, &cfg);

        char response[128];
        snprintf(response, sizeof(response), "Silent role set to <@&%lu>", (unsigned long)role_id);
        struct discord_create_message params = { .content = response };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    if (strncmp(args, "setalert ", 9) == 0) {
        u64snowflake channel_id = parse_channel_mention(args + 9);
        if (channel_id == 0) {
            struct discord_create_message params = { .content = "Please mention a valid channel." };
            discord_create_message(client, msg->channel_id, &params, NULL);
            return;
        }
        snprintf(cfg.log_channel_id, sizeof(cfg.log_channel_id), "%lu", (unsigned long)channel_id);

        /* Check for optional role */
        const char *role_start = strstr(args + 9, "<@&");
        if (role_start) {
            u64snowflake role_id = parse_role_mention(role_start);
            if (role_id != 0) {
                snprintf(cfg.alert_role_id, sizeof(cfg.alert_role_id), "%lu", (unsigned long)role_id);
            }
        }

        db_set_antiraid_config(&g_bot->database, &cfg);

        char response[256];
        snprintf(response, sizeof(response), "Alert channel set to <#%lu>", (unsigned long)channel_id);
        struct discord_create_message params = { .content = response };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    if (strncmp(args, "autosilence ", 12) == 0) {
        const char *mode = args + 12;
        if (strcmp(mode, "off") == 0) cfg.auto_silence = 0;
        else if (strcmp(mode, "log") == 0) cfg.auto_silence = -2;
        else if (strcmp(mode, "alert") == 0) cfg.auto_silence = -1;
        else if (strcmp(mode, "raid") == 0) cfg.auto_silence = 1;
        else if (strcmp(mode, "all") == 0) cfg.auto_silence = 2;
        else {
            struct discord_create_message params = { .content = "Invalid mode. Use: off, log, alert, raid, all" };
            discord_create_message(client, msg->channel_id, &params, NULL);
            return;
        }
        db_set_antiraid_config(&g_bot->database, &cfg);

        char response[128];
        snprintf(response, sizeof(response), "Auto-silence mode set to **%s**", autosilence_label(cfg.auto_silence));
        struct discord_create_message params = { .content = response };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    if (strncmp(args, "set ", 4) == 0) {
        char setting[32];
        int value;
        if (sscanf(args + 4, "%31s %d", setting, &value) == 2) {
            if (strcmp(setting, "raidtime") == 0) {
                cfg.raid_time = value;
            } else if (strcmp(setting, "raidsize") == 0) {
                cfg.raid_size = value;
            } else if (strcmp(setting, "lockdown") == 0) {
                cfg.lockdown_duration = value;
            } else if (strcmp(setting, "action") == 0) {
                /* action is a string, handle separately */
            } else {
                struct discord_create_message params = { .content = "Unknown setting. Use: raidtime, raidsize, lockdown" };
                discord_create_message(client, msg->channel_id, &params, NULL);
                return;
            }
            db_set_antiraid_config(&g_bot->database, &cfg);

            char response[128];
            snprintf(response, sizeof(response), "Setting **%s** updated to **%d**", setting, value);
            struct discord_create_message params = { .content = response };
            discord_create_message(client, msg->channel_id, &params, NULL);
            return;
        }

        /* Check for action string */
        char action[32];
        if (sscanf(args + 4, "action %31s", action) == 1) {
            if (strcmp(action, "silence") == 0 || strcmp(action, "kick") == 0 || strcmp(action, "ban") == 0) {
                strncpy(cfg.action, action, sizeof(cfg.action) - 1);
                db_set_antiraid_config(&g_bot->database, &cfg);

                char response[128];
                snprintf(response, sizeof(response), "Action set to **%s**", action);
                struct discord_create_message params = { .content = response };
                discord_create_message(client, msg->channel_id, &params, NULL);
                return;
            }
        }
    }

    struct discord_create_message params = {
        .content = "Usage: antiraid <status|enable|disable|setrole|setalert|autosilence|set>"
    };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_silence(struct discord *client, const struct discord_interaction *interaction) {
    struct discord_application_command_interaction_data_options *opts = interaction->data->options;
    u64snowflake user_id = 0;

    if (opts) {
        for (int i = 0; i < opts->size; i++) {
            if (strcmp(opts->array[i].name, "user") == 0) {
                user_id = strtoll(opts->array[i].value, NULL, 10);
            }
        }
    }

    if (user_id == 0) {
        respond_ephemeral(client, interaction, "Please specify a user.");
        return;
    }

    char guild_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)interaction->guild_id);

    antiraid_config_t cfg;
    if (db_get_antiraid_config(&g_bot->database, guild_id_str, &cfg) != 0 || !cfg.silent_role_id[0]) {
        respond_ephemeral(client, interaction, "Silent role not configured. Use `/antiraid setrole` first.");
        return;
    }

    u64snowflake role_id = strtoull(cfg.silent_role_id, NULL, 10);
    discord_add_guild_member_role(client, interaction->guild_id, user_id, role_id, NULL, NULL);

    char response[128];
    snprintf(response, sizeof(response), "Silenced <@%lu>", (unsigned long)user_id);
    respond_message(client, interaction, response);
}

void cmd_silence_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    if (!args || !*args) {
        struct discord_create_message params = { .content = "Usage: silence <@user>" };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    u64snowflake user_id = parse_user_mention(args);
    if (user_id == 0) {
        struct discord_create_message params = { .content = "Please mention a valid user." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char guild_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)msg->guild_id);

    antiraid_config_t cfg;
    if (db_get_antiraid_config(&g_bot->database, guild_id_str, &cfg) != 0 || !cfg.silent_role_id[0]) {
        struct discord_create_message params = { .content = "Silent role not configured." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    u64snowflake role_id = strtoull(cfg.silent_role_id, NULL, 10);
    discord_add_guild_member_role(client, msg->guild_id, user_id, role_id, NULL, NULL);

    char response[128];
    snprintf(response, sizeof(response), "Silenced <@%lu>", (unsigned long)user_id);
    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_unsilence(struct discord *client, const struct discord_interaction *interaction) {
    struct discord_application_command_interaction_data_options *opts = interaction->data->options;
    u64snowflake user_id = 0;

    if (opts) {
        for (int i = 0; i < opts->size; i++) {
            if (strcmp(opts->array[i].name, "user") == 0) {
                user_id = strtoll(opts->array[i].value, NULL, 10);
            }
        }
    }

    if (user_id == 0) {
        respond_ephemeral(client, interaction, "Please specify a user.");
        return;
    }

    char guild_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)interaction->guild_id);

    antiraid_config_t cfg;
    if (db_get_antiraid_config(&g_bot->database, guild_id_str, &cfg) != 0 || !cfg.silent_role_id[0]) {
        respond_ephemeral(client, interaction, "Silent role not configured.");
        return;
    }

    u64snowflake role_id = strtoull(cfg.silent_role_id, NULL, 10);
    discord_remove_guild_member_role(client, interaction->guild_id, user_id, role_id, NULL, NULL);

    char response[128];
    snprintf(response, sizeof(response), "Unsilenced <@%lu>", (unsigned long)user_id);
    respond_message(client, interaction, response);
}

void cmd_unsilence_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    if (!args || !*args) {
        struct discord_create_message params = { .content = "Usage: unsilence <@user>" };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    u64snowflake user_id = parse_user_mention(args);
    if (user_id == 0) {
        struct discord_create_message params = { .content = "Please mention a valid user." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char guild_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)msg->guild_id);

    antiraid_config_t cfg;
    if (db_get_antiraid_config(&g_bot->database, guild_id_str, &cfg) != 0 || !cfg.silent_role_id[0]) {
        struct discord_create_message params = { .content = "Silent role not configured." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    u64snowflake role_id = strtoull(cfg.silent_role_id, NULL, 10);
    discord_remove_guild_member_role(client, msg->guild_id, user_id, role_id, NULL, NULL);

    char response[128];
    snprintf(response, sizeof(response), "Unsilenced <@%lu>", (unsigned long)user_id);
    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_getraid(struct discord *client, const struct discord_interaction *interaction) {
    respond_ephemeral(client, interaction, "Use the prefix command `getraid` for this feature.");
}

void cmd_getraid_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    (void)args;

    char guild_id_str[32];
    snprintf(guild_id_str, sizeof(guild_id_str), "%lu", (unsigned long)msg->guild_id);

    antiraid_config_t cfg;
    db_get_antiraid_config(&g_bot->database, guild_id_str, &cfg);

    /* This would need db_get_recent_joins implemented */
    struct discord_create_message params = {
        .content = "**Recent Joins**\nThis feature requires the db_get_recent_joins function to be implemented."
    };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_banraid(struct discord *client, const struct discord_interaction *interaction) {
    respond_ephemeral(client, interaction, "Use the prefix command `banraid` for this feature.");
}

void cmd_banraid_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    (void)args;

    struct discord_create_message params = {
        .content = "**Ban Raid Users**\nThis feature requires the db_get_recent_joins function to be implemented."
    };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_lockdown(struct discord *client, const struct discord_interaction *interaction) {
    struct discord_application_command_interaction_data_options *opts = interaction->data->options;
    int enable = 0;

    if (opts) {
        for (int i = 0; i < opts->size; i++) {
            if (strcmp(opts->array[i].name, "enable") == 0) {
                enable = strcmp(opts->array[i].value, "true") == 0;
            }
        }
    }

    if (enable) {
        struct discord_modify_guild params = {
            .verification_level = DISCORD_VERIFICATION_HIGH
        };
        discord_modify_guild(client, interaction->guild_id, &params, NULL);
        respond_message(client, interaction,
            "**Server Lockdown Enabled**\n"
            "Verification level raised to **High**\n"
            "New members must wait 10 minutes before chatting.");
    } else {
        struct discord_modify_guild params = {
            .verification_level = DISCORD_VERIFICATION_MEDIUM
        };
        discord_modify_guild(client, interaction->guild_id, &params, NULL);
        respond_message(client, interaction,
            "**Lockdown Disabled**\n"
            "Verification level restored to **Medium**");
    }
}

void cmd_lockdown_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    if (!args || !*args) {
        struct discord_create_message params = { .content = "Usage: lockdown <on|off>" };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    if (strcmp(args, "on") == 0 || strcmp(args, "enable") == 0) {
        struct discord_modify_guild params = {
            .verification_level = DISCORD_VERIFICATION_HIGH
        };
        discord_modify_guild(client, msg->guild_id, &params, NULL);

        struct discord_create_message msg_params = {
            .content = "**Server Lockdown Enabled**\nVerification level raised to **High**"
        };
        discord_create_message(client, msg->channel_id, &msg_params, NULL);
    } else if (strcmp(args, "off") == 0 || strcmp(args, "disable") == 0) {
        struct discord_modify_guild params = {
            .verification_level = DISCORD_VERIFICATION_MEDIUM
        };
        discord_modify_guild(client, msg->guild_id, &params, NULL);

        struct discord_create_message msg_params = {
            .content = "**Lockdown Disabled**\nVerification level restored to **Medium**"
        };
        discord_create_message(client, msg->channel_id, &msg_params, NULL);
    } else {
        struct discord_create_message params = { .content = "Usage: lockdown <on|off>" };
        discord_create_message(client, msg->channel_id, &params, NULL);
    }
}

void register_antiraid_commands(himiko_bot_t *bot) {
    himiko_command_t cmds[] = {
        { "antiraid", "Configure anti-raid protection", "Anti-Raid", cmd_antiraid, cmd_antiraid_prefix, 0, 0 },
        { "silence", "Silence a user", "Anti-Raid", cmd_silence, cmd_silence_prefix, 0, 0 },
        { "unsilence", "Unsilence a user", "Anti-Raid", cmd_unsilence, cmd_unsilence_prefix, 0, 0 },
        { "getraid", "Get recent raid users", "Anti-Raid", cmd_getraid, cmd_getraid_prefix, 0, 0 },
        { "banraid", "Ban all raid users", "Anti-Raid", cmd_banraid, cmd_banraid_prefix, 0, 0 },
        { "lockdown", "Toggle server lockdown", "Anti-Raid", cmd_lockdown, cmd_lockdown_prefix, 0, 0 },
    };

    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
        bot_register_command(bot, &cmds[i]);
    }
}
