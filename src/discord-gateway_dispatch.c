#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "discord.h"
#include "discord-internal.h"

#define INIT(type, event_name)                                                \
    {                                                                         \
        sizeof(struct type),                                                  \
            (size_t(*)(jsmnf_pair *, const char *, void *))type##_from_jsmnf, \
            (void (*)(void *))type##_cleanup                                  \
    }

/** @brief Information for deserializing a Discord event */
static const struct {
    /** size of event's datatype */
    size_t size;
    /** event's payload deserializer */
    size_t (*from_jsmnf)(jsmnf_pair *, const char *, void *);
    /** event's cleanup */
    void (*cleanup)(void *);
} dispatch[] = {
    [DISCORD_EV_READY] =
        INIT(discord_ready, ready),
    [DISCORD_EV_APPLICATION_COMMAND_CREATE] =
        INIT(discord_application_command, application_command_create),
    [DISCORD_EV_APPLICATION_COMMAND_UPDATE] =
        INIT(discord_application_command, application_command_update),
    [DISCORD_EV_APPLICATION_COMMAND_DELETE] =
        INIT(discord_application_command, application_command_delete),
    [DISCORD_EV_CHANNEL_CREATE] =
        INIT(discord_channel, channel_create),
    [DISCORD_EV_CHANNEL_UPDATE] =
        INIT(discord_channel, channel_update),
    [DISCORD_EV_CHANNEL_DELETE] =
        INIT(discord_channel, channel_delete),
    [DISCORD_EV_CHANNEL_PINS_UPDATE] =
        INIT(discord_channel_pins_update, channel_pins_update),
    [DISCORD_EV_THREAD_CREATE] =
        INIT(discord_channel, thread_create),
    [DISCORD_EV_THREAD_UPDATE] =
        INIT(discord_channel, thread_update),
    [DISCORD_EV_THREAD_DELETE] =
        INIT(discord_channel, thread_delete),
    [DISCORD_EV_THREAD_LIST_SYNC] =
        INIT(discord_thread_list_sync, thread_list_sync),
    [DISCORD_EV_THREAD_MEMBER_UPDATE] =
        INIT(discord_thread_member, thread_member_update),
    [DISCORD_EV_THREAD_MEMBERS_UPDATE] =
        INIT(discord_thread_members_update, thread_members_update),
    [DISCORD_EV_GUILD_CREATE] =
        INIT(discord_guild, guild_create),
    [DISCORD_EV_GUILD_UPDATE] =
        INIT(discord_guild, guild_update),
    [DISCORD_EV_GUILD_DELETE] =
        INIT(discord_guild, guild_delete),
    [DISCORD_EV_GUILD_BAN_ADD] =
        INIT(discord_guild_ban_add, guild_ban_add),
    [DISCORD_EV_GUILD_BAN_REMOVE] =
        INIT(discord_guild_ban_remove, guild_ban_remove),
    [DISCORD_EV_GUILD_EMOJIS_UPDATE] =
        INIT(discord_guild_emojis_update, guild_emojis_update),
    [DISCORD_EV_GUILD_STICKERS_UPDATE] =
        INIT(discord_guild_stickers_update, guild_stickers_update),
    [DISCORD_EV_GUILD_INTEGRATIONS_UPDATE] =
        INIT(discord_guild_integrations_update, guild_integrations_update),
    [DISCORD_EV_GUILD_MEMBER_ADD] =
        INIT(discord_guild_member, guild_member_add),
    [DISCORD_EV_GUILD_MEMBER_UPDATE] =
        INIT(discord_guild_member_update, guild_member_update),
    [DISCORD_EV_GUILD_MEMBER_REMOVE] =
        INIT(discord_guild_member_remove, guild_member_remove),
    [DISCORD_EV_GUILD_ROLE_CREATE] =
        INIT(discord_guild_role_create, guild_role_create),
    [DISCORD_EV_GUILD_ROLE_UPDATE] =
        INIT(discord_guild_role_update, guild_role_update),
    [DISCORD_EV_GUILD_ROLE_DELETE] =
        INIT(discord_guild_role_delete, guild_role_delete),
    [DISCORD_EV_INTEGRATION_CREATE] =
        INIT(discord_integration, integration_create),
    [DISCORD_EV_INTEGRATION_UPDATE] =
        INIT(discord_integration, integration_update),
    [DISCORD_EV_INTEGRATION_DELETE] =
        INIT(discord_integration_delete, integration_delete),
    [DISCORD_EV_INTERACTION_CREATE] =
        INIT(discord_interaction, interaction_create),
    [DISCORD_EV_INVITE_CREATE] =
        INIT(discord_invite_create, invite_create),
    [DISCORD_EV_INVITE_DELETE] =
        INIT(discord_invite_delete, invite_delete),
    [DISCORD_EV_MESSAGE_CREATE] =
        INIT(discord_message, message_create),
    [DISCORD_EV_MESSAGE_UPDATE] =
        INIT(discord_message, message_update),
    [DISCORD_EV_MESSAGE_DELETE] =
        INIT(discord_message_delete, message_delete),
    [DISCORD_EV_MESSAGE_DELETE_BULK] =
        INIT(discord_message_delete_bulk, message_delete_bulk),
    [DISCORD_EV_MESSAGE_REACTION_ADD] =
        INIT(discord_message_reaction_add, message_reaction_add),
    [DISCORD_EV_MESSAGE_REACTION_REMOVE] =
        INIT(discord_message_reaction_remove, message_reaction_remove),
    [DISCORD_EV_MESSAGE_REACTION_REMOVE_ALL] =
        INIT(discord_message_reaction_remove_all, message_reaction_remove_all),
    [DISCORD_EV_MESSAGE_REACTION_REMOVE_EMOJI] =
        INIT(discord_message_reaction_remove_emoji,
                message_reaction_remove_emoji),
    [DISCORD_EV_PRESENCE_UPDATE] =
        INIT(discord_presence_update, presence_update),
    [DISCORD_EV_STAGE_INSTANCE_CREATE] =
        INIT(discord_stage_instance, stage_instance_create),
    [DISCORD_EV_STAGE_INSTANCE_UPDATE] =
        INIT(discord_stage_instance, stage_instance_update),
    [DISCORD_EV_STAGE_INSTANCE_DELETE] =
        INIT(discord_stage_instance, stage_instance_delete),
    [DISCORD_EV_TYPING_START] =
        INIT(discord_typing_start, typing_start),
    [DISCORD_EV_USER_UPDATE] =
        INIT(discord_user, user_update),
    [DISCORD_EV_VOICE_STATE_UPDATE] =
        INIT(discord_voice_state, voice_state_update),
    [DISCORD_EV_VOICE_SERVER_UPDATE] =
        INIT(discord_voice_server_update, voice_server_update),
    [DISCORD_EV_WEBHOOKS_UPDATE] =
        INIT(discord_webhooks_update, webhooks_update),
};

void
discord_gateway_dispatch(struct discord_gateway *gw)
{
    const enum discord_gateway_events event = gw->payload.event;
    struct discord *client = CLIENT(gw, gw);

    switch (event) {
    case DISCORD_EV_MESSAGE_CREATE:
        if (discord_message_commands_try_perform(&client->commands,
                                                 &gw->payload)) {
            return;
        }
    /* fall-through */
    default:
        if (gw->cbs[event]) {
            void *event_data = calloc(1, dispatch[event].size);

            dispatch[event].from_jsmnf(gw->payload.data, gw->payload.json,
                                       event_data);

            if (CCORD_UNAVAILABLE
                == discord_refcounter_incr(&client->refcounter, event_data))
            {
                discord_refcounter_add_internal(&client->refcounter,
                                                event_data,
                                                dispatch[event].cleanup, true);
            }
            gw->cbs[event](client, event_data);
            discord_refcounter_decr(&client->refcounter, event_data);
        }
        break;
    case DISCORD_EV_NONE:
        logconf_warn(
            &gw->conf,
            "Expected unimplemented GATEWAY_DISPATCH event (code: %d)", event);
        break;
    }
}

void
discord_gateway_send_identify(struct discord_gateway *gw,
                              struct discord_identify *identify)
{
    struct ws_info info = { 0 };
    char buf[1024];
    jsonb b;

    /* Ratelimit check */
    if (gw->timer->now - gw->timer->identify < 5) {
        ++gw->session->concurrent;
        VASSERT_S(gw->session->concurrent
                      < gw->session->start_limit.max_concurrency,
                  "Reach identify request threshold (%d every 5 seconds)",
                  gw->session->start_limit.max_concurrency);
    }
    else {
        gw->session->concurrent = 0;
    }

    jsonb_init(&b);
    jsonb_object(&b, buf, sizeof(buf));
    {
        jsonb_key(&b, buf, sizeof(buf), "op", 2);
        jsonb_number(&b, buf, sizeof(buf), 2);
        jsonb_key(&b, buf, sizeof(buf), "d", 1);
        discord_identify_to_jsonb(&b, buf, sizeof(buf), identify);
        jsonb_object_pop(&b, buf, sizeof(buf));
    }

    if (ws_send_text(gw->ws, &info, buf, b.pos)) {
        io_poller_curlm_enable_perform(CLIENT(gw, gw)->io_poller, gw->mhandle);
        logconf_info(
            &gw->conf,
            ANSICOLOR(
                "SEND",
                ANSI_FG_BRIGHT_GREEN) " IDENTIFY (%d bytes) [@@@_%zu_@@@]",
            b.pos, info.loginfo.counter + 1);
        /* get timestamp for this identify */
        gw->timer->identify = gw->timer->now;
    }
    else {
        logconf_info(
            &gw->conf,
            ANSICOLOR("FAIL SEND",
                      ANSI_FG_RED) " IDENTIFY (%d bytes) [@@@_%zu_@@@]",
            b.pos, info.loginfo.counter + 1);
    }
}

void
discord_gateway_send_resume(struct discord_gateway *gw,
                            struct discord_resume *event)
{
    struct ws_info info = { 0 };
    char buf[1024];
    jsonb b;

    /* reset */
    gw->session->status ^= DISCORD_SESSION_RESUMABLE;

    jsonb_init(&b);
    jsonb_object(&b, buf, sizeof(buf));
    {
        jsonb_key(&b, buf, sizeof(buf), "op", 2);
        jsonb_number(&b, buf, sizeof(buf), 6);
        jsonb_key(&b, buf, sizeof(buf), "d", 1);
        discord_resume_to_jsonb(&b, buf, sizeof(buf), event);
        jsonb_object_pop(&b, buf, sizeof(buf));
    }

    if (ws_send_text(gw->ws, &info, buf, b.pos)) {
        io_poller_curlm_enable_perform(CLIENT(gw, gw)->io_poller, gw->mhandle);
        logconf_info(
            &gw->conf,
            ANSICOLOR("SEND",
                      ANSI_FG_BRIGHT_GREEN) " RESUME (%d bytes) [@@@_%zu_@@@]",
            b.pos, info.loginfo.counter + 1);
    }
    else {
        logconf_info(&gw->conf,
                     ANSICOLOR("FAIL SEND",
                               ANSI_FG_RED) " RESUME (%d bytes) [@@@_%zu_@@@]",
                     b.pos, info.loginfo.counter + 1);
    }
}

/* send heartbeat pulse to websockets server in order
 *  to maintain connection alive */
void
discord_gateway_send_heartbeat(struct discord_gateway *gw, int seq)
{
    struct ws_info info = { 0 };
    char buf[64];
    jsonb b;

    jsonb_init(&b);
    jsonb_object(&b, buf, sizeof(buf));
    {
        jsonb_key(&b, buf, sizeof(buf), "op", 2);
        jsonb_number(&b, buf, sizeof(buf), 1);
        jsonb_key(&b, buf, sizeof(buf), "d", 1);
        jsonb_number(&b, buf, sizeof(buf), seq);
        jsonb_object_pop(&b, buf, sizeof(buf));
    }

    if (ws_send_text(gw->ws, &info, buf, b.pos)) {
        io_poller_curlm_enable_perform(CLIENT(gw, gw)->io_poller, gw->mhandle);
        logconf_info(
            &gw->conf,
            ANSICOLOR(
                "SEND",
                ANSI_FG_BRIGHT_GREEN) " HEARTBEAT (%d bytes) [@@@_%zu_@@@]",
            b.pos, info.loginfo.counter + 1);
        /* update heartbeat timestamp */
        gw->timer->hbeat = gw->timer->now;
    }
    else {
        logconf_info(
            &gw->conf,
            ANSICOLOR("FAIL SEND",
                      ANSI_FG_RED) " HEARTBEAT (%d bytes) [@@@_%zu_@@@]",
            b.pos, info.loginfo.counter + 1);
    }
}

void
discord_gateway_send_request_guild_members(
    struct discord_gateway *gw, struct discord_request_guild_members *event)
{
    struct ws_info info = { 0 };
    char buf[1024];
    jsonb b;

    jsonb_init(&b);
    jsonb_object(&b, buf, sizeof(buf));
    {
        jsonb_key(&b, buf, sizeof(buf), "op", 2);
        jsonb_number(&b, buf, sizeof(buf), 8);
        jsonb_key(&b, buf, sizeof(buf), "d", 1);
        discord_request_guild_members_to_jsonb(&b, buf, sizeof(buf), event);
        jsonb_object_pop(&b, buf, sizeof(buf));
    }

    if (ws_send_text(gw->ws, &info, buf, b.pos)) {
        io_poller_curlm_enable_perform(CLIENT(gw, gw)->io_poller, gw->mhandle);
        logconf_info(
            &gw->conf,
            ANSICOLOR("SEND", ANSI_FG_BRIGHT_GREEN) " REQUEST_GUILD_MEMBERS "
                                                    "(%d bytes) [@@@_%zu_@@@]",
            b.pos, info.loginfo.counter + 1);
        /* update heartbeat timestamp */
        gw->timer->hbeat = gw->timer->now;
    }
    else {
        logconf_info(
            &gw->conf,
            ANSICOLOR(
                "FAIL SEND",
                ANSI_FG_RED) " REQUEST_GUILD_MEMBERS (%d bytes) [@@@_%zu_@@@]",
            b.pos, info.loginfo.counter + 1);
    }
}

void
discord_gateway_send_update_voice_state(
    struct discord_gateway *gw, struct discord_update_voice_state *event)
{
    struct ws_info info = { 0 };
    char buf[256];
    jsonb b;

    jsonb_init(&b);
    jsonb_object(&b, buf, sizeof(buf));
    {
        jsonb_key(&b, buf, sizeof(buf), "op", 2);
        jsonb_number(&b, buf, sizeof(buf), 4);
        jsonb_key(&b, buf, sizeof(buf), "d", 1);
        discord_update_voice_state_to_jsonb(&b, buf, sizeof(buf), event);
        jsonb_object_pop(&b, buf, sizeof(buf));
    }

    if (ws_send_text(gw->ws, &info, buf, b.pos)) {
        io_poller_curlm_enable_perform(CLIENT(gw, gw)->io_poller, gw->mhandle);
        logconf_info(
            &gw->conf,
            ANSICOLOR(
                "SEND",
                ANSI_FG_BRIGHT_GREEN) " UPDATE_VOICE_STATE "
                                      "(%d bytes): %s channels [@@@_%zu_@@@]",
            b.pos, event->channel_id ? "join" : "leave",
            info.loginfo.counter + 1);

        /* update heartbeat timestamp */
        gw->timer->hbeat = gw->timer->now;
    }
    else {
        logconf_info(
            &gw->conf,
            ANSICOLOR(
                "FAIL SEND",
                ANSI_FG_RED) " UPDATE_VOICE_STATE (%d bytes) [@@@_%zu_@@@]",
            b.pos, info.loginfo.counter + 1);
    }
}

void
discord_gateway_send_presence_update(struct discord_gateway *gw,
                                     struct discord_presence_update *presence)
{
    struct ws_info info = { 0 };
    char buf[2048];
    jsonb b;

    if (!gw->session->is_ready) return;

    jsonb_init(&b);
    jsonb_object(&b, buf, sizeof(buf));
    {
        jsonb_key(&b, buf, sizeof(buf), "op", 2);
        jsonb_number(&b, buf, sizeof(buf), 3);
        jsonb_key(&b, buf, sizeof(buf), "d", 1);
        discord_presence_update_to_jsonb(&b, buf, sizeof(buf), presence);
        jsonb_object_pop(&b, buf, sizeof(buf));
    }

    if (ws_send_text(gw->ws, &info, buf, b.pos)) {
        io_poller_curlm_enable_perform(CLIENT(gw, gw)->io_poller, gw->mhandle);
        logconf_info(
            &gw->conf,
            ANSICOLOR("SEND", ANSI_FG_BRIGHT_GREEN) " PRESENCE UPDATE (%d "
                                                    "bytes) [@@@_%zu_@@@]",
            b.pos, info.loginfo.counter + 1);
    }
    else {
        logconf_error(
            &gw->conf,
            ANSICOLOR("FAIL SEND", ANSI_FG_RED) " PRESENCE UPDATE (%d "
                                                "bytes) [@@@_%zu_@@@]",
            b.pos, info.loginfo.counter + 1);
    }
}
