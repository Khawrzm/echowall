"""EchoWallEvents — named event constants.

Use these instead of raw strings to avoid typos.
"""


class EchoWallEvents:
    PRESENCE = "presence"    # someone detected
    EMPTY = "empty"          # room just became empty
    INTRUSION = "intrusion"  # unexpected presence
    FALL = "fall"            # posture == fallen
    ANY = "any"              # every result tick
