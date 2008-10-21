
%{
# include  "config.tab.hpp"
%}

%option noyywrap

%%

"#".* { ; }

  /* Keywords. */
"bus"    { return K_bus; }
"device" { return K_device; }
"port"   { return K_port; }

  /* Skip white space */
" \t\r\n" { ; }

  /* Integers are tokens. */
[0123456789]+ { return INTEGER; }

  /* strings are surrounded by quotes. */
\".*\" { return STRING; }

  /* Single-character tokens */
. { return yytext[0]; }

%%
