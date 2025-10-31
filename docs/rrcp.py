#!/usr/bin/env python3

import sys
from antlr4 import *
from rrcpLexer import rrcpLexer
from rrcpParser import rrcpParser


def main(argv):
    input = FileStream(argv[1])
    lexer = rrcpLexer(input)
    stream = CommonTokenStream(lexer)
    parser = rrcpParser(stream)
    tree = parser.rrcp() // startRule

    print(tree.toStringTree(recog=parser))


if __name__ == "__main__":
    main(sys.argv)
