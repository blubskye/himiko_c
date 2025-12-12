/*
 * Himiko Discord Bot (C Edition) - Fun Commands
 * Copyright (C) 2025 Himiko Contributors
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "commands/fun.h"
#include "bot.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* 8ball responses */
static const char *eightball_responses[] = {
    "It is certain.",
    "It is decidedly so.",
    "Without a doubt.",
    "Yes definitely.",
    "You may rely on it.",
    "As I see it, yes.",
    "Most likely.",
    "Outlook good.",
    "Yes.",
    "Signs point to yes.",
    "Reply hazy, try again.",
    "Ask again later.",
    "Better not tell you now.",
    "Cannot predict now.",
    "Concentrate and ask again.",
    "Don't count on it.",
    "My reply is no.",
    "My sources say no.",
    "Outlook not so good.",
    "Very doubtful."
};
static const int eightball_count = sizeof(eightball_responses) / sizeof(eightball_responses[0]);

/* RPS choices */
static const char *rps_choices[] = { "rock", "paper", "scissors" };

/* Initialize random seed */
static int random_initialized = 0;
static void init_random(void) {
    if (!random_initialized) {
        srand((unsigned int)time(NULL));
        random_initialized = 1;
    }
}

void cmd_8ball(struct discord *client, const struct discord_interaction *interaction) {
    struct discord_application_command_interaction_data_options *opts = interaction->data->options;
    const char *question = NULL;

    if (opts) {
        for (int i = 0; i < opts->size; i++) {
            if (strcmp(opts->array[i].name, "question") == 0) {
                question = opts->array[i].value;
            }
        }
    }

    if (!question || !*question) {
        respond_ephemeral(client, interaction, "Please ask a question!");
        return;
    }

    init_random();
    const char *answer = eightball_responses[rand() % eightball_count];

    char response[512];
    snprintf(response, sizeof(response),
        ":8ball: **Magic 8-Ball**\n\n"
        "**Question:** %s\n"
        "**Answer:** %s",
        question, answer);

    respond_message(client, interaction, response);
}

void cmd_8ball_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    if (!args || !*args) {
        struct discord_create_message params = { .content = "Please ask a question! Usage: 8ball <question>" };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    init_random();
    const char *answer = eightball_responses[rand() % eightball_count];

    char response[512];
    snprintf(response, sizeof(response),
        ":8ball: **Magic 8-Ball**\n\n"
        "**Question:** %s\n"
        "**Answer:** %s",
        args, answer);

    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_dice(struct discord *client, const struct discord_interaction *interaction) {
    struct discord_application_command_interaction_data_options *opts = interaction->data->options;
    int sides = 6;

    if (opts) {
        for (int i = 0; i < opts->size; i++) {
            if (strcmp(opts->array[i].name, "sides") == 0) {
                sides = (int)strtol(opts->array[i].value, NULL, 10);
            }
        }
    }

    if (sides < 2) sides = 2;
    if (sides > 100) sides = 100;

    init_random();
    int result = (rand() % sides) + 1;

    char response[128];
    snprintf(response, sizeof(response),
        ":game_die: You rolled a **%d** (d%d)",
        result, sides);

    respond_message(client, interaction, response);
}

void cmd_dice_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    int sides = 6;

    if (args && *args) {
        sides = (int)strtol(args, NULL, 10);
    }

    if (sides < 2) sides = 2;
    if (sides > 100) sides = 100;

    init_random();
    int result = (rand() % sides) + 1;

    char response[128];
    snprintf(response, sizeof(response),
        ":game_die: You rolled a **%d** (d%d)",
        result, sides);

    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_coinflip(struct discord *client, const struct discord_interaction *interaction) {
    init_random();
    const char *result = (rand() % 2 == 0) ? "Heads" : "Tails";

    char response[64];
    snprintf(response, sizeof(response), ":coin: **%s!**", result);

    respond_message(client, interaction, response);
}

void cmd_coinflip_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    (void)args;

    init_random();
    const char *result = (rand() % 2 == 0) ? "Heads" : "Tails";

    char response[64];
    snprintf(response, sizeof(response), ":coin: **%s!**", result);

    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_rps(struct discord *client, const struct discord_interaction *interaction) {
    struct discord_application_command_interaction_data_options *opts = interaction->data->options;
    const char *choice = NULL;

    if (opts) {
        for (int i = 0; i < opts->size; i++) {
            if (strcmp(opts->array[i].name, "choice") == 0) {
                choice = opts->array[i].value;
            }
        }
    }

    if (!choice || !*choice) {
        respond_ephemeral(client, interaction, "Choose rock, paper, or scissors!");
        return;
    }

    /* Normalize choice */
    int user_choice = -1;
    if (strncasecmp(choice, "rock", 4) == 0 || choice[0] == 'r' || choice[0] == 'R') {
        user_choice = 0;
    } else if (strncasecmp(choice, "paper", 5) == 0 || choice[0] == 'p' || choice[0] == 'P') {
        user_choice = 1;
    } else if (strncasecmp(choice, "scissors", 8) == 0 || choice[0] == 's' || choice[0] == 'S') {
        user_choice = 2;
    }

    if (user_choice < 0) {
        respond_ephemeral(client, interaction, "Invalid choice! Use rock, paper, or scissors.");
        return;
    }

    init_random();
    int bot_choice = rand() % 3;

    const char *result;
    if (user_choice == bot_choice) {
        result = "It's a **tie**!";
    } else if ((user_choice == 0 && bot_choice == 2) ||
               (user_choice == 1 && bot_choice == 0) ||
               (user_choice == 2 && bot_choice == 1)) {
        result = "You **win**!";
    } else {
        result = "I **win**!";
    }

    char response[256];
    snprintf(response, sizeof(response),
        "**Rock Paper Scissors**\n\n"
        "You chose: **%s**\n"
        "I chose: **%s**\n\n"
        "%s",
        rps_choices[user_choice], rps_choices[bot_choice], result);

    respond_message(client, interaction, response);
}

void cmd_rps_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    if (!args || !*args) {
        struct discord_create_message params = { .content = "Usage: rps <rock|paper|scissors>" };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    int user_choice = -1;
    if (strncasecmp(args, "rock", 4) == 0 || args[0] == 'r' || args[0] == 'R') {
        user_choice = 0;
    } else if (strncasecmp(args, "paper", 5) == 0 || args[0] == 'p' || args[0] == 'P') {
        user_choice = 1;
    } else if (strncasecmp(args, "scissors", 8) == 0 || args[0] == 's' || args[0] == 'S') {
        user_choice = 2;
    }

    if (user_choice < 0) {
        struct discord_create_message params = { .content = "Invalid choice! Use rock, paper, or scissors." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    init_random();
    int bot_choice = rand() % 3;

    const char *result;
    if (user_choice == bot_choice) {
        result = "It's a **tie**!";
    } else if ((user_choice == 0 && bot_choice == 2) ||
               (user_choice == 1 && bot_choice == 0) ||
               (user_choice == 2 && bot_choice == 1)) {
        result = "You **win**!";
    } else {
        result = "I **win**!";
    }

    char response[256];
    snprintf(response, sizeof(response),
        "**Rock Paper Scissors**\n\n"
        "You chose: **%s**\n"
        "I chose: **%s**\n\n"
        "%s",
        rps_choices[user_choice], rps_choices[bot_choice], result);

    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_rate(struct discord *client, const struct discord_interaction *interaction) {
    struct discord_application_command_interaction_data_options *opts = interaction->data->options;
    const char *thing = NULL;

    if (opts) {
        for (int i = 0; i < opts->size; i++) {
            if (strcmp(opts->array[i].name, "thing") == 0) {
                thing = opts->array[i].value;
            }
        }
    }

    if (!thing || !*thing) {
        respond_ephemeral(client, interaction, "Please specify something to rate!");
        return;
    }

    init_random();
    int rating = rand() % 11; /* 0-10 */

    char response[256];
    snprintf(response, sizeof(response),
        "I rate **%s** a **%d/10**!",
        thing, rating);

    respond_message(client, interaction, response);
}

void cmd_rate_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    if (!args || !*args) {
        struct discord_create_message params = { .content = "Usage: rate <thing>" };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    init_random();
    int rating = rand() % 11;

    char response[256];
    snprintf(response, sizeof(response),
        "I rate **%s** a **%d/10**!",
        args, rating);

    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void cmd_choose(struct discord *client, const struct discord_interaction *interaction) {
    struct discord_application_command_interaction_data_options *opts = interaction->data->options;
    const char *options = NULL;

    if (opts) {
        for (int i = 0; i < opts->size; i++) {
            if (strcmp(opts->array[i].name, "options") == 0) {
                options = opts->array[i].value;
            }
        }
    }

    if (!options || !*options) {
        respond_ephemeral(client, interaction, "Please provide options separated by commas or |");
        return;
    }

    /* Parse options */
    char options_copy[1024];
    strncpy(options_copy, options, sizeof(options_copy) - 1);
    options_copy[sizeof(options_copy) - 1] = '\0';

    char *choices[50];
    int choice_count = 0;

    /* Try splitting by | first, then by comma */
    char *delim = strchr(options_copy, '|') ? "|" : ",";
    char *token = strtok(options_copy, delim);
    while (token && choice_count < 50) {
        /* Trim whitespace */
        while (*token == ' ') token++;
        char *end = token + strlen(token) - 1;
        while (end > token && *end == ' ') *end-- = '\0';

        if (*token) {
            choices[choice_count++] = token;
        }
        token = strtok(NULL, delim);
    }

    if (choice_count < 2) {
        respond_ephemeral(client, interaction, "Please provide at least 2 options!");
        return;
    }

    init_random();
    const char *chosen = choices[rand() % choice_count];

    char response[512];
    snprintf(response, sizeof(response),
        ":thinking: I choose... **%s**!",
        chosen);

    respond_message(client, interaction, response);
}

void cmd_choose_prefix(struct discord *client, const struct discord_message *msg, const char *args) {
    if (!args || !*args) {
        struct discord_create_message params = { .content = "Usage: choose <option1> | <option2> | ..." };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    char options_copy[1024];
    strncpy(options_copy, args, sizeof(options_copy) - 1);
    options_copy[sizeof(options_copy) - 1] = '\0';

    char *choices[50];
    int choice_count = 0;

    char *delim = strchr(options_copy, '|') ? "|" : ",";
    char *token = strtok(options_copy, delim);
    while (token && choice_count < 50) {
        while (*token == ' ') token++;
        char *end = token + strlen(token) - 1;
        while (end > token && *end == ' ') *end-- = '\0';

        if (*token) {
            choices[choice_count++] = token;
        }
        token = strtok(NULL, delim);
    }

    if (choice_count < 2) {
        struct discord_create_message params = { .content = "Please provide at least 2 options!" };
        discord_create_message(client, msg->channel_id, &params, NULL);
        return;
    }

    init_random();
    const char *chosen = choices[rand() % choice_count];

    char response[512];
    snprintf(response, sizeof(response),
        ":thinking: I choose... **%s**!",
        chosen);

    struct discord_create_message params = { .content = response };
    discord_create_message(client, msg->channel_id, &params, NULL);
}

void register_fun_commands(himiko_bot_t *bot) {
    himiko_command_t cmds[] = {
        { "8ball", "Ask the magic 8-ball a question", "Fun", cmd_8ball, cmd_8ball_prefix, 0, 0 },
        { "dice", "Roll a dice", "Fun", cmd_dice, cmd_dice_prefix, 0, 0 },
        { "coinflip", "Flip a coin", "Fun", cmd_coinflip, cmd_coinflip_prefix, 0, 0 },
        { "rps", "Play rock paper scissors", "Fun", cmd_rps, cmd_rps_prefix, 0, 0 },
        { "rate", "Rate something out of 10", "Fun", cmd_rate, cmd_rate_prefix, 0, 0 },
        { "choose", "Choose between options", "Fun", cmd_choose, cmd_choose_prefix, 0, 0 },
    };

    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
        bot_register_command(bot, &cmds[i]);
    }
}
