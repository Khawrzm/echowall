"""
ECHOWALL — See through walls with the Wi-Fi you already own.
Open-source passive radar using Wi-Fi CSI + Acoustic fusion.

https://github.com/Khawrzm/echowall
"""

__version__ = "0.1.0"
__author__ = "ECHOWALL Contributors"
__license__ = "Apache-2.0"

from echowall.core.pipeline import EchowallPipeline
from echowall.models.echonet.model import EchoNet

__all__ = ["EchowallPipeline", "EchoNet"]
