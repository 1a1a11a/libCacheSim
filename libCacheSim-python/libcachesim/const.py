from __future__ import annotations

import enum


class TraceType(enum.Enum):
    CSV_TRACE = 0
    BIN_TRACE = 1
    PLAIN_TXT_TRACE = 2
    ORACLE_GENERAL_TRACE = 3
    LCS_TRACE = 4 # libCacheSim format
