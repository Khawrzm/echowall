"""LAN node discovery — finds EchoWall nodes on local network with zero config.

Strategy: subnet scan on port 8765 looking for /health endpoint.
No mDNS daemon required. No cloud lookup. Pure local network.

For faster discovery on known subnets, pass subnet explicitly:
    discover_nodes(subnet="192.168.1.")
"""

from __future__ import annotations

import logging
import socket
import urllib.request
import json
from concurrent.futures import ThreadPoolExecutor, as_completed
from typing import Optional

log = logging.getLogger("echowall.discovery")

_PORT = 8765
_HEALTH_PATH = "/health"
_TIMEOUT = 0.8


def _local_subnet() -> str:
    """Best-effort detect local subnet prefix (e.g. '192.168.1.')."""
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("10.255.255.255", 1))
        ip = s.getsockname()[0]
        s.close()
        return ".".join(ip.split(".")[:3]) + "."
    except Exception:
        return "192.168.1."


def _probe(ip: str) -> Optional[str]:
    try:
        url = f"http://{ip}:{_PORT}{_HEALTH_PATH}"
        with urllib.request.urlopen(url, timeout=_TIMEOUT) as resp:  # noqa: S310
            data = json.loads(resp.read())
            if data.get("status") == "ok":
                return ip
    except Exception:
        pass
    return None


def discover_nodes(
    subnet: Optional[str] = None,
    timeout: float = 5.0,
) -> list[str]:
    """Scan local subnet and return IPs of live EchoWall nodes.

    Parameters
    ----------
    subnet:  Subnet prefix like '192.168.1.'. Auto-detected if None.
    timeout: Max seconds to wait for discovery.

    Returns
    -------
    List of IP strings, e.g. ['192.168.1.42']
    """
    subnet = subnet or _local_subnet()
    candidates = [f"{subnet}{i}" for i in range(1, 255)]

    found = []
    with ThreadPoolExecutor(max_workers=64) as pool:
        futures = {pool.submit(_probe, ip): ip for ip in candidates}
        for future in as_completed(futures, timeout=timeout):
            result = future.result()
            if result:
                found.append(result)
                log.info("EchoWall node found: %s", result)

    return found
