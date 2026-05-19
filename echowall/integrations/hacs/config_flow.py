"""Config flow for EchoWall HACS integration — plug and play UI setup."""

from __future__ import annotations
import voluptuous as vol
from homeassistant import config_entries
from homeassistant.const import CONF_HOST, CONF_PORT

DOMAIN = "echowall"


class EchoWallConfigFlow(config_entries.ConfigFlow, domain=DOMAIN):
    """Handle EchoWall setup from the HA UI — no YAML needed."""

    VERSION = 1

    async def async_step_user(self, user_input=None):
        errors = {}
        if user_input is not None:
            return self.async_create_entry(
                title=f"EchoWall @ {user_input[CONF_HOST]}",
                data=user_input,
            )
        return self.async_show_form(
            step_id="user",
            data_schema=vol.Schema({
                vol.Required(CONF_HOST, default="192.168.1.50"): str,
                vol.Optional(CONF_PORT, default=8765): int,
            }),
            errors=errors,
            description_placeholders={
                "note": "Enter your EchoWall node IP. Find it with: echowall discover"
            },
        )
