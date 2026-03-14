/*
 * Copyright (c) 2026 Lingao Meng
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * ZBot - Shell Commands
 *
 * All user-facing commands are under the "zbot" root command.
 *
 * Usage:
 *   zbot key <api-key>                -- Set API key (persisted to flash)
 *   zbot key-delete                   -- Delete API key from flash
 *   zbot host <hostname>              -- Set LLM endpoint host
 *   zbot path <path>                  -- Set LLM API path
 *   zbot model <model>                -- Set model name
 *   zbot provider <id>                -- Set X-Model-Provider-Id header
 *   zbot tls <on|off> [port]          -- Configure TLS
 *   zbot status                       -- Show current config status
 *   zbot wifi connect <ssid> [pass]   -- Connect to WiFi (saves credentials)
 *   zbot wifi disconnect              -- Disconnect from WiFi
 *   zbot wifi status                  -- Show saved SSID and connection state
 *   zbot chat <message>               -- Send a message to the agent
 *   zbot history                      -- Show conversation history
 *   zbot summary                      -- Show persisted NVS summary
 *   zbot clear                        -- Clear conversation history
 *   zbot wipe                         -- Wipe history + NVS summary
 *   zbot skill list                   -- List all registered skills
 *   zbot skill run <name> [arg]       -- Run a skill directly
 *   zbot tools                        -- List all available tools
 */

#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <zephyr/net/wifi_credentials.h>

#include "config.h"
#include "memory.h"
#include "agent.h"
#include "tools.h"
#include "skill.h"

LOG_MODULE_REGISTER(zbot_shell, LOG_LEVEL_INF);

/* ------------------------------------------------------------------ */
/* zbot key */
/* ------------------------------------------------------------------ */

static int cmd_key(const struct shell *sh, size_t argc, char **argv)
{
	int rc;

	if (argc < 2) {
		shell_print(sh, "Usage: zbot key <api-key>");
		return -EINVAL;
	}

	rc = config_set_api_key(argv[1]);
	if (rc == 0) {
		shell_print(sh, "API key set (%zu chars) and saved to flash.", strlen(argv[1]));
	} else {
		shell_error(sh, "Failed to set key: %d", rc);
	}
	return rc;
}

/* ------------------------------------------------------------------ */
/* zbot key-delete — remove API key from NVS flash */
/* ------------------------------------------------------------------ */

static int cmd_key_delete(const struct shell *sh, size_t argc, char **argv)
{
	int rc;

	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	rc = config_delete_api_key();
	shell_print(sh, rc == 0 ? "API key deleted from flash." : "No key stored.");
	return 0;
}

/* ------------------------------------------------------------------ */
/* zbot host */
/* ------------------------------------------------------------------ */

static int cmd_host(const struct shell *sh, size_t argc, char **argv)
{
	int rc;

	if (argc < 2) {
		shell_print(sh, "Usage: zbot host <hostname>");
		shell_print(sh, "Example: zbot host api.openai.com");
		return -EINVAL;
	}

	rc = config_set_endpoint_host(argv[1]);
	if (rc == 0) {
		shell_print(sh, "Host set: %s", argv[1]);
	} else {
		shell_error(sh, "Failed: %d", rc);
	}
	return rc;
}

/* ------------------------------------------------------------------ */
/* zbot path */
/* ------------------------------------------------------------------ */

static int cmd_path(const struct shell *sh, size_t argc, char **argv)
{
	int rc;

	if (argc < 2) {
		shell_print(sh, "Usage: zbot path <path>");
		shell_print(sh, "Example: zbot path /v1/chat/completions");
		return -EINVAL;
	}

	rc = config_set_endpoint_path(argv[1]);
	if (rc == 0) {
		shell_print(sh, "Path set: %s", argv[1]);
	} else {
		shell_error(sh, "Failed: %d", rc);
	}
	return rc;
}

/* ------------------------------------------------------------------ */
/* zbot model */
/* ------------------------------------------------------------------ */

static int cmd_model(const struct shell *sh, size_t argc, char **argv)
{
	int rc;

	if (argc < 2) {
		shell_print(sh, "Usage: zbot model <model-name>");
		shell_print(sh, "Example: zbot model gpt-4o-mini");
		return -EINVAL;
	}

	rc = config_set_model(argv[1]);
	if (rc == 0) {
		shell_print(sh, "Model set: %s", argv[1]);
	} else {
		shell_error(sh, "Failed: %d", rc);
	}
	return rc;
}

/* ------------------------------------------------------------------ */
/* zbot provider */
/* ------------------------------------------------------------------ */

static int cmd_provider(const struct shell *sh, size_t argc, char **argv)
{
	int rc;

	if (argc < 2) {
		shell_print(sh, "Usage: zbot provider <provider-id>");
		shell_print(sh, "Example: zbot provider azure_openai");
		shell_print(sh, "Sets the X-Model-Provider-Id request header.");
		return -EINVAL;
	}

	rc = config_set_provider_id(argv[1]);
	if (rc == 0) {
		shell_print(sh, "Provider ID set: %s", argv[1]);
	} else {
		shell_error(sh, "Failed: %d", rc);
	}
	return rc;
}

/* ------------------------------------------------------------------ */
/* zbot tls */
/* ------------------------------------------------------------------ */

static int cmd_tls(const struct shell *sh, size_t argc, char **argv)
{
	bool use_tls;
	uint16_t port;

	if (argc < 2) {
		shell_print(sh, "Usage: zbot tls <on|off> [port]");
		return -EINVAL;
	}

	use_tls = (strcmp(argv[1], "on") == 0 || strcmp(argv[1], "1") == 0 ||
		   strcmp(argv[1], "yes") == 0);

	port = use_tls ? 443 : 80;
	if (argc >= 3) {
		port = (uint16_t)strtoul(argv[2], NULL, 10);
	}

	int rc = config_set_tls(use_tls, port);

	if (rc == 0) {
		shell_print(sh, "TLS: %s, port: %u (saved to flash)", use_tls ? "on" : "off",
			    port);
	} else {
		shell_error(sh, "Failed to persist TLS config: %d", rc);
	}
	return rc;
}

/* ------------------------------------------------------------------ */
/* zbot status */
/* ------------------------------------------------------------------ */

static int cmd_status(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	config_print_status();
	shell_print(sh, "Agent busy: %s", agent_is_busy() ? "yes" : "no");
	return 0;
}

/* ------------------------------------------------------------------ */
/* zbot chat — runs agent on system workqueue to avoid shell stack OVF */
/* ------------------------------------------------------------------ */

struct chat_work_item {
        int rc;
	char input[AGENT_INPUT_MAX_LEN];
	char response[AGENT_OUTPUT_MAX_LEN];
	struct k_sem done;
};

static struct chat_work_item g_chat_work;

static void agent_response(int err, struct agent_response_ctx *ctx)
{
	struct chat_work_item *chat = ctx->user_data;

	chat->rc = err;
	k_sem_give(&chat->done);
}

static int cmd_chat(const struct shell *sh, size_t argc, char **argv)
{
	size_t pos;
        int rc;

	if (argc < 2) {
		shell_print(sh, "Usage: zbot chat <message>");
		return -EINVAL;
	}

	if (!config_has_api_key()) {
		shell_error(sh, "API key not set. Run: zbot key <your-api-key>");
		return -EACCES;
	}

	/* Concatenate all argv[1..] into a single message */
	pos = 0;
	for (int i = 1; i < argc && pos < sizeof(g_chat_work.input) - 1; i++) {
		size_t rem = sizeof(g_chat_work.input) - 1 - pos;
		size_t len = strlen(argv[i]);

		if (i > 1 && pos < sizeof(g_chat_work.input) - 2) {
			g_chat_work.input[pos++] = ' ';
		}

		if (len > rem) {
			len = rem;
		}

		memcpy(g_chat_work.input + pos, argv[i], len);
		pos += len;
	}

	g_chat_work.input[pos] = '\0';

	shell_print(sh, "You: %s", g_chat_work.input);
	shell_print(sh, "Thinking...");

	/* Submit to system workqueue (CONFIG_SYSTEM_WORKQUEUE_STACK_SIZE)
	 * so the heavy HTTP + JSON work runs off the shell stack. */
	k_sem_init(&g_chat_work.done, 0, 1);

	struct agent_response_ctx ctx = {
		.output        = g_chat_work.response,
		.output_length = sizeof(g_chat_work.response),
		.cb            = agent_response,
		.user_data     = &g_chat_work,
	};

	rc = agent_submit_input(g_chat_work.input, &ctx);
        if (rc) {
		shell_error(sh, "Agent submit input error");
		return -EACCES;
        }

	/* Block shell thread until workqueue finishes (timeout 60s) */
	if (k_sem_take(&g_chat_work.done, K_SECONDS(60)) != 0) {
		shell_error(sh, "Timeout waiting for agent response");
		return -ETIMEDOUT;
	}

	if (g_chat_work.rc < 0) {
		shell_error(sh, "Agent error: %d", g_chat_work.rc);
		if (g_chat_work.response[0] != '\0') {
			shell_print(sh, "%s", g_chat_work.response);
		}
	} else {
		shell_print(sh, "\nzbot: %s\n", g_chat_work.response);
	}

	return g_chat_work.rc;
}

/* ------------------------------------------------------------------ */
/* zbot history */
/* ------------------------------------------------------------------ */

static int cmd_history(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	memory_dump();
	return 0;
}

/* ------------------------------------------------------------------ */
/* zbot summary */
/* ------------------------------------------------------------------ */

static int cmd_summary(const struct shell *sh, size_t argc, char **argv)
{
	const char *s;

	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	s = memory_get_summary();
	shell_print(sh, "NVS Summary: %s", s ? s : "(none)");
	return 0;
}

/* ------------------------------------------------------------------ */
/* zbot clear */
/* ------------------------------------------------------------------ */

static int cmd_clear(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	memory_clear_history();
	shell_print(sh, "In-RAM history cleared. NVS summary preserved.");
	return 0;
}

/* ------------------------------------------------------------------ */
/* zbot wipe */
/* ------------------------------------------------------------------ */

static int cmd_wipe(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	memory_wipe_all();
	shell_print(sh, "All history and NVS summary wiped.");
	return 0;
}

/* ------------------------------------------------------------------ */
/* zbot skill list / run */
/* ------------------------------------------------------------------ */

static int cmd_skill_list(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);
	skill_list();
	return 0;
}

static int cmd_skill_run(const struct shell *sh, size_t argc, char **argv)
{
	static char result[SKILL_RESULT_MAX_LEN];
	const char *arg;
	int rc;

	if (argc < 2) {
		shell_print(sh, "Usage: zbot skill run <name> [arg]");
		return -EINVAL;
	}

	arg = argc >= 3 ? argv[2] : NULL;
	rc = skill_run(argv[1], arg, result, sizeof(result));
	shell_print(sh, "%s", result);
	return rc;
}

/* ------------------------------------------------------------------ */
/* zbot tools */
/* ------------------------------------------------------------------ */

static int cmd_tools(const struct shell *sh, size_t argc, char **argv)
{
	const struct tool_descriptor *tools;
	int count;

	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	tools = tools_get_all(&count);

	shell_print(sh, "=== zbot Tools (%d) ===", count);
	for (int i = 0; i < count; i++) {
		shell_print(sh, "  %-20s %s", tools[i].name, tools[i].description);
	}
	return 0;
}

/* ------------------------------------------------------------------ */
/* zbot wifi connect / disconnect / status                             */
/* ------------------------------------------------------------------ */

static int cmd_wifi_connect(const struct shell *sh, size_t argc, char **argv)
{
	const char *pass;
	int rc;

	if (argc < 2) {
		shell_print(sh, "Usage: zbot wifi connect <ssid> [passphrase]");
		shell_print(sh, "Credentials are saved to flash for auto-connect on next boot.");
		return -EINVAL;
	}

	pass = argc >= 3 ? argv[2] : "";
	rc = config_wifi_connect(argv[1], pass);
	if (rc == 0) {
		shell_print(sh, "WiFi connect request sent to SSID: %s", argv[1]);
		shell_print(sh, "Credentials saved — will auto-connect on reboot.");
	} else {
		shell_error(sh, "WiFi connect failed: %d", rc);
	}
	return rc;
}

static int cmd_wifi_disconnect(const struct shell *sh, size_t argc, char **argv)
{
	int rc;

	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	rc = config_wifi_disconnect();
	if (rc == 0) {
		shell_print(sh, "WiFi disconnect requested.");
	} else {
		shell_error(sh, "WiFi disconnect failed: %d", rc);
	}
	return rc;
}

static int cmd_wifi_status(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	if (wifi_credentials_is_empty()) {
		shell_print(sh, "No WiFi credentials saved.");
		shell_print(sh, "Use: zbot wifi connect <ssid> [pass]");
	} else {
		shell_print(sh, "WiFi credentials saved (use 'zbot status' for details).");
	}
	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_skill, SHELL_CMD(list, NULL, "List all skills", cmd_skill_list),
			       SHELL_CMD_ARG(run, NULL, "Run a skill: run <name> [arg]",
					     cmd_skill_run, 2, 1),
			       SHELL_SUBCMD_SET_END);

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_wifi,
	SHELL_CMD_ARG(connect, NULL, "Connect: wifi connect <ssid> [pass]", cmd_wifi_connect, 2, 1),
	SHELL_CMD(disconnect, NULL, "Disconnect from current WiFi", cmd_wifi_disconnect),
	SHELL_CMD(status, NULL, "Show saved WiFi SSID", cmd_wifi_status), SHELL_SUBCMD_SET_END);

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_zbot, SHELL_CMD_ARG(key, NULL, "Set API key: key <key>", cmd_key, 2, 0),
	SHELL_CMD(key_delete, NULL, "Delete API key from flash", cmd_key_delete),
	SHELL_CMD_ARG(host, NULL, "Set LLM host: host <hostname>", cmd_host, 2, 0),
	SHELL_CMD_ARG(path, NULL, "Set API path: path <path>", cmd_path, 2, 0),
	SHELL_CMD_ARG(model, NULL, "Set model: model <name>", cmd_model, 2, 0),
	SHELL_CMD_ARG(provider, NULL, "Set X-Model-Provider-Id: provider <id>", cmd_provider, 2, 0),
	SHELL_CMD_ARG(tls, NULL, "Configure TLS: tls <on|off> [port]", cmd_tls, 2, 1),
	SHELL_CMD(status, NULL, "Show current configuration", cmd_status),
	SHELL_CMD(wifi, &sub_wifi, "WiFi commands", NULL),
	SHELL_CMD_ARG(chat, NULL, "Chat with the agent: chat <message>", cmd_chat, 2, 32),
	SHELL_CMD(history, NULL, "Show conversation history", cmd_history),
	SHELL_CMD(summary, NULL, "Show NVS persisted summary", cmd_summary),
	SHELL_CMD(clear, NULL, "Clear in-RAM conversation history", cmd_clear),
	SHELL_CMD(wipe, NULL, "Wipe all history and NVS summary", cmd_wipe),
	SHELL_CMD(skill, &sub_skill, "Skill commands", NULL),
	SHELL_CMD(tools, NULL, "List available tools", cmd_tools), SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(zbot, &sub_zbot, "zbot AI Agent commands", NULL);
