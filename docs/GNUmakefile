PYTHONPATH:=$(CURDIR):${PYTHONPATH}
export PYTHONPATH

ANTLR_HOME?=$(CURDIR)
CLASSPATH:="${ANTLR_HOME}/antlr-4.13.0-complete.jar:${CLASSPATH}"
export CLASSPATH

ANTLR4:=java -jar ${ANTLR_HOME}/antlr-4.13.2-complete.jar

.PHONY: all test format clean
all: rrcpParser.py rrcpLexer.py

rrcpParser.py rrcpLexer.py: rrcp.g4
	$(ANTLR4) -Dlanguage=Python3 $<

format: all
	black *.py

test: format
	pygrun --trace rrcp rrcp rrcp.txt
	pygrun --trace rrcp rrcp ../rrcp.txt

clean:
	$(RM) -r *.interp *.tokens *Parser.* *Lexer.* *Listener.* *.class *~ __pycache__
