
%{
# include  "config.tab.hpp"
# include  <string.h>
%}

%option noyywrap yylineno

%%

"#".* { ; }

  /* Keywords. */
"bus"    { return K_bus; }
"device" { return K_device; }
"env"    { return K_env; }
"exec"   { return K_exec; }
"host"   { return K_host; }
"name"   { return K_name; }
"pipe"   { return K_pipe; }
"port"   { return K_port; }
"process"  { return K_process; }
"protocol" { return K_protocol; }
"stderr" { return K_stderr; }
"stdin"  { return K_stdin; }
"stdout" { return K_stdout; }

  /* Skip white space */
[ \t\r\n] { ; }

  /* Integers are tokens. */
[0123456789]+ {
      configlval.integer = strtoul(yytext,0,0);
      configlloc.first_line = yylineno;
      return INTEGER;
  }

  /* Identifiers are words that are not keywords. */
[a-zA-Z_][a-zA-Z0-9_]* {
      configlval.text = strdup(yytext);
      configlloc.first_line = yylineno;
      return IDENTIFIER;
}

  /* strings are surrounded by quotes. */
\".*\" {
      configlval.text = strdup(yytext+1);
	/* Remove trailing quote. */
      configlval.text[strlen(yytext+1)-1] = 0;
      configlloc.first_line = yylineno;
      return STRING;
  }

  /* Single-character tokens */
. {   configlloc.first_line = yylineno;
      return yytext[0]; }

%%
