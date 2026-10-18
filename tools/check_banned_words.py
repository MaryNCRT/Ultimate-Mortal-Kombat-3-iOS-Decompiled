#!/usr/bin/env python3
"""Forwarder so CI (.github/workflows/banned-words.yml) and git hooks find check_banned_words.py."""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "decomp"))
import check_banned_words

if __name__ == "__main__":
    sys.exit(check_banned_words.main(sys.argv[1:]))
