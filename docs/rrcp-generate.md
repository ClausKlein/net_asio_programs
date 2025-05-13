# RRCP with ANTLR

To test the ANTLR grammar with Python, you'll need to follow several steps to set up your environment, generate the parser
and lexer, and then write a Python script to test the grammar. Below are the detailed steps to accomplish this:

## Step 1: Install ANTLR

### Download ANTLR:

Download the ANTLR jar file from the [ANTLR](https://www.antlr.org/download.html) website.

### Set Up ANTLR in Your Environment:

You can place the ANTLR jar file in a directory of your choice.
For example, let's say you place it in `~/antlr/antlr-4.13.0-complete.jar`.

Set Up Environment Variables (Optional but recommended)!

You can set an environment variable for ANTLR. Add the following lines to your `~/.bashrc` or `~/.bash_profile` (or
equivalent for your shell):

    export ANTLR_HOME=~/antlr
    export CLASSPATH="$ANTLR_HOME/antlr-4.13.0-complete.jar:$CLASSPATH"
    alias antlr4='java -jar $ANTLR_HOME/antlr-4.13.0-complete.jar'
    alias grun='java org.antlr.v4.gui.TestRig'

Reload your shell:

    source ~/.bashrc  # or source ~/.bash_profile

## Step 2: Install Python and Required Libraries

### Install Python.

Make sure you have Python 3 installed. You can check by running:

    python3 --version

### Install ANTLR4 Python Runtime.

Use pip to install the ANTLR4 runtime for Python:

    pip install antlr4-python3-runtime

## Step 3: Generate Lexer and Parser

### Create a File for Your Grammar.

Save your ANTLR grammar in a file named `rrcp.g4`.

### Generate the Lexer and Parser.

Run the following command in the terminal to generate the lexer and parser:

    antlr4 rrcp.g4 -Dlanguage=Python3

This will generate several Python files in the same directory, including `rrcpLexer.py`, `rrcpParser.py`, and others.

## Step 4: Write a Test Script

see [rrcp.py](rrcp.py)

and [GNUmakefile](GNUmakefile)
