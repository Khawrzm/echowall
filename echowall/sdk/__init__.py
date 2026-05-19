"""EchoWall SDK — the simplest possible developer interface.

Three lines to integrate EchoWall into anything:

    from echowall.sdk import EchoWallClient
    client = EchoWallClient()          # auto-discovers node on local network
    client.on_presence(lambda r: print("Someone home:", r.count))

No cloud. No API keys. No account. Runs on your LAN only.
"""

from echowall.sdk.client import EchoWallClient
from echowall.sdk.events import EchoWallEvents
from echowall.sdk.rules import RulesEngine
from echowall.sdk.node_discovery import discover_nodes

__all__ = [
    "EchoWallClient",
    "EchoWallEvents",
    "RulesEngine",
    "discover_nodes",
]
